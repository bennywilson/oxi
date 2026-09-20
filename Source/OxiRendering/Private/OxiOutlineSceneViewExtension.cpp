// OXI 2026

#include "OxiOutlineSceneViewExtension.h"

#include "DataDrivenShaderPlatformInfo.h"
#include "FXRenderingUtils.h"
#include "GlobalShader.h"
#include "PixelShaderUtils.h"
#include "PostProcess/PostProcessInputs.h"
#include "RenderGraphUtils.h"
#include "SceneTexturesConfig.h"
#include "SceneView.h"
#include "ScreenPass.h"
#include "ShaderParameterStruct.h"

static TAutoConsoleVariable<int32> CVarOxiOutlines(
	TEXT("r.Oxi.Outlines"),
	1,
	TEXT("Post-process outlines. 0: off, 1: on"),
	ECVF_RenderThreadSafe | ECVF_Scalability);

static TAutoConsoleVariable<int32> CVarOxiOutlinesDirections(
	TEXT("r.Oxi.Outlines.Directions"),
	8,
	TEXT("Search directions per pixel. 8: cheaper, slightly octagonal corners. 16: rounder corners."),
	ECVF_RenderThreadSafe | ECVF_Scalability);

DECLARE_GPU_STAT_NAMED(OxiOutlines, TEXT("Oxi Outlines"));

class FOxiOutlinePS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FOxiOutlinePS);
	SHADER_USE_PARAMETER_STRUCT(FOxiOutlinePS, FGlobalShader);

	class FSixteenDirections : SHADER_PERMUTATION_BOOL("OUTLINE_16_DIRECTIONS");
	using FPermutationDomain = TShaderPermutationDomain<FSixteenDirections>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTextures)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputSceneColor)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FOxiOutlineStyle>, Styles)
		SHADER_PARAMETER(FIntVector4, ViewRect)
		SHADER_PARAMETER(FVector2f, DistanceFalloff)
		SHADER_PARAMETER(FVector2f, SelfOverlap)
		SHADER_PARAMETER(FVector2f, OcclusionBias)
		SHADER_PARAMETER(float, WidthScale)
		SHADER_PARAMETER(float, MaxWidth)
		SHADER_PARAMETER(float, SearchRadius)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};
IMPLEMENT_GLOBAL_SHADER(FOxiOutlinePS, "/OxiShaders/Private/OxiOutline.usf", "OutlinePS", SF_Pixel);

class FOxiOutlineCompositePS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FOxiOutlineCompositePS);
	SHADER_USE_PARAMETER_STRUCT(FOxiOutlineCompositePS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, OutlineTexture)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};
IMPLEMENT_GLOBAL_SHADER(FOxiOutlineCompositePS, "/OxiShaders/Private/OxiOutline.usf", "CompositePS", SF_Pixel);

FOxiOutlineSceneViewExtension::FOxiOutlineSceneViewExtension(const FAutoRegister& AutoRegister)
	: FSceneViewExtensionBase(AutoRegister)
{
}

void FOxiOutlineSceneViewExtension::SetPalette_GameThread(TArray<FOxiOutlineStyleGPU> InStyles, const FOxiOutlineGlobals& InGlobals)
{
	check(IsInGameThread());

	float MaxStyleWidth = 0.f;
	bool bAnyEnabled = false;
	for (const FOxiOutlineStyleGPU& Style : InStyles)
	{
		if (Style.Flags.X & EOxiOutlineFlags::Enabled)
		{
			bAnyEnabled = true;
			MaxStyleWidth = FMath::Max3(MaxStyleWidth, Style.Width.X, Style.Width.Y);
		}
	}

	bHasStyles = bAnyEnabled;

	ENQUEUE_RENDER_COMMAND(OxiSetOutlinePalette)(
		[this, KeepAlive = AsShared(), Styles = MoveTemp(InStyles), InGlobals, MaxStyleWidth](FRHICommandListImmediate&) mutable
		{
			Styles_RenderThread = MoveTemp(Styles);
			Globals_RenderThread = InGlobals;
			MaxStyleWidth_RenderThread = MaxStyleWidth;
		});
}

bool FOxiOutlineSceneViewExtension::IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const
{
	return bHasStyles && CVarOxiOutlines.GetValueOnGameThread() != 0;
}

void FOxiOutlineSceneViewExtension::PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs)
{
	if (Styles_RenderThread.IsEmpty()
		|| CVarOxiOutlines.GetValueOnRenderThread() == 0
		|| !View.Family->EngineShowFlags.PostProcessing
		|| View.GetFeatureLevel() < ERHIFeatureLevel::SM5)
	{
		return;
	}

	Inputs.Validate();

	const FIntRect ViewRect = UE::FXRenderingUtils::GetRawViewRectUnsafe(View);
	FRDGTextureRef SceneColor = (*Inputs.SceneTextures)->SceneColorTexture;
	if (!SceneColor || ViewRect.IsEmpty())
	{
		return;
	}

	const FOxiOutlineGlobals& Globals = Globals_RenderThread;
	const float WidthScale = ViewRect.Height() / FMath::Max(Globals.ReferenceHeight, 1.f);
	const float MaxWidth = FMath::Max(Globals.MaxWidth, 0.f);
	// A pixel can be covered by a line up to (width + 1) px from the object (see coverage in OxiOutline.usf).
	const float SearchRadius = FMath::Min(MaxStyleWidth_RenderThread, MaxWidth) * WidthScale + 1.f;

	RDG_EVENT_SCOPE_STAT(GraphBuilder, OxiOutlines, "OxiOutlines %dx%d", ViewRect.Width(), ViewRect.Height());
	RDG_GPU_STAT_SCOPE(GraphBuilder, OxiOutlines);

	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(View.GetFeatureLevel());

	FRDGTextureRef OutlineTexture = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(SceneColor->Desc.Extent, PF_FloatRGBA, FClearValueBinding::Transparent, TexCreate_ShaderResource | TexCreate_RenderTargetable),
		TEXT("OxiOutline"));

	FRDGBufferRef StyleBuffer = CreateStructuredBuffer(GraphBuilder, TEXT("OxiOutline.Styles"), Styles_RenderThread);

	// Detect lines. Written to a separate target because the pass reads scene color around each pixel.
	{
		FOxiOutlinePS::FPermutationDomain Permutation;
		Permutation.Set<FOxiOutlinePS::FSixteenDirections>(CVarOxiOutlinesDirections.GetValueOnRenderThread() >= 16);
		TShaderMapRef<FOxiOutlinePS> PixelShader(ShaderMap, Permutation);

		FOxiOutlinePS::FParameters* Parameters = GraphBuilder.AllocParameters<FOxiOutlinePS::FParameters>();
		Parameters->View = View.ViewUniformBuffer;
		Parameters->SceneTextures = Inputs.SceneTextures;
		Parameters->InputSceneColor = SceneColor;
		Parameters->Styles = GraphBuilder.CreateSRV(StyleBuffer);
		Parameters->ViewRect = FIntVector4(ViewRect.Min.X, ViewRect.Min.Y, ViewRect.Max.X, ViewRect.Max.Y);
		Parameters->DistanceFalloff = FVector2f(Globals.NearDistance, 1.f / FMath::Max(Globals.FarDistance - Globals.NearDistance, 1.f));
		Parameters->SelfOverlap = FVector2f(Globals.SelfOverlapDepth, Globals.SelfOverlapDepthPerPixel);
		Parameters->OcclusionBias = FVector2f(Globals.OcclusionBias, Globals.OcclusionBiasPerDepth);
		Parameters->WidthScale = WidthScale;
		Parameters->MaxWidth = MaxWidth;
		Parameters->SearchRadius = SearchRadius;
		Parameters->RenderTargets[0] = FRenderTargetBinding(OutlineTexture, ERenderTargetLoadAction::ENoAction);

		// Fullscreen-triangle VS: its only output is SV_Position, matching the pixel shaders' input signature.
		FPixelShaderUtils::AddFullscreenPass(GraphBuilder, ShaderMap, RDG_EVENT_NAME("OxiOutline Detect"), PixelShader, Parameters, ViewRect);
	}

	// Composite premultiplied lines over scene color, leaving scene color alpha untouched.
	{
		TShaderMapRef<FOxiOutlineCompositePS> PixelShader(ShaderMap);

		FOxiOutlineCompositePS::FParameters* Parameters = GraphBuilder.AllocParameters<FOxiOutlineCompositePS::FParameters>();
		Parameters->OutlineTexture = OutlineTexture;
		Parameters->RenderTargets[0] = FRenderTargetBinding(SceneColor, ERenderTargetLoadAction::ELoad);

		FPixelShaderUtils::AddFullscreenPass(
			GraphBuilder,
			ShaderMap,
			RDG_EVENT_NAME("OxiOutline Composite"),
			PixelShader,
			Parameters,
			ViewRect,
			TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_InverseSourceAlpha>::GetRHI());
	}
}
