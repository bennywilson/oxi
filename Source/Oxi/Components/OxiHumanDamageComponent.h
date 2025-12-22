// ELP 2020

#pragma once

#include "CoreMinimal.h"
#include "OxiDamageComponent.h"
#include "OxiHumanDamageComponent.generated.h"

/**
 * 
 */
UCLASS(Blueprintable, editinlinenew, meta = (BlueprintSpawnableComponent))
class OXI_API UOxiHumanDamageComponent : public UOxiDamageComponent
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;

protected:
	virtual float TakeDamage(const FOxiDamageInfo& DamageInfo) override;

	bool bRagdolling = false;
};

/**
 *
 */
UCLASS(Blueprintable, editinlinenew, meta = (BlueprintSpawnableComponent))
class OXI_API UOxiPlayerDamageComponent : public UOxiDamageComponent
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:

	virtual float TakeDamage(const FOxiDamageInfo& DamageInfo) override;

protected:

	UPROPERTY(Transient)
	UMaterialInstanceDynamic* PlayerDamagePP_MatInst;

	float LastDamageTime;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SecondsUntilHealthRegen;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float HealthRegenRate;
};