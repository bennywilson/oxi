// ELP 2022

#include "OxiAIManager.h"
#include "Kismet/GameplayStatics.h"
#include "Components/OxiHumanDamageComponent.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"

DEFINE_LOG_CATEGORY(LogOxiAI);

/** Toggles onscreen AI debugging information */
TAutoConsoleVariable<int32> CVarCoverDebug(
	TEXT("oxi.coverdebug"),

	-1,
	TEXT("Show debug cover info. 0 Shows all.  Otherwise the # indicates a specific cover to display"),
	ECVF_Cheat
);

TAutoConsoleVariable<int32> CVarCoverSpotDebug(
	TEXT("oxi.coverspotdebug"),
	-1,
	TEXT("Show debug cover spot info. 0 Shows all.  Otherwise the # indicates a specific cover spor to display"),
	ECVF_Cheat
);

TAutoConsoleVariable<int32> CVarEnableAI(
	TEXT("oxi.enableai"),
	1,
	TEXT("AI will stop spawning if this flag is enabled"),
	ECVF_Cheat
);

/**
 *
 */
UOxiAIManager* GetOxiAIManager(UObject* const WorldContextObject)
{
	UGameInstance* const GameInst = UGameplayStatics::GetGameInstance(WorldContextObject);
	check(GameInst);

	return GameInst->GetSubsystem<UOxiAIManager>();
}

/**
 *
 */
AOxiCover* UOxiAIManager::FindNearestUnusedCover(const FVector& testPoint)
{
	const FVector2D testPt2D(testPoint.X, testPoint.Y);
	float closestCoverDistSqr = FLT_MAX;
	int closestCoverIdx = -1;
	for (int iCover = 0; iCover < CoverList.Num(); iCover++)
	{
		AOxiCover* const currentCover = CoverList[iCover];
		if (currentCover->GetNumUsers() > 0)
		{
			continue;
		}

		if (currentCover->GetCoverProtectionLevel() == EOxiCoverProtectionLevel::Broken)
		{
			continue;
		}

		const FVector2D currCoverPt2D(currentCover->GetActorLocation().X, currentCover->GetActorLocation().Y);
		const float coverDistSqr = FVector2D::DistSquared(currCoverPt2D, testPt2D);
		if (coverDistSqr < closestCoverDistSqr)
		{
			closestCoverIdx = iCover;
			closestCoverDistSqr = coverDistSqr;
		}
	}

	if (closestCoverIdx == -1)
	{
		return nullptr;
	}

	return CoverList[closestCoverIdx];
}

/**
 *
 */
void UOxiAIManager::FindCoverWithinRadius(TArray<AOxiCover*>& cover, const FVector& searchCenter, const float radius)
{
	check(cover.Num() == 0);

	const FVector2D searchCenter2D(searchCenter.X, searchCenter.Y);
	const float radiusSqr = radius * radius;

	for (int iCover = 0; iCover < CoverList.Num(); iCover++)
	{
		AOxiCover* const currentCover = CoverList[iCover];
		if (currentCover->GetNumUsers() > 0)
		{
			continue;
		}

		if (currentCover->GetCoverProtectionLevel() == EOxiCoverProtectionLevel::Broken)
		{
			continue;
		}

		const FVector2D actorLocation2D(currentCover->GetActorLocation().X, currentCover->GetActorLocation().Y);
		const float CoverDist = FVector2D::DistSquared(searchCenter2D, actorLocation2D);
		if (CoverDist < radiusSqr)
		{
			cover.Add(currentCover);
		}
	}
}

/**
 *
 */
bool AOxiAICharacter::IssueFutureAICommand(const FOxiAICommandData& CommandData, const float secondsInTheFuture)
{
	FutureAICommandTimerDel.BindUFunction(this, FName("DoFutureAICommandCallback"), CommandData);

	if (FutureAICommandTimerHandle.IsValid())
	{
		GetWorld()->GetTimerManager().ClearTimer(FutureAICommandTimerHandle);
	}
	GetWorld()->GetTimerManager().SetTimer(FutureAICommandTimerHandle, FutureAICommandTimerDel, secondsInTheFuture, false);

	return true;
}

/**
 *
 */
void AOxiAICharacter::DoFutureAICommandCallback(const FOxiAICommandData& CommandData)
{
	if (FutureAICommandTimerHandle.IsValid())
	{
		GetWorld()->GetTimerManager().ClearTimer(FutureAICommandTimerHandle);
	}
	IssueAICommand(CommandData);
}

/**
 *
 */
bool AOxiAICharacter::HasReachedDestination()
{
	AAIController* const AIController = Cast<AAIController>(GetController());
	check(AIController);

	const UPathFollowingComponent* const PathComponent = AIController->GetPathFollowingComponent();
	return PathComponent->DidMoveReachGoal();
}

/**
 *
 */
FVector AOxiAICharacter::GetPathDestination()
{
	AAIController* const AIController = Cast<AAIController>(GetController());
	check(AIController);

	const UPathFollowingComponent* const PathComponent = AIController->GetPathFollowingComponent();
	return PathComponent->GetPathDestination();
}


/**
 * 
 */
AOxiCover* AOxiAICharacter::FindAndAcquireCover(AActor* const Attacker, const FVector searchLocation, const float searchRadius)
{
	if (Attacker == nullptr)
	{
		return nullptr;
	}

	UOxiAIManager* const aiMgr = GetOxiAIManager(this);
	AOxiCover* coverToAcquire = nullptr;
	if (searchRadius <= 0.0f)
	{
		coverToAcquire = aiMgr->FindNearestUnusedCover(GetActorLocation());
	}
	else
	{
		if (CVarCoverDebug.GetValueOnGameThread() > 0)
		{
			FVector upOffset(0.0f, 0.0f, 16.0f);
			DrawDebugSphere(GetWorld(), searchLocation + upOffset, 16.0f, 16, FColor(255, 0, 255, 255), false, 5.0f);
			DrawDebugCircle(GetWorld(), searchLocation + upOffset, searchRadius, 32, FColor(255, 0, 255, 255), false, 5.0f, 0U, 1.0f, FVector(0.0f, 1.0f, 0.0f), FVector(1.0f, 0.0f, 0.0f));
		}

		TArray<AOxiCover*> validCover;
		aiMgr->FindCoverWithinRadius(validCover, searchLocation, searchRadius);
		if (validCover.Num() > 0)
		{
			coverToAcquire = validCover[rand() % validCover.Num()];
		}
	}

	if (coverToAcquire == nullptr)
	{
		return nullptr;
	}

	AcquireCover(coverToAcquire, Attacker->GetActorLocation());

	return coverToAcquire;
}

/**
 *
 */
void AOxiAICharacter::OnCoverProtectionLevelChanged(AOxiCover* const Cover, EOxiCoverProtectionLevel NewProtectionLevel)
{
	check(Cover);
	if (NewProtectionLevel == EOxiCoverProtectionLevel::Broken)
	{
		// Cover is broken. Find new cover
		ReleaseCover();
		AOxiCover* const NearestCover = FindAndAcquireCover(CurrentAICommand.Target);
		if (NearestCover != nullptr)
		{
			// Take Cover
			FOxiAICommandData AICommandData;
			AICommandData.AICommand = EOxiAICommand::TakeCover;
			AICommandData.Target = NearestCover;// CurrentAICommand.Target;
//			AICommandData.Goal = NearestCover;

			IssueAICommand(AICommandData);
		}
	}
}

/**
 *
 */
void UOxiAIManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	CurrentCoverToTrace = 0;
}

/**
 * 
 */
void UOxiAIManager::RegisterSquad(AOxiSquad* const Squad)
{
	SquadList.Add(Squad);
}

/**
 * 
 */
void UOxiAIManager::UnregisterSquad(AOxiSquad* const Squad)
{
	SquadList.Remove(Squad);
}

/**
 *
 */
uint32 UOxiAIManager::RegisterCover(AOxiCover* const Squad)
{
	CoverList.Add(Squad);
	
	uint32 coverIdx = NextCoverIndex;
	NextCoverIndex++;
	return coverIdx;
}

/**
 *
 */
void UOxiAIManager::UnregisterCover(AOxiCover* const Squad)
{
	CoverList.Remove(Squad);
}

/**
 *
 */
void UOxiAIManager::RegisterPlayer(AOxiFirstPersonCharacter* const Player)
{
	PlayerList.Add(Player);
}

/**
 *
 */
void UOxiAIManager::UnregisterPlayer(AOxiFirstPersonCharacter* const Player)
{
	PlayerList.Remove(Player);
}

/**
 *
 */
void UOxiAIManager::Tick(float DeltaTime)
{
	if (PlayerList.Num() == 0)
	{
		return;
	}

	CurrentCoverVisTest++;
	if (CurrentCoverVisTest > CoverList.Num())
	{
		CurrentCoverVisTest = 1;
	}

	// Debug Drawing
	DrawDebugInfo();

	DebugDrawPlayerCells();
}

/**
 *
 */
TStatId UOxiAIManager::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UOxiAIManager, STATGROUP_Tickables);
}

/**
 *
 */
void UOxiAIManager::DrawDebugInfo()
{
}

/**
 *
 */
void UOxiAIManager::DebugDrawPlayerCells()
{
	if (CVarCoverDebug.GetValueOnGameThread() < 0)
	{
		return;
	}

	if (PlayerList.Num() == 0)
	{
		return;
	}

	// Draw Player Cells
	const FVector basePos = PlayerList[0]->GetActorLocation();
	const int xIdx = ((int)basePos.X);
	const int yIdx = ((int)basePos.Y);

	const float startOffset = -((VisCellWidth * HalfVisCellsAcross) - (VisCellWidth * 0.5f));
	
	for (int y = 0; y < VisCellsAcross; y++)
	{
		const float yPos = basePos.Y + startOffset + (y * VisCellWidth);
		for (int x = 0; x < VisCellsAcross; x++)
		{
			const float xPos = basePos.X + startOffset + (x * VisCellWidth);

			UE_LOG(LogTemp, Log, TEXT("[%f %f %f] -- [%f %f %f]"), basePos.X, basePos.Y, basePos.Z, xPos, yPos, basePos.Z);

			DrawDebugBox(GetWorld(), FVector(xPos, yPos, basePos.Z), FVector(HalfVisCellWidth, HalfVisCellWidth, 0), FColor::Green, false, -1.0f, 0, 0.35f);
		}
	}
}