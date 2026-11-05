// OXI 2025

#include "OxiUserSettings.h"
#include "Oxi.h"
#include "Components/LightComponentBase.h"
#include "Containers/Ticker.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "Serialization/CustomVersion.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "UObject/LinkerLoad.h"
#include "UObject/ObjectSaveContext.h"

namespace
{
	/** Asset versions for UOxiLightQualityComponent. */
	struct FOxiLightQualityVersion
	{
		enum Type
		{
			BeforeCustomVersionWasAdded = 0,
			// LightVisibilityMask/CastShadowMask replaced by Role/MinShadowQuality.
			RoleAndMinShadowQuality,

			LatestVersion = RoleAndMinShadowQuality
		};

		static inline const FGuid GUID = FGuid(0x6B1E2F4A, 0x93C04D17, 0xA5E82B61, 0x0F7D3C98);
	};

	FCustomVersionRegistration GRegisterOxiLightQualityVersion(FOxiLightQualityVersion::GUID, FOxiLightQualityVersion::LatestVersion, TEXT("OxiLightQualityVersion"));

	// Nothing else references the mix, so make sure it gets cooked (see DirectoriesToAlwaysCook).
	const TSoftObjectPtr<USoundMix> UserVolumeMix(FSoftObjectPath(TEXT("/Game/Oxi/Sounds/SCM_UserVolume.SCM_UserVolume")));
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

	// SC_Master is left out of the mix and multiplied into each child instead, so the result doesn't
	// depend on how sound class volumes inherit. Sounds should use a child class, not SC_Master itself.
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

	// Pushing an already active mix just bumps its ref count, which is harmless since it's never popped.
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
void UOxiLightQualityComponent::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FOxiLightQualityVersion::GUID);

#if WITH_EDITORONLY_DATA
	// Only convert when loading an old asset from disk. PIE copies and undo have no linker.
	const bool bLoadingOldAsset = Ar.IsLoading() && Ar.GetLinker()
		&& GetLinkerCustomVersion(FOxiLightQualityVersion::GUID) < FOxiLightQualityVersion::RoleAndMinShadowQuality;

	if (bLoadingOldAsset)
	{
		// A mask the asset didn't save had the old C++ default of 0. When the archetype is a Blueprint
		// template rather than the C++ defaults, Role and MinShadowQuality were already copied from it.
		const int32 UnsavedMask = GetArchetype() == GetClass()->GetDefaultObject() ? 0 : -1;
		LightVisibilityMask_DEPRECATED = UnsavedMask;
		CastShadowMask_DEPRECATED = UnsavedMask;
	}
#endif

	Super::Serialize(Ar);

#if WITH_EDITORONLY_DATA
	if (bLoadingOldAsset)
	{
		ConvertDeprecatedMasks();
	}
#endif
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

#if WITH_EDITORONLY_DATA
/**
 *
 */
void UOxiLightQualityComponent::ConvertDeprecatedMasks()
{
	constexpr int32 AllLevels = 0b1111;
	constexpr int32 LowAndMedium = 0b0011;

	if (LightVisibilityMask_DEPRECATED >= 0)
	{
		const int32 Mask = LightVisibilityMask_DEPRECATED & AllLevels;
		Role = Mask == LowAndMedium ? EOxiLightRole::GIFill : EOxiLightRole::Always;

		if (Mask != LowAndMedium && Mask != AllLevels)
		{
			UE_LOG(LogOxi, Warning, TEXT("%s: old visibility mask 0x%X has no exact match, converted to Always."), *GetPathName(), Mask);
		}
	}

	if (CastShadowMask_DEPRECATED >= 0)
	{
		const int32 Mask = CastShadowMask_DEPRECATED & AllLevels;
		MinShadowQuality = Mask ? (EOxiQualityLevel)FMath::CountTrailingZeros((uint32)Mask) : EOxiQualityLevel::Never;

		// The new setting can only express "this level and up".
		const int32 Expressible = Mask ? AllLevels & ~((1 << (int32)MinShadowQuality) - 1) : 0;
		if (Mask != Expressible)
		{
			UE_LOG(LogOxi, Warning, TEXT("%s: old shadow mask 0x%X has gaps, converted to shadows from level %d up."), *GetPathName(), Mask, (int32)MinShadowQuality);
		}
	}

	LightVisibilityMask_DEPRECATED = -1;
	CastShadowMask_DEPRECATED = -1;
}
#endif
