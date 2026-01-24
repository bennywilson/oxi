// OXI 2025

#include "OxiUserSettings.h"
#include "Oxi.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Components/LightComponentBase.h"

/**
 *
 */
UOxiUserSettings* UOxiUserSettings::GetOxiUserSettings()
{
	if (!GEngine)
	{
		return nullptr;
	}

	return Cast<UOxiUserSettings>(GEngine->GetGameUserSettings());
}

/**
 *
 */
static FAutoConsoleCommand SetGraphicsQualityLevelCmd(
	TEXT("Oxi.SetGraphicsQuality"),
	TEXT("Sets Graphics Quality Level - 0: Low, 1: Medium, 2: High, 3: Epic"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (Args.Num() >= 1)
			{
				const int32 QualityLevel = FCString::Atoi(*Args[0]);
				UOxiUserSettings* const Settings = UOxiUserSettings::GetOxiUserSettings();
				if (Settings)
				{
					Settings->SetGraphicsQualityLevel(QualityLevel, false);
				}
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("Usage: MyGame.SetSetting <Key> <Value>"));
			}
		})
);

/**
 *
 */
void UOxiUserSettings::SetToDefaults()
{
	Super::SetToDefaults();

	if (GraphicsQualityLevel == EOxiQualityLevelFlags::None)
	{
		UE_LOG(LogTemp, Log, TEXT("GraphicsQualityLevel not set, defaulting to Medium"));

		GraphicsQualityLevel = EOxiQualityLevelFlags::Medium;
		SaveConfig();
	}
}

/**
 *
 */
void UOxiUserSettings::SetGraphicsQualityLevel(const int32 QualityLevel, const bool Force)
{
	UE_LOG(LogOxi, Log, TEXT("%s"), *FString::Printf(TEXT("UOxiGameUserSettings::SetQualityLevel(%d)"), QualityLevel));

	if (!Force)
	{
		EOxiQualityLevelFlags TargetQuality = EOxiQualityLevelFlags::Medium;
		switch (QualityLevel)
		{
			case 0: TargetQuality = EOxiQualityLevelFlags::Low; break;
			case 1: TargetQuality = EOxiQualityLevelFlags::Medium; break;
			case 2: TargetQuality = EOxiQualityLevelFlags::High; break;
			case 3: TargetQuality = EOxiQualityLevelFlags::Ultra; break;
		}

		if (TargetQuality == GraphicsQualityLevel)
		{
			UE_LOG(LogTemp, Log, TEXT("%s"), *FString::Printf(TEXT("UOxiGameUserSettings::SetQualityLevel(%d)"), QualityLevel));
			return;
		}
	}

	Scalability::FQualityLevels Quality;
	Quality.SetFromSingleQualityLevel(QualityLevel);
	Scalability::SetQualityLevels(Quality, true);

	IConsoleManager& ConsoleManager = IConsoleManager::Get();
	IConsoleVariable* const CVarDynamicGIMethod = ConsoleManager.FindConsoleVariable(TEXT("r.DynamicGlobalIlluminationMethod"));
	IConsoleVariable* const CVarAAMethod = ConsoleManager.FindConsoleVariable(TEXT("r.AntialiasingMethod"));
	IConsoleVariable* const CVarLumenDiffuseIndirect = ConsoleManager.FindConsoleVariable(TEXT("r.lumen.DiffuseIndirect.Allow"));
	IConsoleVariable* const CVarReflectionMethod = ConsoleManager.FindConsoleVariable(TEXT("r.reflectionmethod"));
	IConsoleVariable* const CVarMaxRoughnessToTrace = ConsoleManager.FindConsoleVariable(TEXT("r.Lumen.Reflections.MaxRoughnessToTrace"));

	switch(QualityLevel)
	{
		case 0:
		{
			CVarDynamicGIMethod->Set(0, ECVF_SetByConsole);
			CVarLumenDiffuseIndirect->Set(0, ECVF_SetByConsole);
			CVarReflectionMethod->Set(0, ECVF_SetByConsole);
			CVarAAMethod->Set(0, ECVF_SetByConsole);
			CVarMaxRoughnessToTrace->Set(-1.f, ECVF_SetByConsole);
			break;
		}

		case 1:
		{
			CVarDynamicGIMethod->Set(0, ECVF_SetByConsole);
			CVarLumenDiffuseIndirect->Set(0, ECVF_SetByConsole);
			CVarReflectionMethod->Set(0, ECVF_SetByConsole);
			CVarAAMethod->Set(1, ECVF_SetByConsole);
			CVarMaxRoughnessToTrace->Set(-1.f, ECVF_SetByConsole);
			break;
		}

		case 2: 
		{
			CVarDynamicGIMethod->Set(1, ECVF_SetByConsole);
			CVarLumenDiffuseIndirect->Set(1, ECVF_SetByConsole);
			CVarReflectionMethod->Set(1, ECVF_SetByConsole);
			CVarAAMethod->Set(2, ECVF_SetByConsole);
			CVarMaxRoughnessToTrace->Set(0.4f, ECVF_SetByConsole);
			break;
		}

		case 3:
		{
			CVarDynamicGIMethod->Set(1, ECVF_SetByConsole);
			CVarLumenDiffuseIndirect->Set(1, ECVF_SetByConsole);
			CVarReflectionMethod->Set(1, ECVF_SetByConsole);
			CVarAAMethod->Set(2, ECVF_SetByConsole);
			CVarMaxRoughnessToTrace->Set(1.0f, ECVF_SetByConsole);
			break;
		}
	}

	GraphicsQualityLevel = EOxiQualityLevelFlags::Medium;
	switch(QualityLevel)
	{
		case 0: GraphicsQualityLevel = EOxiQualityLevelFlags::Low; break;
		case 1: GraphicsQualityLevel = EOxiQualityLevelFlags::Medium; break;
		case 2: GraphicsQualityLevel = EOxiQualityLevelFlags::High; break;
		case 3: GraphicsQualityLevel = EOxiQualityLevelFlags::Ultra; break;
	}

	OnLightingQualityChanged.Broadcast(GraphicsQualityLevel);

	SaveConfig();
}

/**
 *
 */
void UOxiLightQualityComponent::OnRegister()
{
	Super::OnRegister();

	UOxiUserSettings *const Settings = UOxiUserSettings::GetOxiUserSettings();
	if (Settings)
	{	
		UpdateLight(Settings->GraphicsQualityLevel);

		Settings->OnLightingQualityChanged.AddUObject(
			this, &UOxiLightQualityComponent::UpdateLight);
	
	}
}

/**
 *
 */
#if WITH_EDITOR
void UOxiLightQualityComponent::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
	Super::PostEditChangeProperty(Event);

	if (UOxiUserSettings* Settings = UOxiUserSettings::GetOxiUserSettings())
	{
		UpdateLight(Settings->GraphicsQualityLevel);
	}
}
#endif

/**
 *
 */
void UOxiLightQualityComponent::UpdateLight(const EOxiQualityLevelFlags NewQualitySetting)
{
	if (!GetOwner() || GetOwner()->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		return;
	}

	const uint32 NewSetting = (uint32)NewQualitySetting;
	auto LightComp = GetOwner()->FindComponentByClass<ULightComponentBase>();
	if (!LightComp)
	{
		return;
	}

	LightComp->SetVisibility(NewSetting & LightVisibilityMask);
	LightComp->SetCastShadows(NewSetting & CastShadowMask);
}