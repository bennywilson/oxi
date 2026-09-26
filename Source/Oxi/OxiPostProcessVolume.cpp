// Fill out your copyright notice in the Description page of Project Settings.


#include "OxiPostProcessVolume.h"
#include "Engine/World.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"

void AOxiPostProcessVolume::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		FakeLightMPC = LoadObject<UMaterialParameterCollection>(nullptr, TEXT("/Game/Oxi/Core/MPCs/MPC_FakeLight"));
		if (!FakeLightMPC)
		{
			UE_LOG(LogTemp, Warning, TEXT("AOxiPostProcessVolume: failed to load MPC_FakeLight; fake light disabled."));
			return;
		}
		UpdateFakeLight(true);
	}
}

void AOxiPostProcessVolume::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (this->HasActorBegunPlay() && !HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		UpdateFakeLight(false);
	}
}

void AOxiPostProcessVolume::UpdateFakeLight(bool bForce)
{
	UWorld* World = GetWorld();
	if (!FakeLightMPC || !World)
	{
		return;
	}

	if (!bForce && FakeLightColor == LastPushedColor && FakeLightDirection.Equals(LastPushedDirection))
	{
		return;
	}

	// Write to the runtime instance, not the MPC asset, so the asset is never modified.
	UMaterialParameterCollectionInstance* Instance = World->GetParameterCollectionInstance(FakeLightMPC);
	if (!Instance)
	{
		return;
	}

	Instance->SetVectorParameterValue(TEXT("FakeLight_Color"), FLinearColor(FakeLightColor));
	Instance->SetVectorParameterValue(TEXT("FakeLight_Direction"),
		FLinearColor(FakeLightDirection.X, FakeLightDirection.Y, FakeLightDirection.Z, 0.f));

	LastPushedColor = FakeLightColor;
	LastPushedDirection = FakeLightDirection;
}
