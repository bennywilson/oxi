// ELP 2023

#include "OxiCheckpoint.h"

#include "GameFramework/Character.h"
#include "Engine/TriggerBox.h"
#include "Components/BillboardComponent.h"

/**
 *
 */
AOxiCheckpoint::AOxiCheckpoint()
{
	PrimaryActorTick.bCanEverTick = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(RootComponent);

#if WITH_EDITORONLY_DATA
	SpriteComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (!IsRunningCommandlet())
	{
		// Structure to hold one-time initialization
		struct FConstructorStatics
		{
			ConstructorHelpers::FObjectFinderOptional<UTexture2D> SpriteTexture;
			FName ID_Name;
			FText Name_Name;
			FConstructorStatics()
				: SpriteTexture(TEXT("/Game/Oxi/Core/Editor/S_Checkpoint"))
				, ID_Name(TEXT("SpawnSquad"))
				, Name_Name(NSLOCTEXT("SpriteCategory", "OxiAISpawnSquad", "OxiAISpawnSquads"))
			{
			}
		};
		static FConstructorStatics ConstructorStatics;

		if (SpriteComponent)
		{
			SpriteComponent->Sprite = ConstructorStatics.SpriteTexture.Get();
			SpriteComponent->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.5f));
			SpriteComponent->SpriteInfo.Category = ConstructorStatics.ID_Name;
			SpriteComponent->SpriteInfo.DisplayName = ConstructorStatics.Name_Name;
			SpriteComponent->SetupAttachment(RootComponent);
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
void AOxiCheckpoint::ReloadCheckpoint(ACharacter* const Player)
{
	if (Player == nullptr)
	{
		return;
	}

	Player->SetActorLocationAndRotation(GetActorLocation(), GetActorRotation());
	Player->GetLocalViewingPlayerController()->SetControlRotation(GetActorRotation());

	for (ATriggerBox* Trigger : TriggersToActivateOnReload)
	{
		if (Trigger == nullptr)
		{
			continue;
		}

		Trigger->OnActorBeginOverlap.Broadcast(Trigger, Player);
		Trigger->OnActorEndOverlap.Broadcast(Trigger, Player);
	}
}
