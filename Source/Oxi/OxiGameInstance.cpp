#include "OxiGameInstance.h"
#include "OxiUserSettings.h"

void UOxiGameInstance::Init()
{
	Super::Init();

	if (UOxiUserSettings* Settings = Cast<UOxiUserSettings>(GEngine->GetGameUserSettings()))
	{
		const int32 QualityLevelAsInt = FMath::FloorLog2((int32)Settings->GraphicsQualityLevel);
		Settings->SetGraphicsQualityLevel(QualityLevelAsInt, true);
	}
}