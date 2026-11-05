// OXI 2025

#pragma once

#include "Engine/GameInstance.h"
#include "OxiGameInstance.generated.h"

UCLASS()
class UOxiGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	virtual void Shutdown() override;

private:
	/** Applies the saved volumes to each new world, before any actor's BeginPlay. */
	void OnWorldInitializedActors(const FActorsInitializedParams& Params);

	FDelegateHandle WorldInitializedActorsHandle;
};
