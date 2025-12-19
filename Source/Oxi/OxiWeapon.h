// ELP 2020

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "OxiDamageComponent.h"
#include "OxiWeapon.generated.h"

/**
 *
 */
UCLASS(BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class OXI_API AItem : public AActor
{
	GENERATED_BODY()

};

/**
 *
 */
UCLASS(BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class OXI_API AOxiWeapon : public AItem
{
	GENERATED_BODY()

public:

	AOxiWeapon();
	
	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "Animations")
	bool StartFireWeapon(const UCameraComponent* const FirstPersonCameraComponent, const bool bIsOutline, const FVector VecToTarget, const float SpreadMultiplier, const AOxiCharacter* const WeaponOwner, const int NumTimesToFire);

	UFUNCTION(BlueprintCallable)
	FName GetBodyName(const UStaticMeshComponent* const StaticMeshComp, const int ElementIndex);

	UFUNCTION(BlueprintCallable)
	EWoundLevel GetWoundLevel() const { return (EWoundLevel)WoundLevel; }

protected:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	USkeletalMeshComponent* Mesh1P;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	int ClipSize;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	int CurrentAmmoCount;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	float WeaponDamage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (Bitmask, BitmaskEnum = "/Script/Oxi.EWoundLevel"))
	int32 WoundLevel;
};