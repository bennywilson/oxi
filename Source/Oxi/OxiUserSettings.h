// OXI 2025

#pragma once

#include "GameFramework/GameUserSettings.h"
#include "Scalability.h"
#include "OxiUserSettings.generated.h"

/** What a UOxiLightQualityComponent's light is for. */
UENUM()
enum class EOxiLightRole : uint8
{
	/** Always shown. */
	Always,
	/** Stands in for Lumen GI, so it's only shown while Lumen GI is off. */
	GIFill UMETA(DisplayName = "GI Fill"),
};

/** A scalability level (sg.* value), plus Never. Cinematic counts as Epic. */
UENUM()
enum class EOxiQualityLevel : uint8
{
	Low,
	Medium,
	High,
	Epic,
	Never,
};

/**
 * Graphics quality is Unreal's scalability system: the sg.* groups, whose per-level cvars live in
 * Config/DefaultScalability.ini. Use UGameUserSettings::SetOverallScalabilityLevel + ApplySettings
 * to change it, and the editor viewport's Scalability menu or the `scalability N` command to test it.
 */
UCLASS(config = GameUserSettings, BlueprintType, Blueprintable, meta = (DisplayName = "Oxi User Settings"))
class OXI_API UOxiUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	/** The overall scalability level a first-time player starts on. */
	static constexpr int32 DefaultScalabilityLevel = 2;

	virtual void LoadSettings(bool bForceReload = false) override;
	virtual void SetToDefaults() override;

	/** Kept for the node in Entry.umap's level Blueprint. Force is unused, because ApplySettings always applies. */
	UFUNCTION(BlueprintCallable, Category = Settings, meta = (DeprecatedFunction, DeprecationMessage = "Use Set Overall Scalability Level followed by Apply Settings."))
	void SetGraphicsQualityLevel(const int32 QualityLevel, const bool Force);

	UFUNCTION(BlueprintPure, Category = Settings, meta = (DisplayName = "Get Oxi User Settings"))
	static UOxiUserSettings* GetOxiUserSettings();

	// Volumes range from 0 to 1. The setters only store the value. ApplyAudioSettings makes it audible, and SaveSettings keeps it.

	UFUNCTION(BlueprintCallable, Category = "Settings|Audio")
	void SetMasterVolume(const float Volume) { MasterVolume = FMath::Clamp(Volume, 0.f, 1.f); }

	UFUNCTION(BlueprintPure, Category = "Settings|Audio")
	float GetMasterVolume() const { return MasterVolume; }

	UFUNCTION(BlueprintCallable, Category = "Settings|Audio")
	void SetSFXVolume(const float Volume) { SFXVolume = FMath::Clamp(Volume, 0.f, 1.f); }

	UFUNCTION(BlueprintPure, Category = "Settings|Audio")
	float GetSFXVolume() const { return SFXVolume; }

	UFUNCTION(BlueprintCallable, Category = "Settings|Audio")
	void SetVOVolume(const float Volume) { VOVolume = FMath::Clamp(Volume, 0.f, 1.f); }

	UFUNCTION(BlueprintPure, Category = "Settings|Audio")
	float GetVOVolume() const { return VOVolume; }

	UFUNCTION(BlueprintCallable, Category = "Settings|Audio")
	void SetMusicVolume(const float Volume) { MusicVolume = FMath::Clamp(Volume, 0.f, 1.f); }

	UFUNCTION(BlueprintPure, Category = "Settings|Audio")
	float GetMusicVolume() const { return MusicVolume; }

	/**
	 * Pushes SCM_UserVolume with SC_SFX/SC_VO/SC_Music overridden to Master x their own volume.
	 * UOxiGameInstance calls this for every game world, so menus only need it when a slider moves.
	 */
	UFUNCTION(BlueprintCallable, Category = "Settings|Audio", meta = (WorldContext = "WorldContextObject"))
	void ApplyAudioSettings(const UObject* WorldContextObject) const;

protected:
	UPROPERTY(config)
	float MasterVolume = 1.f;

	UPROPERTY(config)
	float SFXVolume = 1.f;

	UPROPERTY(config)
	float VOVolume = 1.f;

	UPROPERTY(config)
	float MusicVolume = 1.f;
};

/**
 * Shows or hides the owner's light, and turns its shadows on or off, to suit the scalability settings.
 * GI Fill lights follow whether Lumen GI is on, so DefaultScalability.ini stays the only place that decides it.
 * Shadows follow sg.ShadowQuality.
 */
UCLASS(ClassGroup = (Lighting), meta = (BlueprintSpawnableComponent), HideCategories = (Activation, Collision, Cooking, AssetUserData, Tags, ComponentReplication, Rendering, LOD, Physics, Mobility, Replication, Events, Input))
class UOxiLightQualityComponent : public UActorComponent
{
	GENERATED_BODY()

protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;

	virtual void PreSave(FObjectPreSaveContext SaveContext) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
#endif

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Light Quality")
	EOxiLightRole Role = EOxiLightRole::Always;

	/** Lowest sg.ShadowQuality at which the light casts shadows. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Light Quality")
	EOxiQualityLevel MinShadowQuality = EOxiQualityLevel::Low;

private:
	void OnScalabilityChanged(const Scalability::FQualityLevels& QualityLevels);
	void UpdateLight(const bool bLumenGIOn, const int32 ShadowQuality);
};
