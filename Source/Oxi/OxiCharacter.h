// ELP 2020

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "OxiWeapon.h"
#include "OxiCover.h"
#include "Components/OxiDamageComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "OxiCharacter.generated.h"


UENUM(BlueprintType)
enum class EOxiVOType : uint8
{
	EnemyDiscovered,
	BattleChatter,
	BattleCommand,
	NeighborhoodCallout,
	Reloading,
	Hurt,
};

USTRUCT(BlueprintType)
struct FOxiVOData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EOxiVOType VOType = EOxiVOType::EnemyDiscovered;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	USoundWave* SoundWave = nullptr;

	float StartTime = 0.0f;
};

/**
 *
 */
UENUM(BlueprintType)
enum class EOxiSquadState : uint8
{
	Idle,
	Patrol,
	Attack
};

/**
 * 
 */
UCLASS()
class OXI_API UOxiAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Oxi Anim")
	TMap<FName, UAnimSequence*> AnimSequenceMap;
};

/**
 * 
 */
UCLASS()
class UOxiCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

	virtual void RequestDirectMove(const FVector& MoveVelocity, bool bForceMaxSpeed);

	UFUNCTION(BlueprintCallable, Category = "Oxi Character Movement")
	FVector GetRequestedVelocity() const { return RequestedVelocity; }
};

/**
 *	AOxiCharacter
 */
UENUM(BlueprintType)
enum OxiPlayerState
{
	Normal,
	Interacting
};

UCLASS()
class AOxiCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AOxiCharacter();
	AOxiCharacter(const FObjectInitializer&);

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void SetSquad(const class AOxiSquad* newSquad) { Squad = newSquad; }
	void SetVOData(const TArray<FOxiVOData>& inVO) { VOData = inVO; }

	TArray<FOxiVOData>& GetVOData() { return VOData; }

	UFUNCTION(BlueprintCallable)
	bool AcquireCover(AOxiCover* const Cover, FVector threatLocation);

	UFUNCTION(BlueprintCallable)
	void ReleaseCover();

	virtual void OnCoverProtectionLevelChanged(AOxiCover* const, EOxiCoverProtectionLevel) { }

	UFUNCTION(BlueprintImplementableEvent)
	void DebugDraw(const int debugLevel);

	UFUNCTION(BlueprintCallable)
	AOxiCover* GetCurrentCover() const { return CurrentCover; }

	const TMap<FName, float>& GetHitBoneToDamageMultiplier() const { return HitBoneToDamageMultiplier; }

	UFUNCTION(BlueprintCallable)
	void SetOxiCharacterBase(UPrimitiveComponent* NewBase, const FName BoneName = NAME_None, bool bNotifyActor = true) { SetBase(NewBase, BoneName, bNotifyActor); }

protected:
	UFUNCTION(BlueprintImplementableEvent)
	void OnDeath_Internal(UOxiDamageComponent* DamageComp, AActor* Victim, AActor* Killer);

	void DestroyCharacter();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TMap<FName, float> HitBoneToDamageMultiplier;


	UPROPERTY(Transient, BlueprintReadOnly)
	AOxiCover* CurrentCover;

	UPROPERTY(Transient, BlueprintReadOnly)
	const UOxiCoverSpotComponent* CurrentCoverSpot;

	UPROPERTY(Transient, BlueprintReadOnly)
	const AOxiSquad* Squad;

	UPROPERTY(Transient, BlueprintReadWrite)
	TArray<FOxiVOData> VOData;

private:

	virtual void OnDeath(class UOxiDamageComponent* const DamageComp, AActor* const Victim, AActor* const Killer);
};

/**
 *
 */
UCLASS()
class AOxiFirstPersonCharacter : public AOxiCharacter
{
	GENERATED_BODY()

public:
	AOxiFirstPersonCharacter();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void BeginDestroy() override;

	virtual void TickActor(float DeltaTime, enum ELevelTick TickType, FActorTickFunction& ThisTickFunction);

	/** Returns Mesh1P subobject **/
	FORCEINLINE class USkeletalMeshComponent* GetMesh1P() const { return Mesh1P; }
	/** Returns FirstPersonCameraComponent subobject **/
	FORCEINLINE class UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }

protected:
	void OnStartFire(const struct FInputActionValue& Value);
	void OnStopFire(const struct FInputActionValue& Value);

	void OnInteraction(const struct FInputActionValue& Value);

	void Move(const struct FInputActionValue& Value);
	void Look(const struct FInputActionValue& Value);
	void GamePadLook(const struct FInputActionValue& Value);

	void MoveForward(float Val);
	void MoveRight(float Val);

	virtual void OnDeath(class UOxiDamageComponent* const DamageComp, AActor* const Victim, AActor* const Killer) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
	class USkeletalMeshComponent* Mesh1P;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	class UCameraComponent* FirstPersonCameraComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	class USpringArmComponent* SpringArmComponent;

	UFUNCTION(BlueprintCallable, Category = "Oxi Character")
	void ChangeOxiPlayerState(OxiPlayerState NewPlayerState, FVector InteractionPos, bool MovePlayer, FRotator InteractionRot, bool RotatePlayer);

	UFUNCTION(BlueprintCallable, Category = "Oxi Character")
	OxiPlayerState GetOxiPlayerState() const { return PlayerState; }

	UPROPERTY(Transient, BlueprintReadWrite)
	AOxiWeapon* EquippedItem;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	TSubclassOf<AOxiWeapon> DefaultWeapon;

protected:

	/** Look  Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	class UInputAction* LookAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	class UInputAction* MoveAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	class UInputAction* JumpAction;

	/** Fire Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	class UInputAction* FireAction;

	/** Reload Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	class UInputAction* ReloadAction;

	/** Swap Weapon Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	class UInputAction* SwapWeaponAction;

	/** Aim Down Sights Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	class UInputAction* AimDownSightsAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	class UInputMappingContext* MappingContext;

private:
	UFUNCTION()
	void OnCharacterDeathEvent(class UOxiDamageComponent* Victim, UOxiDamageComponent* Killer);

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	float BaseTurnRate;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	float BaseLookUpRate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
	FVector GunOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
	float BaseHealth;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
	FName GripPointName;

	UPROPERTY(BlueprintReadWrite, interp, Category = "Light")
	FLinearColor OxiColor;

	UPROPERTY(BlueprintReadWrite, interp, Category = "Light")
	FLinearColor BloodColor;

	UPROPERTY(BlueprintReadWrite, interp, Category = "Light")
	FLinearColor OxiLightColor;

	UPROPERTY(BlueprintReadWrite, interp, Category = "Light")
	FLinearColor BloodLightColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Oxi Damage")
	TArray<ULightComponent*> OxiPulseLightList;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VO")
	TArray<class USoundBase*> EnemyKilledVO;

	UPROPERTY(Transient, BlueprintReadOnly)
	bool AimingDownSights;

	float AimingDownSightsStartTime;

	float AimingDownSightsEndTime;

	UPROPERTY(Transient, BlueprintReadOnly)
	float AimingDownSightsElapsedTime;

	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "Weapon")
	bool StartFireWeapon(const UCameraComponent* const CameraComp1P);

	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "Weapon")
	void TryReload();

	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "Weapon")
	void TrySwitchWeapon();

	UFUNCTION(BlueprintImplementableEvent)
	void DamageTakenCB(AActor* DamagedActor, FOxiDamageInfo DamageInfo);

	UFUNCTION(BlueprintNativeEvent, Category = "Weapon")
	void StartADS();

	UFUNCTION(BlueprintNativeEvent, Category = "Weapon")
	void EndADS();


	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;

private:
	UMaterialInstanceDynamic* HandsMaterial;

	float CurrentHealth;

	FDelegateHandle OnCharacterDeathEventHandle;

	// Player State	
	OxiPlayerState PlayerState;

	float CurT;
	FVector StartLocation;
	FVector TargetLocation;
	bool ShouldMoveTowardsTarget;

	FRotator StartRotation;
	FRotator TargetRotation;
	bool ShouldRotateTowardsTarget;

	UPROPERTY(Transient)
	bool IsFireDown;
};

/**
 *
 */
UCLASS(BlueprintType, Blueprintable)
class AOxiPlayerController : public APlayerController
{
	GENERATED_BODY()

	AOxiPlayerController();

	virtual void TickActor(float DeltaTime, enum ELevelTick TickType, FActorTickFunction& ThisTickFunction) override;

private:
	void UpdateAutoAim(const float DT);
};