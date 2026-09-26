// ELP 2022

#include "OxiAIVolumes.h"
#include "OxiSquad.h"
#include "Components/ArrowComponent.h"
#include "Components/BillboardComponent.h"

/**
 * 
 */
AOxiAISquadMemberSpawn::AOxiAISquadMemberSpawn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* NewSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	SetRootComponent(NewSceneComponent);

#if WITH_EDITORONLY_DATA
	ArrowComponent = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("ArrowComponent0"));
	SpriteComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (!IsRunningCommandlet())
	{
		// Structure to hold one-time initialization
		struct FConstructorStatics
		{
			ConstructorHelpers::FObjectFinderOptional<UTexture2D> SquadSpawnTexture;
			FName ID_SpawnSquadTrigger;
			FText NAME_OxiAISpawn;
			FConstructorStatics()
				: SquadSpawnTexture(TEXT("/Game/Oxi/Core/Editor/S_HiddenHand"))
				, ID_SpawnSquadTrigger(TEXT("SpawnSquad"))
				, NAME_OxiAISpawn(NSLOCTEXT("SpriteCategory", "OxiAISpawnSquad", "OxiAISpawnSquads"))
			{
			}
		};
		static FConstructorStatics ConstructorStatics;

		if (ArrowComponent)
		{
			ArrowComponent->bTreatAsASprite = true;
			ArrowComponent->ArrowSize = 1.0f;
			ArrowComponent->ArrowColor = FColor(80, 80, 200, 255);
			ArrowComponent->SpriteInfo.Category = ConstructorStatics.ID_SpawnSquadTrigger;
			ArrowComponent->SpriteInfo.DisplayName = ConstructorStatics.NAME_OxiAISpawn;
			ArrowComponent->SetupAttachment(NewSceneComponent);
			ArrowComponent->SetUsingAbsoluteScale(true);
			ArrowComponent->bIsScreenSizeScaled = true;
		}

		if (SpriteComponent)
		{
			SpriteComponent->Sprite = ConstructorStatics.SquadSpawnTexture.Get();
			SpriteComponent->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.5f));
			SpriteComponent->SpriteInfo.Category = ConstructorStatics.ID_SpawnSquadTrigger;
			SpriteComponent->SpriteInfo.DisplayName = ConstructorStatics.NAME_OxiAISpawn;
			SpriteComponent->SetupAttachment(NewSceneComponent);
			SpriteComponent->bIsScreenSizeScaled = true;
			SpriteComponent->SetUsingAbsoluteScale(true);
			SpriteComponent->bReceivesDecals = false;
		}
	}
#endif
}

/**
 *
 */
AOxiAISpawnSquadTrigger::AOxiAISpawnSquadTrigger()
{
	this->GetRootComponent()->Bounds.SphereRadius = 1.0f;
}

/**
 * 
 */
void AOxiAISpawnSquadTrigger::ActorEnteredVolume(AActor* Other)
{
	const int enableAI = CVarEnableAI.GetValueOnGameThread();
	if (enableAI == 0)
	{
		return;
	}

	if (HasFired)
	{ 
		HasFired = true;
		return;
	}

	AOxiFirstPersonCharacter* const Player = Cast <AOxiFirstPersonCharacter>(Other);
	if (Other == nullptr)
	{
		return;
	}

	HasFired = true;

	ActivateSpawnSquadTrigger();
}

/**
 *
 */
void AOxiAISpawnSquadTrigger::ActivateSpawnSquadTrigger()
{
	if (SquadTypes.Num() == 0)
	{
		UE_LOG(LogOxiAI, Warning, TEXT("AAISpawnSquadTrigger::ActorEnteredVolume() - Volume %s doesn't have any squad types"), *GetFullName());
		return;
	}

	/*if (Squad != nullptr && Squad->GetNumAliveSquadMembers() > 0)
	{
		return;
	}*/

	if (SquadMembersToSpawn.Num() == 0 && PosedSquadMembersToSpawn.Num() == 0)
	{
		UE_LOG(LogOxiAI, Warning, TEXT("AAISpawnSquadTrigger::ActorEnteredVolume() - Volume %s has no squad members to spawn"), *GetFullName());
		return;
	}

	if (Squad != nullptr)
	{
		GWorld->DestroyActor(Squad);
		Squad = nullptr;
	}

	const FVector SpawnLoc = GetActorLocation();
	const FRotator SpawnRot = GetActorRotation();

	const int SquadIdx = FMath::Rand() % SquadTypes.Num();
	TSubclassOf<AOxiSquad> SquadType = SquadTypes[SquadIdx];

	if (*SquadType == nullptr)
	{
		UE_LOG(LogOxiAI, Warning, TEXT("AAISpawnSquadTrigger::ActorEnteredVolume() - Volume %s has a null squad type"), *GetFullName());
		return;
	}
	Squad = Cast<AOxiSquad>(GWorld->SpawnActor(SquadType, &SpawnLoc, &SpawnRot));

	for (int i = 0; i < SquadMembersToSpawn.Num(); i++)
	{
		if (SquadMembersToSpawn[i] == nullptr)
		{
			UE_LOG(LogOxiAI, Warning, TEXT("AAISpawnSquadTrigger::ActorEnteredVolume() - Volume %s has no null squad member entries"), *GetFullName());
			continue;
		}

		TSubclassOf<AOxiCharacter> ActorToSpawn = SquadMembersToSpawn[i]->GetOxiCharacterClassToSpawn();

		AOxiAICharacter* const OxiChar = Cast<AOxiAICharacter>(GWorld->SpawnActor(ActorToSpawn, &SquadMembersToSpawn[i]->GetTransform()));
		check(OxiChar != nullptr);

		Squad->AddSquadMember(OxiChar);
	}

	for (AOxiAICharacter* const Char : PosedSquadMembersToSpawn)
	{
		Squad->AddSquadMember(Char);
	}

	Squad->ApplyBehaviorOverrides(BehaviorOverrides);
	Squad->StartSquadActions();

	OnSquadSpawned.Broadcast(nullptr);
}