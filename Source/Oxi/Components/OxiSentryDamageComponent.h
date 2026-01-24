// ELP 2020

#pragma once

#include "CoreMinimal.h"
#include "OxiDamageComponent.h"
#include "OxiSentryDamageComponent.generated.h"

/**
 * 
 */
UCLASS(Blueprintable, editinlinenew, meta = (BlueprintSpawnableComponent))
class OXI_API UOxiSentryDamageComponent : public UOxiDamageComponent
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction);

protected:

	void LifeSpanCallback();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=FX)
	TArray<TSubclassOf<AActor>> BloodDecals;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=FX)
	TArray<TSubclassOf<AActor>> BloodSprayFX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=FX)
	TArray<TSubclassOf<AActor>> GibList;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=FX)
	float GibChance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=FX)
	float DeathDurationSec;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category=FX)
	float DeathStartTime;

	FTimerHandle DeleteTimer;
	
	UPROPERTY(Transient)
	float LastClipTime;

	// IOxiDamageInterface
protected:
	virtual float TakeDamage(const FOxiDamageInfo& DamageInfo) override;
};
