// OXI 2026

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Subsystems/EngineSubsystem.h"
#include "OxiOutlineSubsystem.generated.h"

class FOxiOutlineSceneViewExtension;
class UOxiOutlinePalette;
class UPrimitiveComponent;

/** Owns the outline scene view extension and feeds it the palette from UOxiOutlineSettings. */
UCLASS()
class OXI_API UOxiOutlineSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	static UOxiOutlineSubsystem* Get();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Reloads the palette/settings and pushes them to the renderer. */
	void RefreshPalette();

	/** Stencil value for a named style, or INDEX_NONE if the palette has no such style. */
	int32 FindStencilValue(FName StyleName) const;

	/** Tints every style with a FocusInfluence toward Color. Amount is typically the aim blend. */
	void SetFocus(const FLinearColor& Color, float EmissiveIntensity, float Amount);

	FLinearColor GetFocusColor() const { return FocusColor; }
	float GetFocusEmissiveIntensity() const { return FocusEmissiveIntensity; }
	float GetFocusAmount() const { return FocusAmount; }

private:
	UPROPERTY(Transient)
	TObjectPtr<UOxiOutlinePalette> Palette;

	FLinearColor FocusColor = FLinearColor::White;
	float FocusEmissiveIntensity = 0.f;
	float FocusAmount = 0.f;

	TSharedPtr<FOxiOutlineSceneViewExtension, ESPMode::ThreadSafe> Extension;
};

UCLASS()
class OXI_API UOxiOutlineLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Outlines a component with a named palette style. Returns false if the style doesn't exist. */
	UFUNCTION(BlueprintCallable, Category = "Oxi|Outlines")
	static bool SetOutlineStyle(UPrimitiveComponent* Component, FName StyleName);

	/** Outlines every primitive component on an actor with a named palette style. */
	UFUNCTION(BlueprintCallable, Category = "Oxi|Outlines")
	static bool SetActorOutlineStyle(AActor* Actor, FName StyleName);

	UFUNCTION(BlueprintCallable, Category = "Oxi|Outlines")
	static void ClearOutline(UPrimitiveComponent* Component);

	UFUNCTION(BlueprintCallable, Category = "Oxi|Outlines")
	static void ClearActorOutline(AActor* Actor);

	/**
	 * Tints every outline style that has a FocusInfluence toward Color, globally. Meant for the aim-down-sights
	 * highlight: each weapon passes its own color, and Amount is the aim blend, so the tint eases in with the sight.
	 * EmissiveIntensity above 0 also makes the highlight glow and bloom.
	 */
	UFUNCTION(BlueprintCallable, Category = "Oxi|Outlines")
	static void SetOutlineFocus(FLinearColor Color, float Amount, float EmissiveIntensity = 0.f);

	/** Changes how far the focus tint is applied, keeping the current color. Cheap enough to call every frame. */
	UFUNCTION(BlueprintCallable, Category = "Oxi|Outlines")
	static void SetOutlineFocusAmount(float Amount);
};
