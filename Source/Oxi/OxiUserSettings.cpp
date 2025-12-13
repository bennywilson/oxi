// OXI 2025

#include "OxiUserSettings.h"
#include "Kismet/KismetSystemLibrary.h"

void UOxiUserSettings::SetQualityLevel(const int QualityLevel)
{
	UE_LOG(LogTemp, Log, TEXT("UOxiGameUserSettings::SetQualityLevel()"));
	
	IConsoleManager& ConsoleManager = IConsoleManager::Get();
	IConsoleVariable* const CVarDynamicGIMethod = ConsoleManager.FindConsoleVariable(TEXT("r.DynamicGlobalIlluminationMethod"));
	IConsoleVariable* const CVarAAMethod = ConsoleManager.FindConsoleVariable(TEXT("r.AntialiasingMethod"));
	IConsoleVariable* const CVarLumenDiffuseIndirect = ConsoleManager.FindConsoleVariable(TEXT("r.lumen.DiffuseIndirect.Allow"));

	UKismetSystemLibrary::ExecuteConsoleCommand(GetWorld(), FString::Printf(TEXT("scalability %d"), QualityLevel), nullptr);

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
}
