// Fill out your copyright notice in the Description page of Project Settings.


#include "OxiPostProcessVolume.h"
#include "Materials/MaterialParameterCollection.h"

void AOxiPostProcessVolume::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		FakeLightMPC = LoadObject<UMaterialParameterCollection>(nullptr, TEXT("/Game/Oxi/Core/MPCs/FakeLight_MPC"));
		SetFakeLightParam("FakeLight_Color", FakeLightColor);
		SetFakeLightParam("FakeLight_Direction", FakeLightColor);
	}
}

void AOxiPostProcessVolume::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (this->HasActorBegunPlay() && !HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		SetFakeLightParam("FakeLight_Color", FakeLightColor);
		SetFakeLightParam("FakeLight_Direction", FakeLightColor);
	}
}

void AOxiPostProcessVolume::SetFakeLightParam(const FName ParamName, const FLinearColor& NewColor)
{
	const FGuid LightColorGUID = FakeLightMPC->GetParameterId(ParamName);
	int OutIndex = 0;
	int OutCompIndex = 0;
	FakeLightMPC->GetParameterIndex(LightColorGUID, OutIndex, OutCompIndex);
	if (OutIndex < 0 || OutIndex > FakeLightMPC->VectorParameters.Num())
	{
		return;
	}

	FakeLightMPC->VectorParameters[OutIndex].DefaultValue = FakeLightColor;
}