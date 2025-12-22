// ELP 2020

#include "OxiDamageComponent.h"

DECLARE_MULTICAST_DELEGATE_ThreeParams(FDamageComponentOnDeath, UOxiDamageComponent* const, class AActor* const, class AActor* const);

UOxiDamageComponent::UOxiDamageComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

// Called when the game starts
void UOxiDamageComponent::BeginPlay()
{
	Super::BeginPlay();

	TArray<UActorComponent*> Children;
	GetOwner()->GetComponents(USkeletalMeshComponent::StaticClass(), Children);

	for (int iChild = 0; iChild < Children.Num(); iChild++)
	{
		USkeletalMeshComponent* const SkelMesh = Cast<USkeletalMeshComponent>(Children[iChild]);
		if (SkelMesh == nullptr || SkelMesh->IsVisible() == false)
		{
			continue;
		}

		SkeletalMeshes.Add(SkelMesh);
	}

	CurrentHealth = BaseHealth;
}

// Called every frame
void UOxiDamageComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	/*for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		if (APlayerController* PC = Iterator->Get())
		{
			FVector VecTo = GetOwner()->GetActorLocation() - PC->GetPawn()->GetActorLocation();
			VecTo.Z = 0.f;
			VecTo.Normalize();
			GetOwner()->SetActorRotation(VecTo.Rotation());
		}
	}*/
}

float UOxiDamageComponent::TakeDamage(const FOxiDamageInfo& DamageInfo)
{
	TakeDamage_Internal(DamageInfo);

	return 0.0f;
}

void UOxiDamageComponent::DisableRagdoll()
{
	/*for (int i = 0; i < SkeletalMeshes.Num(); i++)
	{
		USkeletalMeshComponent* const SkelMesh = SkeletalMeshes[i];
		SkelMesh->PutAllRigidBodiesToSleep();
	}*/

	GetOwner()->SetActorHiddenInGame(true);

	GetOwner()->GetWorldTimerManager().ClearTimer(RagdollSleepTimerHandle);
}

void UOxiDamageComponent::BroadcastDeath()
{
//	void OnDeath() const;

}