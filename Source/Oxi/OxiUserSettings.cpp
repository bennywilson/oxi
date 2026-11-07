// OXI 2025

#include "OxiUserSettings.h"
#include "Oxi.h"
#include "Components/LightComponentBase.h"
#include "Containers/Ticker.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "UObject/ObjectSaveContext.h"

namespace
{
	// No asset references SCM_UserVolume, so it lives in /Game/Oxi/AlwaysCook, which DefaultGame.ini always cooks.
	const TSoftObjectPtr<USoundMix> UserVolumeMix(FSoftObjectPath(TEXT("/Game/Oxi/AlwaysCook/SCM_UserVolume.SCM_UserVolume")));
	const TSoftObjectPtr<USoundClass> SFXClass(FSoftObjectPath(TEXT("/Game/Oxi/Sounds/SC_SFX.SC_SFX")));
	const TSoftObjectPtr<USoundClass> VOClass(FSoftObjectPath(TEXT("/Game/Oxi/Sounds/SC_VO.SC_VO")));
	const TSoftObjectPtr<USoundClass> MusicClass(FSoftObjectPath(TEXT("/Game/Oxi/Sounds/SC_Music.SC_Music")));

	/** True when Lumen is the GI method and DefaultScalability.ini allows its diffuse GI at the current level. */
	bool IsLumenGIOn()
	{
		IConsoleManager& ConsoleManager = IConsoleManager::Get();
		const IConsoleVariable* const GIMethod = ConsoleManager.FindConsoleVariable(TEXT("r.DynamicGlobalIlluminationMethod"));
		const IConsoleVariable* const LumenAllow = ConsoleManager.FindConsoleVariable(TEXT("r.Lumen.DiffuseIndirect.Allow"));
		return GIMethod && GIMethod->GetInt() == 1 && LumenAllow && LumenAllow->GetInt() != 0;
	}
}

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
void UOxiUserSettings::LoadSettings(bool bForceReload)
{
	Super::LoadSettings(bForceReload);

	// The editor keeps its own scalability, so only the game gets a first-run default.
	// UGameEngine::Init calls ApplyNonResolutionSettings next, which applies ScalabilityQuality.
	if (!GIsEditor && !GConfig->DoesSectionExist(TEXT("ScalabilityGroups"), GGameUserSettingsIni))
	{
		UE_LOG(LogOxi, Log, TEXT("No saved scalability, defaulting to level %d"), DefaultScalabilityLevel);
		ScalabilityQuality.SetFromSingleQualityLevel(DefaultScalabilityLevel);
	}
}

/**
 *
 */
void UOxiUserSettings::SetToDefaults()
{
	Super::SetToDefaults();

	MasterVolume = 1.f;
	SFXVolume = 1.f;
	VOVolume = 1.f;
	MusicVolume = 1.f;
}

/**
 *
 */
void UOxiUserSettings::ApplyAudioSettings(const UObject* WorldContextObject) const
{
	USoundMix* const Mix = UserVolumeMix.LoadSynchronous();
	if (!Mix)
	{
		UE_LOG(LogOxi, Warning, TEXT("ApplyAudioSettings: %s not found, volume settings ignored."), *UserVolumeMix.ToString());
		return;
	}

	// The mix leaves out SC_Master and multiplies the master volume into each child instead, so the result
	// doesn't depend on how sound class volumes inherit. A sound assigned to SC_Master itself ignores these settings.
	const auto Override = [&](const TSoftObjectPtr<USoundClass>& Class, const float Volume)
	{
		if (USoundClass* const LoadedClass = Class.LoadSynchronous())
		{
			UGameplayStatics::SetSoundMixClassOverride(WorldContextObject, Mix, LoadedClass, MasterVolume * Volume, 1.f, 0.f, false);
		}
		else
		{
			UE_LOG(LogOxi, Warning, TEXT("ApplyAudioSettings: %s not found."), *Class.ToString());
		}
	};

	Override(SFXClass, SFXVolume);
	Override(VOClass, VOVolume);
	Override(MusicClass, MusicVolume);

	// Pushing an active mix again only increments its ref count. Nothing pops the mix, so the count doesn't matter.
	UGameplayStatics::PushSoundMixModifier(WorldContextObject, Mix);
}

/**
 *
 */
void UOxiUserSettings::SetGraphicsQualityLevel(const int32 QualityLevel, const bool Force)
{
	SetOverallScalabilityLevel(QualityLevel);
	ApplySettings(false);
}

/**
 *
 */
void UOxiLightQualityComponent::OnRegister()
{
	Super::OnRegister();

	UpdateLight(IsLumenGIOn(), Scalability::GetQualityLevels().ShadowQuality);

	Scalability::OnScalabilitySettingsChanged.AddUObject(this, &UOxiLightQualityComponent::OnScalabilityChanged);
}

/**
 *
 */
void UOxiLightQualityComponent::OnUnregister()
{
	Scalability::OnScalabilitySettingsChanged.RemoveAll(this);

	Super::OnUnregister();
}

/**
 *
 */
void UOxiLightQualityComponent::PreSave(FObjectPreSaveContext SaveContext)
{
	Super::PreSave(SaveContext);

	// UpdateLight writes the light's saved bVisible/bCastShadows. Save the Epic state so the saved
	// values don't depend on the editor's current scalability level. OnRegister reapplies on load.
	UpdateLight(true, 3);

	// Put the current settings back once the save is done.
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float)
		{
			UpdateLight(IsLumenGIOn(), Scalability::GetQualityLevels().ShadowQuality);
			return false;
		}));
}

/**
 *
 */
#if WITH_EDITOR
void UOxiLightQualityComponent::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
	Super::PostEditChangeProperty(Event);

	UpdateLight(IsLumenGIOn(), Scalability::GetQualityLevels().ShadowQuality);
}
#endif

/**
 *
 */
void UOxiLightQualityComponent::OnScalabilityChanged(const Scalability::FQualityLevels& QualityLevels)
{
	// SetQualityLevels broadcasts after applying the ini cvars, so IsLumenGIOn is already up to date.
	UpdateLight(IsLumenGIOn(), QualityLevels.ShadowQuality);
}

/**
 *
 */
void UOxiLightQualityComponent::UpdateLight(const bool bLumenGIOn, const int32 ShadowQuality)
{
	if (!GetOwner() || GetOwner()->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		return;
	}

	ULightComponentBase* const LightComp = GetOwner()->FindComponentByClass<ULightComponentBase>();
	if (!LightComp)
	{
		return;
	}

	// Cinematic (4) counts as Epic.
	const bool bCastShadows = MinShadowQuality != EOxiQualityLevel::Never
		&& FMath::Min(ShadowQuality, (int32)EOxiQualityLevel::Epic) >= (int32)MinShadowQuality;

	LightComp->SetVisibility(Role == EOxiLightRole::Always || !bLumenGIOn);
	LightComp->SetCastShadows(bCastShadows);
}
