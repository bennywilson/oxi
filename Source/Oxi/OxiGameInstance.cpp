// OXI 2025

#include "OxiGameInstance.h"
#include "Engine/World.h"
#include "OxiUserSettings.h"

/**
 *
 */
void UOxiGameInstance::Init()
{
	Super::Init();

	WorldInitializedActorsHandle = FWorldDelegates::OnWorldInitializedActors.AddUObject(this, &UOxiGameInstance::OnWorldInitializedActors);
}

/**
 *
 */
void UOxiGameInstance::Shutdown()
{
	FWorldDelegates::OnWorldInitializedActors.Remove(WorldInitializedActorsHandle);

	Super::Shutdown();
}

/**
 *
 */
void UOxiGameInstance::OnWorldInitializedActors(const FActorsInitializedParams& Params)
{
	// The delegate is global, so skip other PIE instances' worlds.
	if (!Params.World || Params.World->GetGameInstance() != this)
	{
		return;
	}

	if (const UOxiUserSettings* const Settings = UOxiUserSettings::GetOxiUserSettings())
	{
		Settings->ApplyAudioSettings(Params.World);
	}
}
