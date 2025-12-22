// ELP 2022

#include "OxiSquad.h"
#include "Kismet/GameplayStatics.h"
#include "OxiAIManager.h"
#include "Components/OxiHumanDamageComponent.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "DrawDebugHelpers.h"

/** Toggles onscreen AI debugging information */
static TAutoConsoleVariable<int32> CVarSquadDebug(
	TEXT("oxi.squaddebug"),
	0,
	TEXT("Show debug squad indo"),
	ECVF_Cheat
);


/**
 * 
 */
AOxiSquad::AOxiSquad()
{
	PrimaryActorTick.bCanEverTick = true;
	TargetsPositionRadius = 1000.0f;
}

/**
 *
 */
void AOxiSquad::AddSquadMember(AOxiCharacter* const SquadMemberToAdd)
{
	if (ensure(SquadMemberToAdd) == false)
	{
		return;
	}

	SquadMemberToAdd->SetSquad(this);

	UOxiHumanDamageComponent* const DamageComp = Cast<UOxiHumanDamageComponent>(SquadMemberToAdd->GetComponentByClass(UOxiHumanDamageComponent::StaticClass()));
	if (DamageComp != nullptr)
	{
		DamageComp->OnDeath.AddUObject(this, &AOxiSquad::SquadMemberKilledCB);
	}

	CurrentSquadMembers.Add(SquadMemberToAdd);
}

/**
 *
 */
void AOxiSquad::SquadMemberKilledCB(UOxiDamageComponent* const DamageComp, AActor* const Victim, AActor* const Killer)
{
	check(DamageComp);

	AOxiCharacter* const OxiChar = Cast<AOxiCharacter>(Victim);
	check(OxiChar != nullptr);

	DamageComp->OnDeath.RemoveAll(this);
}

/**
 *
 */
void AOxiSquad::BeginPlay()
{
	Super::BeginPlay();

	GetOxiAIManager(this)->RegisterSquad(this);
}

/**
 *
 */
void AOxiSquad::StartSquadActions()
{
	if (BehaviorContexts.DefaultSquadBehaviors.Num() == 0)
	{
		return;
	}

	SquadState = EOxiSquadState::Attack;

	UOxiAIManager* AIMgr = GetOxiAIManager(this);
	TArray<AOxiFirstPersonCharacter*> PlayerList = AIMgr->GetPlayerList();
	if (PlayerList.Num() == 0)
	{
		return;
	}

	AOxiFirstPersonCharacter* const Player = PlayerList[0];
	for (int i = 0; i < CurrentSquadMembers.Num(); i++)
	{
		AOxiCharacter* const SquadMember = CurrentSquadMembers[i];
		{
			SquadTargets.Empty();
			FOxiSquadTarget NewTarget;
			NewTarget.Character = Player;
			NewTarget.Location = Player->GetActorLocation();
			SquadTargets.Add(NewTarget);
		}
	}


	TSubclassOf<UOxiSquadBehavior> DesiredBehavior;
	if (DebugBehavior.Get() != nullptr)
	{
		DesiredBehavior = DebugBehavior;
	}
	else
	{
		DesiredBehavior = BehaviorContexts.DefaultSquadBehaviors[FMath::RandRange(0, BehaviorContexts.DefaultSquadBehaviors.Num() - 1)];
	}

	check(DesiredBehavior != nullptr);
	CurrentBehavior = NewObject<UOxiSquadBehavior>(this, DesiredBehavior);
	CurrentBehavior->StartBehavior(this);
}

/**
 *
 */
void AOxiSquad::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

/*	for (int i = 0; i < CurrentSquadMembers.Num(); i++)
	{
		GWorld->DestroyActor(CurrentSquadMembers[i]);
	}
*/
	CurrentSquadMembers.Empty();

	UOxiAIManager* AIMgr = GetOxiAIManager(this);
	AIMgr->UnregisterSquad(this);
}

/**
 *
 */
void AOxiSquad::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UOxiAIManager* const AIMgr = GetOxiAIManager(this);
	switch (SquadState)
	{
		case EOxiSquadState::Idle :
		{
			TickIdleState(DeltaTime); 
			break;
		}

		case EOxiSquadState::Attack:
		{
			TickAttackState(DeltaTime);
			break;
		}
	}

	const double curTimeSec = GetWorld()->GetTimeSeconds();
	for (int i = RunningVO.Num() - 1; i >= 0; i--)
	{
		if (curTimeSec > RunningVO[i].StartTime + 5.0f)
		{
			RunningVO.RemoveAt(i);
		}
	}

	const int debugSquadLevel = CVarSquadDebug.GetValueOnGameThread();
	if (debugSquadLevel)
	{
		if (debugSquadLevel == 1 || debugSquadLevel == 2)
		{
			for (int i = 0; i < SquadTargets.Num(); i++)
			{
				const FOxiSquadTarget& curSquadTarget = SquadTargets[i];
				const FMatrix debugMatrix(FVector::UpVector, FVector::ForwardVector, FVector::LeftVector, curSquadTarget.Location);

			//	DrawDebugSolidCircle(GetWorld(), debugMatrix, TargetsPositionRadius, 36, FColor(255, 0, 0, 64), false, -1.0f, SDPG_World);
			}
		}
	}
}

/**
 *
 */
void AOxiSquad::TickIdleState(const float DeltaTime)
{
	UOxiAIManager* AIMgr = GetOxiAIManager(this);
	TArray<AOxiFirstPersonCharacter*> PlayerList = AIMgr->GetPlayerList();
	if (PlayerList.Num() == 0)
	{
		return;
	}

	AOxiFirstPersonCharacter* const Player = PlayerList[0];
	for (int i = 0; i < CurrentSquadMembers.Num(); i++)
	{
		AOxiCharacter* const SquadMember = CurrentSquadMembers[i];
		if (PerceptionRadius <= 0.0f || FVector::Dist(SquadMember->GetActorLocation(), Player->GetActorLocation()) <= PerceptionRadius)
		{
			SquadTargets.Empty();
			FOxiSquadTarget NewTarget;
			NewTarget.Character = Player;
			NewTarget.Location = Player->GetActorLocation();
			SquadTargets.Add(NewTarget);

			EnterAttackState();
			break;
		}
	}
}

/**
 *
 */
void AOxiSquad::EnterAttackState()
{
	check(SquadTargets.Num() > 0);

}

/**
 *
 */
void AOxiSquad::TickAttackState(const float DeltaTime)
{
	for (int i = 0; i < SquadTargets.Num(); i++)
	{
		FOxiSquadTarget& CurTarget = SquadTargets[i];
		const FVector ActorsLocation = CurTarget.Character->GetActorLocation();
		if (FVector::Dist(ActorsLocation, CurTarget.Location) > TargetsPositionRadius)
		{
			const FVector OldPosition = CurTarget.Location;
			CurTarget.Location = ActorsLocation;			
			CurrentBehavior->OnTargetChangedPosition(CurTarget, OldPosition);
			break;
		}
	}

	if (ensure(CurrentBehavior != nullptr) == false)
	{
		return;
	}

	CurrentBehavior->TickBehavior(DeltaTime);
}

/**
 *
 */
int AOxiSquad::GetNumAliveSquadMembers() const
{
	int NumAlive = 0;

	for (int i = 0; i < CurrentSquadMembers.Num(); i++)
	{
		AOxiCharacter* const SquadMember = CurrentSquadMembers[i];
		UOxiHumanDamageComponent* const DamageComp = Cast<UOxiHumanDamageComponent>(SquadMember->GetComponentByClass(UOxiHumanDamageComponent::StaticClass()));
		if (DamageComp == nullptr || DamageComp->IsAlive())
		{
			NumAlive++;
		}
	}

	return NumAlive;
}

/**
 *
 */
void AOxiSquad::GetAliveSquadMembers(TArray<AOxiCharacter*>& outSquadMembers)
{
	check(outSquadMembers.Num() == 0);

	for (int i = 0; i < CurrentSquadMembers.Num(); i++)
	{
		AOxiCharacter* const SquadMember = CurrentSquadMembers[i];
		UOxiHumanDamageComponent* const DamageComp = Cast<UOxiHumanDamageComponent>(SquadMember->GetComponentByClass(UOxiHumanDamageComponent::StaticClass()));
		if (DamageComp != nullptr && DamageComp->IsAlive() == false)
		{
			continue;
		}
		outSquadMembers.Add(SquadMember);
	}
}


/**
 *
 */
void UOxiSquadBehavior::GetCoverInRadius(TArray<AOxiCover*>& OutCoverList, const FVector& TestPoint, const float radius)
{
	UOxiAIManager* const OxiMgr = GetOxiAIManager(this);
	OutCoverList = OxiMgr->GetCoverList();
}

/**
 *
 */
void UOxiSquadBehavior::GetOutermostSquadMembers(TArray<int>& outCharacters, TArray<FVector>& outRightVec, const FVector focusPoint)
{
	check(OwningSquad != nullptr);

	TArray<AOxiCharacter*> squadMembers;
	OwningSquad->GetAliveSquadMembers(squadMembers);
	if (squadMembers.Num() == 0)
	{
		return;
	}
	
	// Get the mid point of the squad members
	FVector2D squadCenter(0.f);
	for (int i = 0; i < squadMembers.Num(); i++)
	{
		const FVector2D squadMember2DPos(squadMembers[i]->GetActorLocation());
		squadCenter += squadMember2DPos;
	}
	squadCenter /= squadMembers.Num();

	const FVector2D focusPoint2D(focusPoint.X, focusPoint.Y);
	const FVector2D squadToFocusForwardVec = (focusPoint2D - squadCenter).GetSafeNormal();
	const FVector2D squadToFocusRightVec(squadToFocusForwardVec.Y, squadToFocusForwardVec.X);

	int furthestAlongNegIdx = -1;
	int furthestAlongPosIdx = -1;
	float furthestAlongNegDist = std::numeric_limits<float>::max();
	float futhestAlongPosDist = std::numeric_limits<float>::lowest();

	for (int i = 0; i < squadMembers.Num(); i++)
	{
		const AOxiCharacter* curSquadMember = squadMembers[i];
		const FVector2D curMember2DPos(curSquadMember->GetActorLocation());
		const FVector2D squadCenterToMember = curMember2DPos - squadCenter;
		const float distSqrFromCenter = squadCenterToMember.Dot(squadToFocusRightVec);

		if (distSqrFromCenter >= futhestAlongPosDist)
		{
			furthestAlongPosIdx = i;
			futhestAlongPosDist = distSqrFromCenter;
		}

		if (distSqrFromCenter <= furthestAlongNegDist)
		{
			furthestAlongNegIdx = i;
			furthestAlongNegDist = distSqrFromCenter;
		}
	}

	check(furthestAlongPosIdx != -1 && furthestAlongNegDist != -1);
	outCharacters.Add(furthestAlongPosIdx);
	outCharacters.Add(furthestAlongNegIdx);

	outRightVec.Add(FVector(squadToFocusRightVec.X, squadToFocusRightVec.Y, 0.0f));
	outRightVec.Add(FVector(-squadToFocusRightVec.X, -squadToFocusRightVec.Y, 0.0f));
}

/**
 *
 */
bool AOxiSquad::PlaySquadMemberVO(class AOxiAICharacter* const squadMember, EOxiVOType VOType, USoundAttenuation* const soundAttenuation)
{
	check(squadMember != nullptr);

	if (SuppressVO)
	{
		return false;
	}

	TArray<FOxiVOData*> availableVO;
	TArray<FOxiVOData>& squadVO = squadMember->GetVOData();
	for (int i = 0; i < squadVO.Num(); i++)
	{
		if (squadVO[i].VOType == VOType)
		{
			availableVO.Add(&squadVO[i]);
		}
	}

	if (availableVO.Num() == 0)
	{
		return false;
	}

	const double currentTimeSeconds = GetWorld()->GetTimeSeconds();
	float delayBetween = 5.0f;
	if (VOType == EOxiVOType::Hurt)
	{
		delayBetween = 3.0f;
	}

	for (int i = 0; i < RunningVO.Num(); i++)
	{
		if (currentTimeSeconds < RunningVO[i].StartTime + delayBetween)
		{
			return false;
		}
	}

	while (availableVO.Num())
	{
		const int randIdx = rand() % availableVO.Num();
		FOxiVOData selectedVO = *availableVO[randIdx];
		availableVO.RemoveAt(randIdx);
		if (selectedVO.StartTime == 0.0f || selectedVO.StartTime > GetWorld()->GetTimeSeconds() + 10.0f)
		{
			selectedVO.StartTime = currentTimeSeconds;
			RunningVO.Add(selectedVO);

			FVector someVec(ForceInitToZero);
			UGameplayStatics::SpawnSoundAttached(selectedVO.SoundWave, squadMember->GetRootComponent(), EName(), someVec, (FRotator)FRotator::ZeroRotator, (EAttachLocation::Type)EAttachLocation::KeepRelativeOffset, true, 1.0f, 1.0f, 0.0f, soundAttenuation, nullptr, true);

			break;
		}

		if (availableVO.Num() == 0)
		{
			return false;
		}
	}

	return true;
}