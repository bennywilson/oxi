// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/PostProcessVolume.h"
#include "OxiPostProcessVolume.generated.h"

/**
 * 
 */
UCLASS()
class OXI_API AOxiPostProcessVolume : public APostProcessVolume
{
	GENERATED_BODY()

protected:
	void BeginPlay() override;

	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fake Light")
	FVector FakeLightDirection;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fake Light")
	FColor FakeLightColor;

	UPROPERTY(Transient)
	UMaterialParameterCollection* FakeLightMPC = nullptr;

private:
	// Pushes the current color/direction to the world's MPC instance.
	// Skips the write when nothing changed since the last push (unless bForce).
	void UpdateFakeLight(bool bForce);

	FColor LastPushedColor = FColor::Black;
	FVector LastPushedDirection = FVector::ZeroVector;
};
