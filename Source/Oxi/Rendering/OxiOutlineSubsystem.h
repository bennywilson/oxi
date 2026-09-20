// OXI 2026

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
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

	/** Starts a dash trail from FromLocation to the actor's current position, fading out over Duration. */
	bool StartSmear(AActor* Actor, const FVector& FromLocation, float Duration);

	/** Ends a dash trail early and puts the actor's outline back. */
	void StopSmear(AActor* Actor);

	/** Stencil values this system hands out to actors while they smear. Palette styles must avoid them. */
	static constexpr int32 FirstSmearStencil = 248;
	static constexpr int32 NumSmearSlots = 8;

	FLinearColor GetFocusColor() const { return FocusColor; }
	float GetFocusEmissiveIntensity() const { return FocusEmissiveIntensity; }
	float GetFocusAmount() const { return FocusAmount; }

private:
	/** An actor's outline while it is smearing: it borrows a reserved stencil value that carries the trail. */
	struct FActiveSmear
	{
		TWeakObjectPtr<AActor> Actor;
		TArray<TWeakObjectPtr<UPrimitiveComponent>> Components;
		TArray<int32> OriginalStencils;
		int32 SmearStencil = 0;
		int32 BaseStencil = 0;
		FVector Start = FVector::ZeroVector;
		float Duration = 0.f;
		float TimeLeft = 0.f;
	};

	bool Tick(float DeltaSeconds);
	void PushSmears();
	const struct FOxiOutlineStyle* FindStyleByStencil(int32 StencilValue) const;

	TArray<FActiveSmear> ActiveSmears;
	FTSTicker::FDelegateHandle TickHandle;

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

	/**
	 * Draws a trail of the actor's outline ink from FromLocation to where it is now, fading out over Duration.
	 * Call it right after a dash teleports the actor, passing the position it dashed from. The actor keeps its
	 * usual outline; the trail is extra ink behind it, the way stretching the old hull looked.
	 */
	UFUNCTION(BlueprintCallable, Category = "Oxi|Outlines")
	static bool StartOutlineSmear(AActor* Actor, FVector FromLocation, float Duration = 0.15f);

	UFUNCTION(BlueprintCallable, Category = "Oxi|Outlines")
	static void StopOutlineSmear(AActor* Actor);
};
