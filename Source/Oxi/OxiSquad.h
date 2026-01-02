// ELP 2022

#pragma once

#include "CoreMinimal.h"
#include "OxiCharacter.h"
#include "OxiCover.h"

#include "OxiSquad.generated.h"


/**
 *
 */
USTRUCT(BlueprintType)
struct FOxiSquadTarget
{
	GENERATED_BODY()

	UPROPERTY(Transient, EditAnywhere, BlueprintReadWrite)
	AOxiCharacter* Character = nullptr;

	UPROPERTY(Transient, EditAnywhere, BlueprintReadWrite)
	FVector Location = FVector(0.0f, 0.0f, 0.0f);
};

USTRUCT(BlueprintType)
struct FOxiSquadBehaviorContexts
{
	GENERATED_BODY()

	// Actions squad will take if the other lists are empty or their conditions haven't been met	
	UPROPERTY(EditAnywhere)
	TArray<TSubclassOf<UOxiSquadBehavior>> DefaultSquadBehaviors;

	// Actions squad will take at the beginning of battle
	UPROPERTY(EditAnywhere)
	TArray<TSubclassOf<UOxiSquadBehavior>> InitialSquadBehaviors;

	// Actions squad will take when desperate (ex. losing the fight, low morale, etc)
	UPROPERTY(EditAnywhere)
	TArray<TSubclassOf<UOxiSquadBehavior>> DesperateSquadBehaviors;

	// Actions squad will take when winning (ex. player is hurt, high morale, etc)
	UPROPERTY(EditAnywhere)
	TArray<TSubclassOf<UOxiSquadBehavior>> ConfidentSquadBehaviors;

	void ApplyOverrides(const FOxiSquadBehaviorContexts& behaviorOverrides)
	{
		if (behaviorOverrides.DefaultSquadBehaviors.Num() > 0)
		{
			DefaultSquadBehaviors = behaviorOverrides.DefaultSquadBehaviors;
		}

		if (behaviorOverrides.InitialSquadBehaviors.Num() > 0)
		{
			InitialSquadBehaviors = behaviorOverrides.InitialSquadBehaviors;
		}

		if (behaviorOverrides.DesperateSquadBehaviors.Num() > 0)
		{
			DesperateSquadBehaviors = behaviorOverrides.DesperateSquadBehaviors;
		}

		if (behaviorOverrides.ConfidentSquadBehaviors.Num() > 0)
		{
			ConfidentSquadBehaviors = behaviorOverrides.ConfidentSquadBehaviors;
		}
	}
};

UCLASS(BlueprintType)
class OXI_API AOxiSquad : public AActor
{
	GENERATED_BODY()

public:
	AOxiSquad();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void ApplyBehaviorOverrides(const FOxiSquadBehaviorContexts& overrides) { BehaviorContexts.ApplyOverrides(overrides); }

	void StartSquadActions();

	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable)
	void AddSquadMember(AOxiCharacter* const SquadMember);

	int GetNumSquadMembers() const { return CurrentSquadMembers.Num(); }
	int GetNumAliveSquadMembers() const;

	const TArray<AOxiCharacter*>& GetSquadMembers() const { return CurrentSquadMembers; }
	void GetAliveSquadMembers(TArray<AOxiCharacter*>& outSquadMembers);

	UFUNCTION(BlueprintCallable)
	bool PlaySquadMemberVO(class AOxiAICharacter* const squadMember, EOxiVOType VOType, USoundAttenuation* const soundAttenuation);

private:
	void SquadMemberKilledCB(UOxiDamageComponent* const DamageComp, AActor* const Victim, AActor* const Killer, const EWoundLevel WoundLevel);
	void EnterAttackState();
	void TickAttackState(const float DeltaTime);

	void TickIdleState(const float DeltaTIme);

protected:

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EOxiSquadState SquadState;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float PerceptionRadius;

	UPROPERTY(EditAnywhere)
	FOxiSquadBehaviorContexts BehaviorContexts;

	// If a target leaves this radius, they are considered to have changed positions and the squad may call a new action
	UPROPERTY(EditAnywhere)
	float TargetsPositionRadius;

	UPROPERTY(EditAnywhere)
	bool SuppressVO;

	UPROPERTY(BlueprintReadOnly, Transient)
	TArray<FOxiSquadTarget> SquadTargets;

	UPROPERTY(Transient, BlueprintReadonly)
	TArray<AOxiCharacter*> CurrentSquadMembers;

	UPROPERTY(EditAnywhere)
	TSubclassOf<UOxiSquadBehavior> DebugBehavior;

	UPROPERTY(Transient, BlueprintReadWrite)
	UOxiSquadBehavior* CurrentBehavior;

private:
	TArray<FOxiVOData> RunningVO;
};

/**
 *
 */
UCLASS(Blueprintable, BlueprintType)
class OXI_API UOxiSquadBehavior : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintImplementableEvent)
	void StartBehavior(AOxiSquad* OxiSquad);

	UFUNCTION(BlueprintImplementableEvent)
	void TickBehavior(const float DeltaTime);

	UFUNCTION(BlueprintImplementableEvent)
	void OnTargetChangedPosition(const FOxiSquadTarget& Target, const FVector oldPosition);

protected:
	UFUNCTION(BlueprintCallable)
	void GetCoverInRadius(TArray<AOxiCover*>& OutCoverList, const FVector& TestPoint, const float radius);

	UFUNCTION(BlueprintCallable)
	void GetOutermostSquadMembers(TArray<int>& outCharacters, TArray<FVector>& outRightVec, const FVector focusPoint);

protected:
	UPROPERTY(Transient, BlueprintReadWrite)
	AOxiSquad* OwningSquad;
};