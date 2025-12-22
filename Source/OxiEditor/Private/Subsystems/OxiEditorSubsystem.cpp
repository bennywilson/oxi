// OXI 2025

#include "Subsystems/OxiEditorSubsystem.h"
#include "OxiUserSettings.h"
#include "Engine/Engine.h"

void UOxiEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

#if WITH_EDITOR
	FEditorDelegates::OnEditorInitialized.AddLambda([this](double TimeToInitializeEditor)
		{
			if (UOxiUserSettings* Settings = UOxiUserSettings::GetOxiUserSettings())
			{
				const int32 QualityLevelAsInt = FMath::FloorLog2((int32)Settings->GraphicsQualityLevel);
				Settings->SetGraphicsQualityLevel(QualityLevelAsInt, true);
			}
		});
#endif
}

void UOxiEditorSubsystem::Deinitialize()
{
	Super::Deinitialize();
}