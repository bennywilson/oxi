// OXI 2025

#include "OxiUserSettings.h"
#include "Kismet/KismetSystemLibrary.h"

static FAutoConsoleCommand SetGraphicsQualityLevelCmd(
	TEXT("Oxi.SetGraphicsQuality"),
	TEXT("Sets Graphics Quality Level - 0: Low, 1: Medium, 2: High, 3: Epic"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (Args.Num() >= 1)
			{
				const int32 QualityLevel = FCString::Atoi(*Args[0]);
				if (UOxiUserSettings* Settings = GetMutableDefault<UOxiUserSettings>())
				{
					Settings->SetGraphicsQualityLevel(QualityLevel);
				}
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("Usage: MyGame.SetSetting <Key> <Value>"));
			}
		})
);

void UOxiUserSettings::SetToDefaults()
{
	Super::SetToDefaults();

	if (GraphicsQualityLevel == INDEX_NONE)
	{
		UE_LOG(LogTemp, Log, TEXT("GraphicsQualityLevel not set, defaulting to 1"));

		GraphicsQualityLevel = 1; // Default to Medium
		SaveConfig();
	}
}

void UOxiUserSettings::SetGraphicsQualityLevel(const int32 QualityLevel)
{
	const int ActualQualityLevel = FMath::Clamp(QualityLevel, 0, 4);
	UE_LOG(LogTemp, Log, TEXT("%s"), *FString::Printf(TEXT("UOxiGameUserSettings::SetQualityLevel(%d)"), ActualQualityLevel));
	
	IConsoleManager& ConsoleManager = IConsoleManager::Get();
	IConsoleVariable* const CVarDynamicGIMethod = ConsoleManager.FindConsoleVariable(TEXT("r.DynamicGlobalIlluminationMethod"));
	IConsoleVariable* const CVarAAMethod = ConsoleManager.FindConsoleVariable(TEXT("r.AntialiasingMethod"));
	IConsoleVariable* const CVarLumenDiffuseIndirect = ConsoleManager.FindConsoleVariable(TEXT("r.lumen.DiffuseIndirect.Allow"));

	UKismetSystemLibrary::ExecuteConsoleCommand(GetWorld(), FString::Printf(TEXT("scalability %d"), ActualQualityLevel), nullptr);

	switch(QualityLevel)
	{
		case 0:
		{
			CVarDynamicGIMethod->Set(0);
			CVarLumenDiffuseIndirect->Set(0);
			CVarAAMethod->Set(0);
			break;
		}

		case 1:
		{
			CVarDynamicGIMethod->Set(0);
			CVarLumenDiffuseIndirect->Set(0);
			CVarAAMethod->Set(1);
			break;
		}

		case 2: 
		{
			CVarDynamicGIMethod->Set(1);
			CVarLumenDiffuseIndirect->Set(1);
			CVarAAMethod->Set(2);
			break;
		}

		case 3:
		{
			CVarDynamicGIMethod->Set(1);
			CVarLumenDiffuseIndirect->Set(1);
			CVarAAMethod->Set(2);
			break;
		}
	}

	SaveConfig();
}
