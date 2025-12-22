// OXI 2025

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "OxiEditorSubsystem.generated.h"

UCLASS()
class UOxiEditorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
};