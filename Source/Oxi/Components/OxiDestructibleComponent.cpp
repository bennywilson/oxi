// ELP 2020
#include "OxiDestructibleComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "Components\LightComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Particles\ParticleSystemComponent.h"

/**
 * 
 */
UOxiDestructibleComponent::UOxiDestructibleComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

/**
 *
 */
void UOxiDestructibleComponent::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();

	ThresholdToShowDamagedMeshAt = FMath::Min(ThresholdToShowDamagedMeshAt, Health);
}

/**
 *
 */
bool UOxiDestructibleComponent::InitDestructibleComponent(UStaticMeshComponent* InBaseMeshComponent, USkeletalMeshComponent* InDestructibleMeshComponent)
{
	BaseMeshComponent = InBaseMeshComponent;
	DestructibleMeshComponent = InDestructibleMeshComponent;

	if (BaseMeshComponent != nullptr)
	{
		BaseMeshComponent->SetHiddenInGame(false, true);

		if (DestructibleMeshComponent != nullptr)
		{
			DestructibleMeshComponent->SetHiddenInGame(true, true);
			DestructibleMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}

	}
	else
	{
		if (DestructibleMeshComponent != nullptr)
		{
			DestructibleMeshComponent->SetHiddenInGame(false);
			DestructibleMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			DestructibleMeshComponent->SetSimulatePhysics(true);
			DestructibleMeshComponent->SetAllBodiesSimulatePhysics(false);
		}

	}

	if (DestructibleMeshComponent == nullptr)
	{
		static int breakhere = 0;
		breakhere++;
	}
	
	return true;
}

/**
 *
 */
void UOxiDestructibleComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (DestructibleMeshComponent != nullptr && DestructibleMeshComponent->IsVisible())
	{
		UMaterialInstanceDynamic* const Mat = Cast<UMaterialInstanceDynamic>(DestructibleMeshComponent->GetMaterial(1));		// TODO: Unhardcode this
		if (Mat)
		{
			const float t = FMath::Clamp((GetWorld()->GetUnpausedTimeSeconds() - SmearStartTime) / SmearLengthSec, 0.0f, 1.0f);
			Mat->SetScalarParameterValue("SmearStrength", SmearStartStrength * (1.0f - t));
		}

		if (ExplosionLightComponent != nullptr)
		{
			const float t = FMath::Clamp((GetWorld()->GetUnpausedTimeSeconds() - SmearStartTime) / ExplosionLightDurationSec, 0.0f, 1.0f);
			const float lightMultiplier = 1.0f - t;	// Goes from off, on, off
			ExplosionLightComponent->SetIntensity(lightMultiplier * ExplosionLightTargetIntensity);
		}
	}
}

/**
 *
 */
float UOxiDestructibleComponent::TakeDamage(const FOxiDamageInfo& DamageInfo)
{
	const float OldHealth = Health;
	Health -= DamageInfo.DamageAmount;
	
	// Swap Meshes
	if (OldHealth > ThresholdToShowDamagedMeshAt && Health <= FMath::Max(0.0f, ThresholdToShowDamagedMeshAt) && DestructibleMeshComponent != nullptr)
	{
		if (BaseMeshComponent != nullptr)
		{ 
			BaseMeshComponent->SetHiddenInGame(true);
			BaseMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			BaseMeshComponent->SetGenerateOverlapEvents(false);
		}

		if (DestructibleMeshComponent != nullptr)
		{
			DestructibleMeshComponent->SetHiddenInGame(false);
			DestructibleMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			DestructibleMeshComponent->SetSimulatePhysics(true);
			DestructibleMeshComponent->SetAllBodiesSimulatePhysics(false);
		}
	}

	// Knock individual bodies off
	if (DamageInfo.HitBoneName != NAME_None)
	{
		if (HideBodiesWhenKnockedOff)
		{
			DestructibleMeshComponent->HideBoneByName(DamageInfo.HitBoneName, EPhysBodyOp::PBO_None);

//			DestructibleMeshComponent->SetAllBodiesBelowSimulatePhysics(DamageInfo.HitBoneName, true, true);
	//		FVector ImpulseDir = (DamageInfo.DamageLocation - DestructibleMeshComponent->Bounds.Origin).GetSafeNormal() * DamageInfo.DamageXYImpulse;
		//	DestructibleMeshComponent->AddImpulseToAllBodiesBelow(ImpulseDir, DamageInfo.HitBoneName, true, true);
		}
		else
		{
			DestructibleMeshComponent->SetAllBodiesBelowSimulatePhysics(DamageInfo.HitBoneName, true, true);
			FVector ImpulseDir = (DamageInfo.DamageLocation - DestructibleMeshComponent->Bounds.Origin).GetSafeNormal() * DamageInfo.DamageXYImpulse;
			DestructibleMeshComponent->AddImpulseToAllBodiesBelow(ImpulseDir, DamageInfo.HitBoneName, true, true);
		}

		if (BodyKnockOffFX.Num() > 0)
		{
			const int fxIdx = FMath::RandRange(0, BodyKnockOffFX.Num() - 1);
			auto fxToPlay = BodyKnockOffFX[fxIdx];
			if (fxToPlay != nullptr)
			{
				FTransform damageLoc;
				damageLoc.SetLocation(DamageInfo.DamageLocation);
				damageLoc.SetRotation(DamageInfo.ImpactNormal.ToOrientationQuat());
				GetWorld()->SpawnActor(fxToPlay, &damageLoc);
			}
		}
		NumBrokenPieces++;
	}

	if (OldHealth > 0.0f && Health <= 0.0f)
	{
		OnDestructibleKilled.Broadcast(DamageInfo.DamageCauser, DamageInfo.DamageAmount);

		if (ExplosionSound != nullptr)
		{
			UGameplayStatics::PlaySoundAtLocation(this, ExplosionSound, GetComponentTransform().GetLocation());
		}

		if (DestructibleMeshComponent != nullptr)
		{
			NumBrokenPieces = -1;
			SmearStartTime = GetWorld()->GetUnpausedTimeSeconds();
			DestructibleMeshComponent->SetSimulatePhysics(true);
			DestructibleMeshComponent->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
			if (DisableCollisionWhenDead)
			{
				DestructibleMeshComponent->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
				DestructibleMeshComponent->SetCollisionResponseToChannel(ECollisionChannel::ECC_WorldStatic, ECollisionResponse::ECR_Block);
			}

			const FVector ExplosionLocation = DestructibleMeshComponent->GetSocketLocation("ExplosionLocation");

			for (int iBodies = 0; iBodies < DestructibleMeshComponent->Bodies.Num(); iBodies++)
			{
				FBodyInstance* BI = DestructibleMeshComponent->Bodies[iBodies];
				if (BI->GetBodySetup()->CollisionReponse == EBodyCollisionResponse::BodyCollision_Disabled)
				{
					BI->SetPhysicsDisabled(true);
					continue;
				}

				bool bSkipBody = false;
				for (int i = 0; i < BodiesToSkip.Num(); i++)
				{
					if (BodiesToSkip[i] == iBodies) 
					{
						bSkipBody = true;
						break;
					}
			
				}
				if (bSkipBody)
				{
					BI->SetPhysicsDisabled(true);
					continue;
				}

				const float XYImpulse = FMath::RandRange(ExplosionXYImpulseMin, ExplosionXYImpulseMax);
				const FVector XAmount = XYImpulse * DestructibleMeshComponent->GetComponentTransform().TransformFVector4(FVector4(1.0f, 0.0f, 0.0f, 1.0f));
				const FVector YAmount = XYImpulse * DestructibleMeshComponent->GetComponentTransform().TransformFVector4(FVector4(0.0f, 1.0f, 0.0f));
				const FVector ZAmount = FVector::UpVector * FMath::RandRange(ExplosionZImpulseMin, ExplosionZImpulseMax);

				const FVector ImpulseDir = (BI->GetCOMPosition() - ExplosionLocation).GetSafeNormal() * ExplosionImpulseMagnitude;
				const FVector FinalImpulse = ImpulseDir + XAmount + YAmount + ZAmount;

				FTransform BodyTransform = BI->GetUnrealWorldTransform();
				BodyTransform.SetLocation(BodyTransform.GetLocation() + FinalImpulse.GetSafeNormal() * SmearInitialPopDistance);
				BI->SetBodyTransform(BodyTransform, ETeleportType::None);

				const FName BoneName = DestructibleMeshComponent->GetBoneName(BI->InstanceBoneIndex);
				DestructibleMeshComponent->AddImpulse(FinalImpulse, BoneName, true);
				
				FVector RotationAxis(FMath::RandRange(-1.0f, 1.0f), FMath::RandRange(-1.0f, 1.0f), FMath::RandRange(-1.0f, 1.0f));
				if (RotationAxis.SizeSquared() < 0.01f)
				{
					RotationAxis.Set(1.0f, 0.0f, 0.0f);
				}
				RotationAxis.Normalize();
				RotationAxis = RotationAxis * FMath::RandRange(ExplosionAngularImpulseMin, ExplosionAngularImpulseMax);
				DestructibleMeshComponent->AddAngularImpulseInRadians(RotationAxis, BoneName, true);
			}
		}

		if (ExplosionParticleComponent != nullptr)
		{
			ExplosionParticleComponent->ResetParticles(true);
			ExplosionParticleComponent->SetVisibility(true);
			ExplosionParticleComponent->ActivateSystem();
		}

		if (ExplosionLightComponent != nullptr)
		{
			ExplosionLightComponent->SetVisibility(true);
			ExplosionLightTargetIntensity = ExplosionLightComponent->Intensity;
			ExplosionLightComponent->SetIntensity(0.0f);
		}

		if (ExplosionSplashDamageRadius > 0 && ExplosionSplashDamageAmount > 0)
		{
			TArray<TEnumAsByte<EObjectTypeQuery> > ObjectTypes;
			TArray<AActor*> ActorsToIgnore;
			TArray<class UPrimitiveComponent*> OutComponents;
			ActorsToIgnore.Add(GetOwner());

			// todo: this returns static mesh components and potentially other stuff we don't want
			UKismetSystemLibrary::SphereOverlapComponents(this, GetOwner()->GetActorLocation(), ExplosionSplashDamageRadius, ObjectTypes, nullptr, ActorsToIgnore, OutComponents);

			const FVector OwnerLocation = GetOwner()->GetActorLocation();
			for (int i = 0; i < OutComponents.Num(); i++)
			{
				UPrimitiveComponent* CurComp = OutComponents[i];
				AActor* const CurOwner = CurComp->GetOwner();
				if (CurOwner)
				{
					FOxiDamageInfo NewDamageInfo = DamageInfo;
					NewDamageInfo.DamageAmount = 9999999.f;
					NewDamageInfo.DamageCauser = GetOwner();
					NewDamageInfo.DamageXYImpulse = 3000;
					NewDamageInfo.DamageZImpulse = 3000;

					TInlineComponentArray<UOxiDamageComponent*> DamageComps;
					CurOwner->GetComponents(DamageComps);
					for (UOxiDamageComponent* CurDamageComp : DamageComps)
					{
						CurDamageComp->TakeDamage(NewDamageInfo);
					}
				}
			}
		}
	}

	return 1.0f;
}

/**
 *
 */
void UOxiDestructibleComponent::DisablePhysicsOnBodies(const TArray<int>& BodyList, bool shouldDisable)
{
	BodiesToSkip = BodyList;
}