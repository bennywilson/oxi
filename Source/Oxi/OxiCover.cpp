// ELP 2022

#include "OxiCover.h"
#include "Components/OxiDestructibleComponent.h"
#include "OxiAIManager.h"


/**
 *
 */
void UOxiCoverSpotComponent::BeginPlay()
{
	Super::BeginPlay();

	const FVector ShootHeight(0.0f, 0.0f, 170.0f);
	const float ShootRightDistFromCoverSpot = 55.0f;
	const float ShootForwardDistFromCoverSpot = 55.0f;

	const FVector coverSpotLocation = GetComponentLocation();
	const FVector coverSpotDirection = GetComponentRotation().Vector();
	const FVector coverSpotPerp = coverSpotDirection.Cross(FVector(0.0f, 0.0f, 1.0f)).GetSafeNormal();
	// .707105 0.707108 0


	LeanLeftWorldFirePoint = coverSpotLocation + ShootHeight + coverSpotPerp * ShootRightDistFromCoverSpot + FVector(coverSpotDirection.X, coverSpotDirection.Y, 0.0f) * ShootForwardDistFromCoverSpot;
	LeanRightWorldFirePoint = coverSpotLocation + ShootHeight + -coverSpotPerp * ShootRightDistFromCoverSpot + FVector(coverSpotDirection.X, coverSpotDirection.Y, 0.0f) * ShootForwardDistFromCoverSpot;

	LeftLeanVisibilityToTarget = 0;
	RightLeanVisibilityToTarget = 0;
}

/**
 *
 */
AOxiCover::AOxiCover()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	//RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultSceneRoot"));
	//SetRootComponent(NewSceneComponent);

	DestructibleComponent = CreateDefaultSubobject<UOxiDestructibleComponent>(TEXT("DestructibleComponent"));
	//DestructibleComponent->SetupAttachment(NewSceneComponent);
	SetRootComponent(DestructibleComponent);
		
	UndamagedMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("UndamagedMesh"));
	UndamagedMesh->SetupAttachment(DestructibleComponent);

	DamagedMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("DamagedMesh"));
	DamagedMesh->SetupAttachment(DestructibleComponent);
	DamagedMesh->SetCollisionProfileName(FName("BlockAllDynamic"));
}

/**
 *
 */
void AOxiCover::BeginPlay()
{
	Super::BeginPlay();
	
	DestructibleComponent->InitDestructibleComponent(UndamagedMesh, DamagedMesh);

	TakeDamageDelegate.BindUFunction(this, FName("OnDestructibleTakeDamage"));
	DestructibleComponent->OnDestructibleKilled.Add(TakeDamageDelegate);

	CoverIndex = GetOxiAIManager(this)->RegisterCover(this);

	for (int i = 0; i < CoverSpotList.Num(); i++)
	{
		UOxiCoverSpotComponent* const coverSpot = Cast<UOxiCoverSpotComponent>(CoverSpotList[i].GetComponent(this));
		if (coverSpot == nullptr)
		{
			continue;
		}

		CoverSpots.Add(coverSpot);
	}
}

/**
 *
 */
void AOxiCover::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	UOxiAIManager* const AIMgr = GetOxiAIManager(this);
	if (AIMgr != nullptr)
	{
		AIMgr->UnregisterCover(this);
	}
	DestructibleComponent->OnTakeDamage.RemoveAll(this);
}

/**
 *
 */
void AOxiCover::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateVisTraces();

	DebugDrawCoverInfo();
}

/**
 *
 */
bool AOxiCover::AddUser(AOxiCharacter* const NewUser)
{
	// Only one user per cover for now
	//ensure(CurrentUsers.Num() == 0);
	
	CurrentUsers.Add(NewUser);
	OnProtectionLevelChanged.AddUObject(NewUser, &AOxiCharacter::OnCoverProtectionLevelChanged);

	return true;
}

/**
 * 
 */
void AOxiCover::RemoveUser(AOxiCharacter* const UserToRemove)
{
	CurrentUsers.Remove(UserToRemove);
	OnProtectionLevelChanged.RemoveAll(UserToRemove);
}

/**
 *
 */
void AOxiCover::OnDestructibleTakeDamage(AActor* const DamageCauser, float DamageAmount)
{
	if (ProtectionLevel == EOxiCoverProtectionLevel::Unbreakable)
	{
		return;
	}

	if (ProtectionLevel != EOxiCoverProtectionLevel::Broken)
	{
		ProtectionLevel = EOxiCoverProtectionLevel::Broken;
		OnProtectionLevelChanged.Broadcast(this, EOxiCoverProtectionLevel::Broken);
	}
}

/**
 *
 */
UOxiCoverSpotComponent* AOxiCover::GetBestCoverSpot(const FVector TargetPosition)
{
	const FVector CoverToTargetVec = (TargetPosition - GetActorLocation()).GetSafeNormal();
	float closestVecMatch = -999.0f;
	UOxiCoverSpotComponent* BestCoverSpot = nullptr;
	for (int i = 0; i < CoverSpotList.Num(); i++)
	{
		UOxiCoverSpotComponent* const CurCoverSpot = Cast<UOxiCoverSpotComponent>(CoverSpotList[i].GetComponent(this));
		float dot = CurCoverSpot->GetComponentRotation().Vector().Dot(CoverToTargetVec);
		if (dot > closestVecMatch)
		{
			BestCoverSpot = CurCoverSpot;
			closestVecMatch = dot;
		}
	}

	return BestCoverSpot;
}

/**
 *
 */
void AOxiCover::UpdateVisTraces()
{
	const int coverDebug = CVarCoverDebug.GetValueOnGameThread();
	const int coverSpotDebug = CVarCoverSpotDebug.GetValueOnGameThread();
	const bool bDebugDraw = coverDebug == 0 || coverDebug == CoverIndex;

	UOxiAIManager* const aiMgr = GetOxiAIManager(this);
	auto PlayerList = aiMgr->GetPlayerList();
	if (PlayerList.Num() == 0)
	{
		return;
	}

	TArray<AActor*> actorsToIgnore;
	actorsToIgnore.Add(PlayerList[0]);

	// Get Last Frame
	for (int iCoverSpot = 0; iCoverSpot < CoverSpotList.Num(); iCoverSpot++)
	{
		UOxiCoverSpotComponent* const curCoverSpot = CoverSpots[iCoverSpot];
		TArray<FDebugTraceData>& debugTraceDataList = curCoverSpot->GetDebugTraceDataList();
		auto& visiblityTraces = curCoverSpot->GetVisibilityHandles();
		if (visiblityTraces.Num() == 0)
		{
			if (bDebugDraw && (coverSpotDebug == 0 || coverSpotDebug - 1 == iCoverSpot))
			{
				for (int iDebugTrace = 0; iDebugTrace < debugTraceDataList.Num(); iDebugTrace++)
				{
					const FDebugTraceData& debugTraceData = debugTraceDataList[iDebugTrace];
					if (debugTraceData.HitSomething)
					{

						DrawDebugLine(GetWorld(), debugTraceData.Start, debugTraceData.End, FColor::Red, false, -1.0f, 0, 0.32f);
					}
					else
					{
						DrawDebugLine(GetWorld(), debugTraceData.Start, debugTraceData.End, FColor::Blue, false, -1.0f, 0, 0.32f);
					}
				}
			}

			continue;
		}

		const int32 halfNumTraces = visiblityTraces.Num() / 2;

		float leftLeanVis = 0;
		float rightLeanVis = 0;
		for (int32 iTrace = 0; iTrace < visiblityTraces.Num(); iTrace++)
		{
			FTraceDatum traceData;
			const bool bTraceValid = GetWorld()->QueryTraceData(visiblityTraces[iTrace], traceData);
			if (bTraceValid == false)
			{
				continue;
			}

			FDebugTraceData debugTraceData;
			debugTraceData.Start = traceData.Start;

			if (traceData.OutHits.Num() == 0)
			{
				if (iTrace < halfNumTraces)
				{
					leftLeanVis += 1.0f;
				}
				else
				{
					rightLeanVis += 1.0f;
				}

				debugTraceData.End = traceData.End;
				debugTraceData.HitSomething = false;
			}
			else
			{
				debugTraceData.HitSomething = true;
				debugTraceData.End = traceData.OutHits[0].Location;
			}

			debugTraceDataList.Add(debugTraceData);
		}

		curCoverSpot->SetLeftLeanVisibilityToTarget(leftLeanVis / halfNumTraces);
		curCoverSpot->SetRightLeanVisibilityToTarget(rightLeanVis / halfNumTraces);
		visiblityTraces.Empty();
	}

	if (CoverIndex != aiMgr->GetCurrentCoverVisTest())
	{
		return;
	}

	// Draw Player Cells
	FVector basePos = PlayerList[0]->GetActorLocation();

	const int32 visTileWidth = UOxiAIManager::VisCellWidth;
	const int32 halfNumCellsAcross = UOxiAIManager::HalfVisCellWidth;

	// Queue up async line traces
	for (int iCoverSpot = 0; iCoverSpot < CoverSpotList.Num(); iCoverSpot++)
	{
		UOxiCoverSpotComponent* const curCoverSpot = CoverSpots[iCoverSpot];
		TArray<FDebugTraceData>& debugTraceDataList = curCoverSpot->GetDebugTraceDataList();
		debugTraceDataList.Empty();

		float leftLeanVis = 0;
		float rightLeanVis = 0;

		const FVector firePositions[] = { curCoverSpot->GetLeanLeftFirePoint(), curCoverSpot->GetLeanRightFirePoint() };

		for (int iLean = 0; iLean < 2; iLean++)
		{
			const float startOffset = -((UOxiAIManager::VisCellWidth * UOxiAIManager::HalfVisCellsAcross) - (UOxiAIManager::VisCellWidth * 0.5f));

			for (int y = 0; y < UOxiAIManager::VisCellsAcross; y++)
			{
				const float yPos = basePos.Y + startOffset + (y * UOxiAIManager::VisCellWidth);
				for (int x = 0; x < UOxiAIManager::VisCellsAcross; x++)
				{
					const float xPos = basePos.X + startOffset + (x * UOxiAIManager::VisCellWidth);

					FHitResult hitResult;
					FCollisionQueryParams params = FCollisionQueryParams::DefaultQueryParam;
					params.AddIgnoredActors(actorsToIgnore);
					const FVector cellPos = FVector(xPos, yPos, basePos.Z);

					FTraceHandle traceHandle = GetWorld()->AsyncLineTraceByChannel(EAsyncTraceType::Single, firePositions[iLean], cellPos, ECollisionChannel::ECC_Visibility, params);
					if (traceHandle.IsValid() == false)
					{
						continue;
					}

					curCoverSpot->GetVisibilityHandles().Add(traceHandle);
				}
			}
		}
	}
}

/**
 *
 */
void AOxiCover::DebugDrawCoverInfo()
{
	const int coverDebug = CVarCoverDebug.GetValueOnGameThread();
	const int coverSpotDebug = CVarCoverSpotDebug.GetValueOnGameThread();
	const bool bDebugDraw = coverDebug == 0 || coverDebug == CoverIndex;
	if (bDebugDraw == false)
	{
		return;
	}

	const float debugSphereRadius = 10.0f;
	const float debugArrowLength = 20;
	const float debugArrowThickness = 2.0f;
	const float debugArrowSize = 15.0f;

	for (int i = 0; i < CoverSpotList.Num(); i++)
	{
		if (coverSpotDebug != 0 && coverSpotDebug != i + 1)
		{
			continue;
		}

		UOxiCoverSpotComponent* const coverSpot = Cast<UOxiCoverSpotComponent>(CoverSpotList[i].GetComponent(this));
		if (coverSpot == nullptr)
		{
			continue;
		}

		const FVector coverSpotLocation = coverSpot->GetComponentLocation();
		const FVector coverSpotDirection = coverSpot->GetComponentRotation().Vector();

		DrawDebugSphere(GetWorld(), coverSpotLocation, debugSphereRadius, 16, FColor::Green);
		DrawDebugDirectionalArrow(GetWorld(), coverSpotLocation, coverSpotLocation + coverSpotDirection * debugArrowLength, debugArrowSize, FColor::Red, false, -1.0f, 0, debugArrowThickness);

		if (coverDebug == 0)
		{
			continue;
		}

		FColor sphereColor = FMath::Lerp(FLinearColor::Red, FLinearColor::Green, coverSpot->GetLeftLeanVisibilityToTarget()).ToFColor(false);
		DrawDebugSphere(GetWorld(), coverSpot->GetLeanLeftFirePoint(), debugSphereRadius, 16, sphereColor);

		sphereColor = FMath::Lerp(FLinearColor::Red, FLinearColor::Green, coverSpot->GetRightLeanVisibilityToTarget()).ToFColor(false);
		DrawDebugSphere(GetWorld(), coverSpot->GetLeanRightFirePoint(), debugSphereRadius, 16, sphereColor);
	}
}

/**
 *
 */
UOxiCoverSpotComponent::UOxiCoverSpotComponent(FObjectInitializer const& objecttInitializer)
	: UArrowComponent(objecttInitializer)
{

}
