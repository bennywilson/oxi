// OXI 2025

#include "GameFramework/GameUserSettings.h"
#include "OxiUserSettings.generated.h"

UCLASS(config = GameUserSettings, defaultconfig, BlueprintType, Blueprintable, meta = (DisplayName = "Oxi User Settings"))

class UOxiUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

	UFUNCTION(BlueprintCallable, Category = Settings)
	void SetQualityLevel(const int QualityLevel);
};