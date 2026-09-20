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

private:
	UPROPERTY(Transient)
	TObjectPtr<UOxiOutlinePalette> Palette;

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
};
