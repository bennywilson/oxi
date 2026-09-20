// OXI 2026

#pragma once

#include "CoreMinimal.h"
#include "SceneViewExtension.h"

#include <atomic>

// Mirrors FOxiOutlineStyle in OxiOutline.usf. The palette is indexed directly by custom stencil value.
struct FOxiOutlineStyleGPU
{
	FVector4f ColorDim = FVector4f::Zero();			// rgb line color, a dim amount
	FVector4f EmissiveFloor = FVector4f::Zero();	// rgb emissive (color * intensity), a minimum brightness when dimmed
	FVector4f Width = FVector4f::Zero();			// x width (px @ reference height), y width at far distance, z interior line scale, w opacity
	FVector4f Misc = FVector4f::Zero();				// x focus influence, yzw unused
	FUintVector4 Flags = FUintVector4(0, 0, 0, 0);	// x EOxiOutlineFlags
};

namespace EOxiOutlineFlags
{
	enum : uint32
	{
		Enabled			= 1 << 0,
		ThroughWalls	= 1 << 1,
		InteriorLines	= 1 << 2,
	};
}

struct FOxiOutlineGlobals
{
	// Width eases from Width to MinWidth between these view depths (cm).
	float NearDistance = 500.f;
	float FarDistance = 4000.f;

	// Widths are authored in pixels at this vertical resolution.
	float ReferenceHeight = 1080.f;

	// Hard cap on authored width (px @ reference height). Bounds the search radius, so it bounds the cost.
	float MaxWidth = 8.f;

	// Depth gap needed before a nearer part of an object draws a line over a farther part (self-overlap / touching styles).
	// Threshold = SelfOverlapDepth + SelfOverlapDepthPerPixel * Depth * PixelDistance.
	float SelfOverlapDepth = 3.f;
	float SelfOverlapDepthPerPixel = 0.01f;

	// Slack when comparing custom depth against scene depth to decide whether an outlined surface is visible.
	float OcclusionBias = 1.f;
	float OcclusionBiasPerDepth = 0.002f;
};

/**
 * Post-process outlines driven by custom depth + custom stencil.
 * Runs before TSR/DOF/motion blur/bloom, so lines get anti-aliased with the scene, emissive lines bloom,
 * and after-DOF translucency (most particles) is composited over them.
 */
class OXIRENDERING_API FOxiOutlineSceneViewExtension : public FSceneViewExtensionBase
{
public:
	FOxiOutlineSceneViewExtension(const FAutoRegister& AutoRegister);

	/** Game thread. Styles are indexed by stencil value (0 is always "no outline"). */
	void SetPalette_GameThread(TArray<FOxiOutlineStyleGPU> InStyles, const FOxiOutlineGlobals& InGlobals);

	/**
	 * Game thread. Tints styles that opt in (focus influence > 0) toward a color, for things like
	 * weapon-specific highlights while aiming. Amount is usually the aim blend, so it can be driven per frame.
	 */
	void SetFocus_GameThread(const FLinearColor& InColor, float InEmissiveIntensity, float InAmount);

	virtual void PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs) override;

protected:
	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override;

private:
	std::atomic<bool> bHasStyles = false;

	FVector4f FocusColor_GameThread = FVector4f::Zero();
	float FocusAmount_GameThread = 0.f;

	TArray<FOxiOutlineStyleGPU> Styles_RenderThread;
	FOxiOutlineGlobals Globals_RenderThread;
	float MaxStyleWidth_RenderThread = 0.f;

	FVector4f FocusColor_RenderThread = FVector4f::Zero();	// rgb focus color, a emissive intensity
	float FocusAmount_RenderThread = 0.f;
};
