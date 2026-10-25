// OXI 2026

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "OxiOutlineSettings.generated.h"

class UOxiOutlinePalette;

/** Project Settings > Game > Outlines */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Outlines"))
class OXI_API UOxiOutlineSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Styles available to outlined meshes, keyed by custom stencil value. */
	UPROPERTY(Config, EditAnywhere, Category = "Outlines")
	TSoftObjectPtr<UOxiOutlinePalette> Palette;

	/** View depth (cm) up to which lines use their full Width. */
	UPROPERTY(Config, EditAnywhere, Category = "Width", meta = (ClampMin = 0, ForceUnits = "cm"))
	float NearDistance = 500.f;

	/** View depth (cm) at which lines have thinned to MinWidth. */
	UPROPERTY(Config, EditAnywhere, Category = "Width", meta = (ClampMin = 0, ForceUnits = "cm"))
	float FarDistance = 4000.f;

	/** Style widths are in pixels at this vertical resolution and scale with the actual render height. */
	UPROPERTY(Config, EditAnywhere, Category = "Width", meta = (ClampMin = 1))
	float ReferenceHeight = 1080.f;

	/** Cap on any style's width (px at ReferenceHeight). The search radius, and so the cost, scales with this. */
	UPROPERTY(Config, EditAnywhere, Category = "Width", meta = (ClampMin = 0, ClampMax = 16))
	float MaxWidth = 8.f;

	/** Minimum depth gap (cm) for a nearer part of an object to draw an interior line over a farther part. */
	UPROPERTY(Config, EditAnywhere, Category = "Interior Lines", AdvancedDisplay, meta = (ClampMin = 0, ForceUnits = "cm"))
	float SelfOverlapDepth = 3.f;

	/** Extra depth gap, as a fraction of depth per pixel of distance. Stops lines appearing on surfaces seen at grazing angles. */
	UPROPERTY(Config, EditAnywhere, Category = "Interior Lines", AdvancedDisplay, meta = (ClampMin = 0))
	float SelfOverlapDepthPerPixel = 0.01f;

	/** Slack (cm) when testing whether an outlined surface is hidden behind other geometry. */
	UPROPERTY(Config, EditAnywhere, Category = "Occlusion", AdvancedDisplay, meta = (ClampMin = 0, ForceUnits = "cm"))
	float OcclusionBias = 1.f;

	/** Additional occlusion slack as a fraction of depth. */
	UPROPERTY(Config, EditAnywhere, Category = "Occlusion", AdvancedDisplay, meta = (ClampMin = 0))
	float OcclusionBiasPerDepth = 0.002f;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
