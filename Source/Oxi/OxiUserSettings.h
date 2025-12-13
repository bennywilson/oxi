// OXI 2025

#include "GameFramework/GameUserSettings.h"
#include "OxiUserSettings.generated.h"

UCLASS(config = GameUserSettings, defaultconfig, BlueprintType, Blueprintable, meta = (DisplayName = "Oxi User Settings"))

class UOxiUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	virtual void SetToDefaults() override;

	UFUNCTION(BlueprintCallable, Category = Settings)
	void SetGraphicsQualityLevel(const int32 QualityLevel);

	// Your custom quality level property
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Graphics")
	int32 GraphicsQualityLevel = INDEX_NONE;

};