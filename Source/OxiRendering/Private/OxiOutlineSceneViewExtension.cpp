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

// Mirrors FOxiSmearInstance in OxiOutline.usf.
struct FOxiSmearInstanceGPU
{
	FVector4f StartEnd = FVector4f::Zero();
	FVector4f Params = FVector4f::Zero();
	FVector4f Depths = FVector4f::Zero();
	FVector4f Bounds = FVector4f::Zero();
	FUintVector4 Stencil = FUintVector4(0, 0, 0, 0);
};

// These are read as structured buffers, so a field added on one side and not the other shifts every element
// and quietly corrupts the whole palette. Update the matching struct in OxiOutline.usf when these change.
static_assert(sizeof(FOxiOutlineStyleGPU) == 5 * sizeof(FVector4f), "FOxiOutlineStyleGPU must match FOxiOutlineStyle in OxiOutline.usf");
static_assert(sizeof(FOxiSmearInstanceGPU) == 5 * sizeof(FVector4f), "FOxiSmearInstanceGPU must match FOxiSmearInstance in OxiOutline.usf");

// World position to pixels in this view's render-resolution rect. False when it is behind the camera.
static bool ProjectToViewPixels(const FSceneView& View, const FIntRect& ViewRect, const FVector& WorldPosition, FVector2f& OutPixel, float& OutDepth)
{
	const FVector4 ScreenPosition = View.WorldToScreen(WorldPosition);
	FVector2D PixelPosition;
	if (ScreenPosition.W <= 0.0 || !View.ScreenToPixel(ScreenPosition, PixelPosition))
	{
		return false;
	}
	OutDepth = static_cast<float>(ScreenPosition.W);

	// ScreenToPixel works in unscaled viewport pixels, but the pass runs at render resolution.
	const FIntRect Unscaled = View.UnscaledViewRect;
	const FVector2f Scale(
		ViewRect.Width() / static_cast<float>(FMath::Max(Unscaled.Width(), 1)),
		ViewRect.Height() / static_cast<float>(FMath::Max(Unscaled.Height(), 1)));

	OutPixel = (FVector2f(PixelPosition) - FVector2f(Unscaled.Min.X, Unscaled.Min.Y)) * Scale + FVector2f(ViewRect.Min.X, ViewRect.Min.Y);
	return true;
}

class FOxiOutlinePS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FOxiOutlinePS);
	SHADER_USE_PARAMETER_STRUCT(FOxiOutlinePS, FGlobalShader);

	class FSixteenDirections : SHADER_PERMUTATION_BOOL("OUTLINE_16_DIRECTIONS");
	class FSmear : SHADER_PERMUTATION_BOOL("OUTLINE_SMEAR");
	using FPermutationDomain = TShaderPermutationDomain<FSixteenDirections, FSmear>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTextures)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputSceneColor)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FOxiOutlineStyle>, Styles)
		SHADER_PARAMETER(FIntVector4, ViewRect)
		SHADER_PARAMETER(FVector2f, DistanceFalloff)
		SHADER_PARAMETER(FVector2f, SelfOverlap)
		SHADER_PARAMETER(FVector2f, OcclusionBias)
		SHADER_PARAMETER(FVector4f, FocusColor)
		SHADER_PARAMETER(float, FocusAmount)
		SHADER_PARAMETER(float, WidthScale)
		SHADER_PARAMETER(float, MaxWidth)
		SHADER_PARAMETER(float, SearchRadius)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FOxiSmearInstance>, Smears)
		SHADER_PARAMETER(int32, NumSmears)
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

void FOxiOutlineSceneViewExtension::SetFocus_GameThread(const FLinearColor& InColor, float InEmissiveIntensity, float InAmount)
{
	check(IsInGameThread());

	const FVector4f Color(InColor.R, InColor.G, InColor.B, InEmissiveIntensity);
	const float Amount = FMath::Clamp(InAmount, 0.f, 1.f);

	// Usually driven by an aim blend, so skip the render command when nothing moved.
	if (Color.Equals(FocusColor_GameThread) && FMath::IsNearlyEqual(Amount, FocusAmount_GameThread))
	{
		return;
	}
	FocusColor_GameThread = Color;
	FocusAmount_GameThread = Amount;

	ENQUEUE_RENDER_COMMAND(OxiSetOutlineFocus)(
		[this, KeepAlive = AsShared(), Color, Amount](FRHICommandListImmediate&)
		{
			FocusColor_RenderThread = Color;
			FocusAmount_RenderThread = Amount;
		});
}

void FOxiOutlineSceneViewExtension::SetSmears_GameThread(TArray<FOxiOutlineSmear> InSmears)
{
	check(IsInGameThread());

	ENQUEUE_RENDER_COMMAND(OxiSetOutlineSmears)(
		[this, KeepAlive = AsShared(), Smears = MoveTemp(InSmears)](FRHICommandListImmediate&) mutable
		{
			Smears_RenderThread = MoveTemp(Smears);
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

	// Project each dash trail into this view: the object is at End now, so the trail runs back toward Start.
	TArray<FOxiSmearInstanceGPU, SceneRenderingAllocator> SmearInstances;
	SmearInstances.Reserve(Smears_RenderThread.Num());

	for (const FOxiOutlineSmear& Smear : Smears_RenderThread)
	{
		FVector2f StartPixel, EndPixel;
		float StartDepth = 0.f, EndDepth = 0.f;
		if (Smear.Strength <= 0.f || !ProjectToViewPixels(View, ViewRect, Smear.End, EndPixel, EndDepth))
		{
			continue;
		}

		// A long trail can start behind the camera, where there is no projection. Pull it in toward the object
		// until it lands on screen, so an over-long trail is shortened rather than dropped.
		bool bStartProjected = false;
		for (float Reach = 1.f; !bStartProjected && Reach > 0.03f; Reach *= 0.5f)
		{
			bStartProjected = ProjectToViewPixels(View, ViewRect, FMath::Lerp(Smear.End, Smear.Start, Reach), StartPixel, StartDepth);
		}

		if (!bStartProjected)
		{
			continue;
		}

		const FVector2f Trail = EndPixel - StartPixel;
		float TrailLength = Trail.Size();
		if (TrailLength < 1.f)
		{
			continue;
		}

		// A long dash would otherwise paint ink right across the screen.
		const float MaxLength = Smear.MaxLength * WidthScale;
		if (TrailLength > MaxLength)
		{
			const float Fraction = MaxLength / TrailLength;
			StartPixel = EndPixel - Trail * Fraction;
			StartDepth = FMath::Lerp(EndDepth, StartDepth, Fraction);
			TrailLength = MaxLength;
		}

		// How wide the object is on screen decides how wide its trail is.
		const float ProjectionScale = View.ViewMatrices.GetProjectionMatrix().M[0][0] * 0.5f * ViewRect.Width();
		const float RadiusPixels = FMath::Max(Smear.Radius * ProjectionScale / FMath::Max(EndDepth, 1.f), 1.f);
		const FVector2f BoundsMin = FVector2f::Min(StartPixel, EndPixel) - RadiusPixels;
		const FVector2f BoundsMax = FVector2f::Max(StartPixel, EndPixel) + RadiusPixels;

		FOxiSmearInstanceGPU& Instance = SmearInstances.AddDefaulted_GetRef();
		Instance.StartEnd = FVector4f(StartPixel.X, StartPixel.Y, EndPixel.X, EndPixel.Y);
		Instance.Params = FVector4f(FMath::Max(Smear.Falloff, 0.01f), FMath::Clamp(Smear.Opacity, 0.f, 1.f), RadiusPixels, FMath::Clamp(Smear.Strength, 0.f, 1.f));
		// Spread the taps evenly over the trail so the silhouette's copies overlap into one smear instead of
		// reading as separate ghosts. An object narrower than that spacing falls between taps, so it uses the
		// swept band instead - which is what a bullet casing wants anyway.
		const float TapStep = FMath::Max(TrailLength / 31.f, 1.f);
		const bool bSilhouette = RadiusPixels >= 6.f && TapStep <= RadiusPixels * 0.5f;

		Instance.Depths = FVector4f(StartDepth, EndDepth, FMath::Clamp(Smear.Taper, 0.f, 1.f), bSilhouette ? TapStep : 0.f);
		Instance.Bounds = FVector4f(BoundsMin.X, BoundsMin.Y, BoundsMax.X, BoundsMax.Y);
		Instance.Stencil = FUintVector4(Smear.Stencil, 0, 0, 0);
	}

	const bool bSmear = !SmearInstances.IsEmpty();
	FRDGBufferRef SmearBuffer = CreateStructuredBuffer(GraphBuilder, TEXT("OxiOutline.Smears"), SmearInstances);

	// Detect lines. Written to a separate target because the pass reads scene color around each pixel.
	{
		FOxiOutlinePS::FPermutationDomain Permutation;
		Permutation.Set<FOxiOutlinePS::FSixteenDirections>(CVarOxiOutlinesDirections.GetValueOnRenderThread() >= 16);
		Permutation.Set<FOxiOutlinePS::FSmear>(bSmear);
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
		Parameters->FocusColor = FocusColor_RenderThread;
		Parameters->FocusAmount = FocusAmount_RenderThread;
		Parameters->WidthScale = WidthScale;
		Parameters->MaxWidth = MaxWidth;
		Parameters->SearchRadius = SearchRadius;
		Parameters->Smears = GraphBuilder.CreateSRV(SmearBuffer);
		Parameters->NumSmears = SmearInstances.Num();
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
