// OXI 2026

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "OxiOutlinePalette.generated.h"

/** One outline look. Meshes opt in by rendering custom depth with CustomDepthStencilValue = StencilValue. */
USTRUCT(BlueprintType)
struct FOxiOutlineStyle
{
	GENERATED_BODY()

	/** Name used to apply this style from code/Blueprint (UOxiOutlineLibrary). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outline")
	FName Name;

	/** Custom stencil value that selects this style. 0 is reserved for "no outline". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outline", meta = (ClampMin = 1, ClampMax = 255))
	int32 StencilValue = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Color")
	FLinearColor Color = FLinearColor::Black;

	/** 1 = solid line. 0 = no line color at all, only the emissive glow is added. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Color", meta = (ClampMin = 0, ClampMax = 1))
	float Opacity = 1.f;

	/** How much the line darkens with the lighting on the object it outlines. Has no visible effect on black lines. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Color", meta = (ClampMin = 0, ClampMax = 1))
	float DimAmount = 1.f;

	/** Dimming never goes below this, so silhouettes stay readable in the dark. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Color", meta = (ClampMin = 0, ClampMax = 1))
	float MinBrightness = 0.15f;

	/** Added on top of the line in HDR, ignoring lighting, so it blooms like an emissive material. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Emissive")
	FLinearColor EmissiveColor = FLinearColor::Black;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Emissive", meta = (ClampMin = 0))
	float EmissiveIntensity = 0.f;

	/**
	 * How far this style tints toward the global focus color (see SetOutlineFocus), where the weapon being
	 * aimed sets the color. 0 = never tints. Enemy styles want 1; world props usually want 0.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Focus", meta = (ClampMin = 0, ClampMax = 1))
	float FocusInfluence = 0.f;

	/** Line width in pixels at 1080p for objects up close. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Width", meta = (ClampMin = 0))
	float Width = 3.f;

	/** Line width in pixels at 1080p at the far falloff distance (see Project Settings > Outlines). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Width", meta = (ClampMin = 0))
	float MinWidth = 1.f;

	/** Draw lines where the object overlaps itself (arm over torso) and where it meets another outline style at similar depth. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Width")
	bool bInteriorLines = true;

	/** Interior lines are thinner than the silhouette, like an inker's contour vs detail weight. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Width", meta = (ClampMin = 0, ClampMax = 1, EditCondition = "bInteriorLines"))
	float InteriorWidthScale = 0.6f;

	/** Draw the silhouette even where the object is hidden behind other geometry. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visibility")
	bool bShowThroughWalls = false;
};

UCLASS(BlueprintType)
class OXI_API UOxiOutlinePalette : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outlines", meta = (TitleProperty = "Name"))
	TArray<FOxiOutlineStyle> Styles;

	const FOxiOutlineStyle* FindStyle(FName StyleName) const;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
