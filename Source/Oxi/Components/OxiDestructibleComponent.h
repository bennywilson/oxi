// ELP 2020

#pragma once

#include "OxiDamageComponent.h"
#include "OxiDestructibleComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTakeDamage, class AActor* const, damageCauser, float, damageAmount);

UENUM(BlueprintType)
enum EOxiDestructibleType
{
	Standing,
	Crouching,
};

UCLASS(Blueprintable, editinlinenew, meta = (BlueprintSpawnableComponent))
class OXI_API UOxiDestructibleComponent : public UOxiDamageComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	UOxiDestructibleComponent();

	UPROPERTY(BlueprintAssignable)
	FOnTakeDamage OnDestructibleKilled;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction);

	UFUNCTION(BlueprintCallable, Category = "Oxi Damage")
	bool InitDestructibleComponent(UStaticMeshComponent* InBaseMeshComponent, USkeletalMeshComponent* InDestructibleMeshComponent);

	int GetNumBrokenPieces() const { return NumBrokenPieces; }

	UFUNCTION(BlueprintCallable, Category = "Oxi Damage")
	void DisablePhysicsOnBodies(const TArray<int>& BodyList, bool shouldEnableResponse);

private:
	virtual float TakeDamage(const FOxiDamageInfo& DamageInfo) override;

protected:
	UPROPERTY(EditAnywhere, Category = "Oxi Damage")
	float Health;

	UPROPERTY(EditAnywhere, Category = "Oxi Damage")
	float ExplosionSplashDamageRadius;

	UPROPERTY(EditAnywhere, Category = "Oxi Damage")
	float ExplosionSplashDamageAmount;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Oxi Damage FX")
	TEnumAsByte<EOxiDestructibleType> DestructibleType;

	UPROPERTY(EditAnywhere, Category = "Oxi Damage FX")
	float ExplosionImpulseMagnitude;

	UPROPERTY(EditAnywhere, Category = "Oxi Damage FX")
	float ExplosionXYImpulseMin;

	UPROPERTY(EditAnywhere, Category = "Oxi Damage FX")
	float ExplosionXYImpulseMax;

	UPROPERTY(EditAnywhere, Category = "Oxi Damage FX")
	float ExplosionZImpulseMin;

	UPROPERTY(EditAnywhere, Category = "Oxi Damage FX")
	float ExplosionZImpulseMax;

	UPROPERTY(EditAnywhere, Category = "Oxi Damage FX")
	float ExplosionAngularImpulseMin;

	UPROPERTY(EditAnywhere, Category = "Oxi Damage FX")
	float ExplosionAngularImpulseMax;

	UPROPERTY(EditAnywhere, Category = "Oxi Damage FX")
	class USoundBase* ExplosionSound;

	UPROPERTY(EditAnywhere, Category = "Oxi Damage FX")
	float SmearInitialPopDistance;

	UPROPERTY(EditAnywhere, Category = "Oxi Damage FX")
	float SmearLengthSec;

	UPROPERTY(EditAnywhere, Category = "Oxi Damage FX")
	float SmearStartStrength;

	UPROPERTY(EditAnywhere, Category = "Oxi Damage FX")
	float SmearStartTime;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Oxi Damage FX")
	class ULightComponent* ExplosionLightComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Oxi Damage FX")
	class UParticleSystemComponent* ExplosionParticleComponent;

	UPROPERTY(EditAnywhere, Category = "Oxi Damage FX")
	float ExplosionLightDurationSec;

	UPROPERTY(EditAnywhere, Category = "Oxi Damage FX")
	TArray<TSubclassOf<AActor>> BodyKnockOffFX;

	UPROPERTY(EditAnywhere, Category = "Oxi Damage FX")
	bool HideBodiesWhenKnockedOff;

	UPROPERTY(EditAnywhere, Category = "Oxi Damage")
	float ThresholdToShowDamagedMeshAt;

	UPROPERTY(EditAnywhere, Category = "Oxi Damage")
	bool DisableCollisionWhenDead;

	UPROPERTY(Transient)
	int NumBrokenPieces;

	TArray<int> BodiesToSkip;

	float ExplosionLightTargetIntensity;

	UStaticMeshComponent* BaseMeshComponent;
	USkeletalMeshComponent* DestructibleMeshComponent;
};
