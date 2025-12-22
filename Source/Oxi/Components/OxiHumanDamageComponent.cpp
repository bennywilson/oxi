// ELP 2021

#include "OxiHumanDamageComponent.h"
#include "OxiCharacter.h"
#include "TimerManager.h"
#include "Engine/PostProcessVolume.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Components/CapsuleComponent.h"

/** Toggles god mode */
static TAutoConsoleVariable<int32> CVarGodMode(
	TEXT("oxi.god"),
	0,
	TEXT("Toggles god mode"),
	ECVF_Cheat
);

/**
 *
 */ 
void UOxiHumanDamageComponent::BeginPlay()
{
	Super::BeginPlay();

	TArray<USceneComponent*> Children;
	GetChildrenComponents(true, Children);

	for (int i = 0; i < Children.Num(); i++)
	{
		USkeletalMeshComponent* const SkelMesh = Cast<USkeletalMeshComponent>(Children[i]);
		if (SkelMesh == nullptr)
		{
			continue;
		}

		SkelMesh->SetSimulatePhysics(false);
		SkelMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		SkelMesh->SetAllBodiesPhysicsBlendWeight(0.f);
	}
}

/**
 *
 */
float UOxiHumanDamageComponent::TakeDamage(const FOxiDamageInfo& DamageInfo)
{
	Super::TakeDamage(DamageInfo);

	float DamageAmount = DamageInfo.DamageAmount;
	AOxiCharacter* Victim = Cast<AOxiCharacter>(GetOwner());
	if (Victim != nullptr)
	{
		const float* const damageMultiplier = Victim->GetHitBoneToDamageMultiplier().Find(DamageInfo.HitBoneName);
		if (damageMultiplier != nullptr)
		{
			DamageAmount *= *damageMultiplier;
		}
	}
	const float OldHealth = CurrentHealth;
	CurrentHealth -= DamageAmount;
	bool bJustKilled = (OldHealth > 0 && CurrentHealth <= 0);

	const float UnpausedTimeSec = GetWorld()->GetUnpausedTimeSeconds();

	TArray<USceneComponent*> AllChildren;
	GetChildrenComponents(true, AllChildren);

	static float Scalar = 350.f;
	FVector Impulse(0.0f, 0.0f, 0.0f);

	if (DamageInfo.DamageCauser != nullptr)
	{
		Impulse = (DamageInfo.DamageLocation - DamageInfo.DamageCauser->GetActorLocation());
		Impulse.Z = 0;
		Impulse = Impulse.GetSafeNormal() * DamageInfo.DamageXYImpulse + FVector::UpVector * DamageInfo.DamageZImpulse;
	}

	bool bAddedWound = false;

	// Bloodspray
	if (BloodSplatter.Num() > 0)
	{
		const int SplatterIdx = FMath::RandRange(0, BloodSplatter.Num() - 1);
		FOxiBloodSplatterData& Splatter = BloodSplatter[SplatterIdx];
		if (Splatter.SplatterActor.Num() > 0)
		{
			const int ActorIdx = FMath::RandRange(0, Splatter.SplatterActor.Num() - 1);
			TSubclassOf<AActor> SplatterActor = Splatter.SplatterActor[ActorIdx];
			if (*SplatterActor != nullptr)
			{
				const FVector TraceDirection = (DamageInfo.DamageLocation - DamageInfo.DamageCauser->GetActorLocation()).GetSafeNormal() * Splatter.BloodSprayDistance;
				FHitResult HitResult;
				FCollisionObjectQueryParams ObjectType(ECollisionChannel::ECC_WorldStatic);
				const bool bHit = GWorld->LineTraceSingleByObjectType(HitResult, DamageInfo.DamageLocation, DamageInfo.DamageLocation + TraceDirection, ObjectType);
				if (bHit)
				{
					FRotator ImpactNormalRotation = HitResult.ImpactNormal.ToOrientationRotator();
					ImpactNormalRotation = FRotationMatrix(ImpactNormalRotation).GetScaledAxis(EAxis::Z).ToOrientationRotator();

					GWorld->SpawnActor(SplatterActor, &HitResult.Location, &ImpactNormalRotation);
				}
			}
		}
	}

	if (bJustKilled)
	{
		for (int i = 0; i < SkeletalMeshes.Num(); i++)
		{
			USkeletalMeshComponent* const SkelMesh = SkeletalMeshes[i];

			if (DamageInfo.DamageWeapon && !bAddedWound && WoundInstances.Num() < 1)
			{
				bAddedWound = true;
				for (int iWoundSearch = 0; iWoundSearch < WoundData.Num(); iWoundSearch++)
				{
					const FWoundData& CurWoundData = WoundData[iWoundSearch];

					if (!(CurWoundData.WoundLevel | DamageInfo.DamageWeapon->GetWoundLevel()))
					{
						continue;
					}

					static const FName AnyBone = { "Any" };
					if (CurWoundData.BoneName != AnyBone && CurWoundData.BoneName != DamageInfo.HitBoneName)
					{
						continue;
					}
			
					FWoundInstance WoundInfo;
					WoundInfo.BoneName = DamageInfo.HitBoneName;
					WoundInfo.WoundIndex = iWoundSearch;
					WoundInfo.HitLocation = SkelMesh->GetBoneLocation(DamageInfo.HitBoneName, EBoneSpaces::ComponentSpace);
					WoundInfo.HitTime = UnpausedTimeSec;
					WoundInstances.Add(WoundInfo);
					bAddedWound = true;

					const FWoundFXData& WoundFX = CurWoundData.WoundFX;
					if (WoundFX.AttachSocket != NAME_None && WoundFX.DismembermentFX != nullptr)
					{
						AActor* const FXActor = GetWorld()->SpawnActor(WoundFX.DismembermentFX, &GetComponentTransform());
						FXActor->AttachToComponent(SkelMesh, FAttachmentTransformRules::SnapToTargetIncludingScale, WoundFX.AttachSocket);

						for (int iComponentName = 0; iComponentName < WoundFX.TagsOfComponentsToEnable.Num(); iComponentName++)
						{
							const FName ComponentTag = WoundFX.TagsOfComponentsToEnable[iComponentName];
							if (ComponentTag == NAME_None)
							{
								continue;
							}

							for (int iCap = 0; iCap < AllChildren.Num(); iCap++)
							{
								if (AllChildren[iCap]->ComponentHasTag(ComponentTag))
								{
									AllChildren[iCap]->SetVisibility(true);
									break;
								}
							}

						}
					}

					const FWoundFXData& GibFX = CurWoundData.GibFX;
					if (GibFX.AttachSocket != NAME_None)
					{
						if (GibFX.DismembermentFX != nullptr)
						{
							FVector SocketLocation;
							FQuat SocketRotation;
							SkelMesh->GetSocketWorldLocationAndRotation(GibFX.AttachSocket, SocketLocation, SocketRotation);

							FTransform SocketTransform(SocketRotation, SocketLocation);
							AActor* const FXActor = GetWorld()->SpawnActor(GibFX.DismembermentFX, &SocketTransform);
							UStaticMeshComponent* SM = Cast<UStaticMeshComponent>(FXActor->GetComponentByClass(UStaticMeshComponent::StaticClass()));
							if (SM != nullptr)
							{
								SM->AddImpulse(Impulse * 2.0f, NAME_None, true);
							}
						}
					}
					break;
				}
			}

			static const FName ClipParams[] = { "WoundClip1_Params", "WoundClip2_Params"};
			static const FName BoneParams[] = { "Wound1_Params", "Wound2_Params" };

			for (int iBone = 0; iBone < WoundInstances.Num(); iBone++)
			{
				const FWoundInstance& WoundInst = WoundInstances[iBone];
				const FWoundData& CurWoundData = WoundData[WoundInst.WoundIndex];

				FLinearColor DamageBoneParam;
				DamageBoneParam.R = WoundInst.HitLocation.X;
				DamageBoneParam.G = WoundInst.HitLocation.Y;
				DamageBoneParam.B = WoundInst.HitLocation.Z;
				DamageBoneParam.A = CurWoundData.MaterialWoundRadius;

				if (DamageBoneParam.A > 0.0f || CurWoundData.ClipSphereLocationAndRadius.W > 0.0f)
				{
					const TArray<UMaterialInterface*> MaterialInterfaces = SkelMesh->GetMaterials();
					for (int32 MaterialIndex = 0; MaterialIndex < MaterialInterfaces.Num(); ++MaterialIndex)
					{
						UMaterialInterface* MaterialInterface = MaterialInterfaces[MaterialIndex];
						if (MaterialInterface)
						{
							UMaterialInstanceDynamic* DynamicMaterial = Cast<UMaterialInstanceDynamic>(MaterialInterface);
							if (!DynamicMaterial)
							{
								DynamicMaterial = SkelMesh->CreateAndSetMaterialInstanceDynamic(MaterialIndex);
							}

							if (DamageBoneParam.A > 0.0f)
							{
								DynamicMaterial->SetVectorParameterValue(BoneParams[iBone], DamageBoneParam);
							}

							if (CurWoundData.ClipSphereLocationAndRadius.W > 0.0f)
							{
								DynamicMaterial->SetVectorParameterValue(ClipParams[iBone], FLinearColor(CurWoundData.ClipSphereLocationAndRadius));
							}
						}
					}
				}

				if (CurWoundData.BoneConstraintToBreak != NAME_None)
				{
					SkelMesh->BreakConstraint(Impulse * 20.0f, WoundInst.HitLocation, CurWoundData.BoneConstraintToBreak);
				}
			}

			// Physics
			if (bRagdolling == false)
	 		{
				bRagdolling = true;
				SkelMesh->SetSimulatePhysics(true);
				SkelMesh->SetAllBodiesPhysicsBlendWeight(1.f);
				SkelMesh->AddImpulse(Impulse, DamageInfo.HitBoneName, true);

				SkelMesh->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Ignore);
				Victim->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				GetOwner()->GetWorldTimerManager().SetTimer(RagdollSleepTimerHandle, this, &UOxiDamageComponent::DisableRagdoll, 5.0f, true, 5.0f);
			}		
		}
	}

	if (bJustKilled)
	{
		OnDeath.Broadcast(this, GetOwner(), DamageInfo.DamageCauser);
	}
	else
	{
		OnTakeDamage.Broadcast(GetOwner(), DamageInfo);
	}

	return 0.f;
}

/**
 *
 */
void UOxiPlayerDamageComponent::BeginPlay()
{
	Super::BeginPlay();

	for (IInterface_PostProcessVolume* PPVolume : GetWorld()->PostProcessVolumes)
	{
		APostProcessVolume* const curVol = Cast<APostProcessVolume>(PPVolume);
		if (curVol == nullptr)
		{
			continue;
		}

		FPostProcessSettings& ppSettings = curVol->Settings;

		for (FWeightedBlendable& weightedBlendable : ppSettings.WeightedBlendables.Array)
		{
			UMaterialInstance* const MatInst = Cast<UMaterialInstance>(weightedBlendable.Object);
			if (MatInst == nullptr)
			{
				continue;
			}

			if (MatInst->GetName().Contains("PlayerDamage_PP_Inst"))
			{
				PlayerDamagePP_MatInst = UKismetMaterialLibrary::CreateDynamicMaterialInstance(this, MatInst);
				weightedBlendable.Object = PlayerDamagePP_MatInst;//TObjectPtr<UObject>(PlayerDamagePP_MatInst);
				continue;
			}
		}
	}
}

/**
 *
 */
float UOxiPlayerDamageComponent::TakeDamage(const FOxiDamageInfo& DamageInfo)
{
	if (CVarGodMode.GetValueOnGameThread() > 0)
	{
		return 0.0f;
	}

	if (CurrentHealth <= 0)
	{
		return 0.0f;
	}

	CurrentHealth -= DamageInfo.DamageAmount;

	if (CurrentHealth <= 0.0f)
	{
		OnDeath.Broadcast(this, GetOwner(), DamageInfo.DamageCauser);
	}

	OnTakeDamage.Broadcast(GetOwner(), DamageInfo);

	LastDamageTime = GetWorld()->GetTimeSeconds();
	return 0.0f;
}

/**
 *
 */
void UOxiPlayerDamageComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const float ScreenBloodIntensity = FMath::Clamp((BaseHealth - CurrentHealth) / (BaseHealth * 0.9f), 0.0f, 1.0f);// * 0.5f + 0.5f;
	if (PlayerDamagePP_MatInst != nullptr)
	{
		PlayerDamagePP_MatInst->SetScalarParameterValue("FXIntensity", ScreenBloodIntensity);
	}

	const float curTime = GetWorld()->GetTimeSeconds();
	if (CurrentHealth > 0 && CurrentHealth < BaseHealth && HealthRegenRate > 0.0f)
	{
		if (curTime > LastDamageTime + SecondsUntilHealthRegen)
		{
			CurrentHealth = CurrentHealth + HealthRegenRate * DeltaTime;
			if (CurrentHealth > BaseHealth)
			{
				CurrentHealth = BaseHealth;
			}
		}
	}
}
