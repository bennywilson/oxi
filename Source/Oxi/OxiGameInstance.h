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
};