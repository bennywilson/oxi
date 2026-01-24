// ELP 2020

#pragma once 

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Blueprint/UserWidget.h"
#include "OxiHUD.generated.h"

/**
 *
 */
UCLASS()
class AOxiHUD : public AHUD
{
	GENERATED_BODY()

public:
	AOxiHUD();

	virtual void DrawHUD() override;

};

/**
 *
 */
UCLASS()
class UOxiCrosshairsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
	void UpdateSpread(const float Spread);
};
