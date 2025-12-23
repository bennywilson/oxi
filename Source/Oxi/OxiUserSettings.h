// OXI 2025

#pragma once

#include "GameFramework/GameUserSettings.h"
#include "OxiUserSettings.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnLightingQualityChanged, EOxiQualityLevelFlags);


/**
 *
 */
UENUM(meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EOxiQualityLevelFlags : uint32
{
	None = 0 UMETA(Hidden),
	Low = 1 << 0,
	Medium = 1 << 1,
	High = 1 << 2,
	Ultra = 1 << 3
};
ENUM_CLASS_FLAGS(EOxiQualityLevelFlags);

/**
 *
 */
UCLASS(config = GameUserSettings, defaultconfig, BlueprintType, Blueprintable, meta = (DisplayName = "Oxi User Settings"))
class OXI_API UOxiUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	FOnLightingQualityChanged OnLightingQualityChanged;

	virtual void SetToDefaults() override;

	UFUNCTION(BlueprintCallable, Category = Settings)
	void SetGraphicsQualityLevel(const int32 QualityLevel, const bool force);

	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Graphics")
	EOxiQualityLevelFlags GraphicsQualityLevel = EOxiQualityLevelFlags::None;

	static UOxiUserSettings* GetOxiUserSettings();
};

/**
 *
 */
UCLASS(ClassGroup = (Lighting), meta = (BlueprintSpawnableComponent), HideCategories = (Activation, Collision, Cooking, AssetUserData, Tags, ComponentReplication, Rendering, LOD, Physics, Mobility, Replication, Events, Input))
class UOxiLightQualityComponent : public UActorComponent
{
	GENERATED_BODY()

protected:
	void OnRegister() override;

#if WITH_EDITOR
	void PostEditChangeProperty(FPropertyChangedEvent& Event);
#endif

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Bitmask, BitmaskEnum = "/Script/Oxi.EOxiQualityLevelFlags"))
	int32 LightVisibilityMask;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Bitmask, BitmaskEnum = "/Script/Oxi.EOxiQualityLevelFlags"))
	int32 CastShadowMask;

private:
	void UpdateLight(const EOxiQualityLevelFlags NewQualitySetting);
};

inline int32 ToInt32(EOxiQualityLevelFlags QualityLevel)
{
	const int32 QualityMask = (int32)QualityLevel;
	ensureMsgf(QualityMask && !(QualityMask & (QualityMask - 1)), TEXT("ToInt32(EOxiQualityLevelFlags) called with invalid bitmask: %d. Expected exactly one bit set."), QualityMask);
	return FMath::FloorLog2(QualityMask);

}