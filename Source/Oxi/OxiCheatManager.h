// ELP 2023

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "OxiCheatManager.generated.h"

/**
 * 
 */
UCLASS()
class OXI_API UOxiCheatManager : public UCheatManager
{
	GENERATED_BODY()
	
	UFUNCTION(exec, BlueprintCallable, Category = "Oxi | Cheat Manager")
	virtual void WarpToCheckPoint(FString CheckPointName);

	UFUNCTION(exec, BlueprintCallable, Category = "Oxi | Cheat Manager")
	virtual void SetPlayerHealth(const float NewHealth);
};
