// OXI 2026

#include "OxiOutlineSubsystem.h"
#include "Oxi.h"
#include "OxiOutlinePalette.h"
#include "OxiOutlineSceneViewExtension.h"
#include "OxiOutlineSettings.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "SceneViewExtension.h"

UOxiOutlineSubsystem* UOxiOutlineSubsystem::Get()
{
	return GEngine ? GEngine->GetEngineSubsystem<UOxiOutlineSubsystem>() : nullptr;
}

void UOxiOutlineSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Extension = FSceneViewExtensions::NewExtension<FOxiOutlineSceneViewExtension>();
	RefreshPalette();

	TickHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UOxiOutlineSubsystem::Tick));
}

void UOxiOutlineSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	ActiveSmears.Empty();
	Extension.Reset();
	Palette = nullptr;

	Super::Deinitialize();
}

void UOxiOutlineSubsystem::RefreshPalette()
{
	if (!Extension.IsValid())
	{
		return;
	}

	const UOxiOutlineSettings* Settings = GetDefault<UOxiOutlineSettings>();
	UOxiOutlinePalette* LoadedPalette = Settings->Palette.LoadSynchronous();
	if (!LoadedPalette && !Settings->Palette.IsNull())
	{
		// Keep drawing with whatever was pushed last rather than blanking every outline in the level.
		UE_LOG(LogOxi, Warning, TEXT("Outline palette '%s' failed to load; keeping the styles already in use."), *Settings->Palette.ToString());
		return;
	}
	Palette = LoadedPalette;

	// Indexed directly by stencil value.
	TArray<FOxiOutlineStyleGPU> GPUStyles;
	GPUStyles.SetNum(256);

	if (Palette)
	{
		for (const FOxiOutlineStyle& Style : Palette->Styles)
		{
			if (Style.StencilValue < 1 || Style.StencilValue > 255)
			{
				continue;
			}

			FOxiOutlineStyleGPU& GPUStyle = GPUStyles[Style.StencilValue];
			GPUStyle.ColorDim = FVector4f(Style.Color.R, Style.Color.G, Style.Color.B, Style.DimAmount);
			const FLinearColor Emissive = Style.EmissiveColor * Style.EmissiveIntensity;
			GPUStyle.EmissiveFloor = FVector4f(Emissive.R, Emissive.G, Emissive.B, Style.MinBrightness);
			GPUStyle.Width = FVector4f(Style.Width, Style.MinWidth, Style.InteriorWidthScale, Style.Opacity);
			GPUStyle.Misc = FVector4f(Style.FocusInfluence, 0.f, 0.f, 0.f);
			GPUStyle.Flags.X = EOxiOutlineFlags::Enabled
				| (Style.bShowThroughWalls ? EOxiOutlineFlags::ThroughWalls : 0)
				| (Style.bInteriorLines ? EOxiOutlineFlags::InteriorLines : 0);
		}
	}

	// A smearing actor borrows a reserved stencil value, so give that value the same look as the style it came from.
	for (const FActiveSmear& Smear : ActiveSmears)
	{
		if (Smear.SmearStencil != Smear.BaseStencil
			&& GPUStyles.IsValidIndex(Smear.SmearStencil) && GPUStyles.IsValidIndex(Smear.BaseStencil))
		{
			GPUStyles[Smear.SmearStencil] = GPUStyles[Smear.BaseStencil];
		}
	}

	FOxiOutlineGlobals Globals;
	Globals.NearDistance = Settings->NearDistance;
	Globals.FarDistance = Settings->FarDistance;
	Globals.ReferenceHeight = Settings->ReferenceHeight;
	Globals.MaxWidth = Settings->MaxWidth;
	Globals.SelfOverlapDepth = Settings->SelfOverlapDepth;
	Globals.SelfOverlapDepthPerPixel = Settings->SelfOverlapDepthPerPixel;
	Globals.OcclusionBias = Settings->OcclusionBias;
	Globals.OcclusionBiasPerDepth = Settings->OcclusionBiasPerDepth;

	Extension->SetPalette_GameThread(MoveTemp(GPUStyles), Globals);
}

void UOxiOutlineSubsystem::SetFocus(const FLinearColor& Color, float EmissiveIntensity, float Amount)
{
	FocusColor = Color;
	FocusEmissiveIntensity = EmissiveIntensity;
	FocusAmount = Amount;

	if (Extension.IsValid())
	{
		Extension->SetFocus_GameThread(Color, EmissiveIntensity, Amount);
	}
}

bool UOxiOutlineSubsystem::StartSmear(AActor* Actor, const FVector& FromLocation, float Duration)
{
	if (!Actor || Duration <= 0.f || !Extension.IsValid())
	{
		return false;
	}

	// Restarting a dash reuses the actor's slot rather than stacking trails.
	StopSmear(Actor);

	// Free reserved stencil value.
	int32 SmearStencil = INDEX_NONE;
	for (int32 Slot = 0; Slot < NumSmearSlots; ++Slot)
	{
		const int32 Candidate = FirstSmearStencil + Slot;
		const bool bInUse = ActiveSmears.ContainsByPredicate(
			[Candidate](const FActiveSmear& Smear) { return Smear.SmearStencil == Candidate; });
		if (!bInUse)
		{
			SmearStencil = Candidate;
			break;
		}
	}

	if (SmearStencil == INDEX_NONE)
	{
		// More actors dashing at once than there are slots; the oldest trail keeps its slot.
		return false;
	}

	FActiveSmear Smear;
	Smear.Actor = Actor;
	Smear.SmearStencil = SmearStencil;
	Smear.Start = FromLocation;
	Smear.Duration = Duration;
	Smear.TimeLeft = Duration;
	Smear.BaseStencil = INDEX_NONE;

	// Collect first, swap after: an actor whose components use several styles would otherwise end up borrowing
	// one reserved value for all of them, and any component whose stencil has no style would blank the lot.
	Actor->ForEachComponent<UPrimitiveComponent>(false, [this, &Smear](UPrimitiveComponent* Component)
	{
		const int32 Stencil = Component->CustomDepthStencilValue;
		if (!Component->bRenderCustomDepth || !FindStyleByStencil(Stencil))
		{
			return;
		}

		// The first style found owns the trail; components using a different one keep their own outline.
		if (Smear.BaseStencil == INDEX_NONE)
		{
			Smear.BaseStencil = Stencil;
		}
		if (Stencil != Smear.BaseStencil)
		{
			return;
		}

		Smear.Components.Add(Component);
		Smear.OriginalStencils.Add(Stencil);
	});

	if (Smear.Components.IsEmpty())
	{
		UE_LOG(LogOxi, Warning,
			TEXT("StartOutlineSmear: '%s' has no component rendering custom depth with a stencil value the outline palette defines, so there is no ink to trail."),
			*Actor->GetName());
		return false;
	}

	for (const TWeakObjectPtr<UPrimitiveComponent>& Component : Smear.Components)
	{
		Component->SetCustomDepthStencilValue(SmearStencil);
	}

	// Measure the trail against the outlined mesh rather than the actor, whose bounds take in everything
	// attached to it and would describe something far larger than the character.
	Smear.TrackedComponent = Smear.Components[0];

	UE_LOG(LogOxi, Verbose, TEXT("StartOutlineSmear: '%s' %d component(s), style stencil %d borrowing %d for %.2fs."),
		*Actor->GetName(), Smear.Components.Num(), Smear.BaseStencil, SmearStencil, Duration);

	ActiveSmears.Add(MoveTemp(Smear));

	// The reserved stencil value needs a style before it is drawn with.
	RefreshPalette();
	PushSmears();
	return true;
}

bool UOxiOutlineSubsystem::StartMotionSmear(AActor* Actor, float TrailSeconds, float Duration)
{
	if (!Actor || TrailSeconds <= 0.f || !Extension.IsValid())
	{
		return false;
	}

	StopSmear(Actor);

	if (ActiveSmears.Num() >= MaxActiveSmears)
	{
		return false;
	}

	// A motion trail keeps the object's own stencil value: several objects can share a style and still each
	// get their own trail, so this does not eat one of the dash slots.
	int32 BaseStencil = INDEX_NONE;
	UPrimitiveComponent* TrackedComponent = nullptr;
	Actor->ForEachComponent<UPrimitiveComponent>(false, [this, &BaseStencil, &TrackedComponent](UPrimitiveComponent* Component)
	{
		if (BaseStencil == INDEX_NONE && Component->bRenderCustomDepth && FindStyleByStencil(Component->CustomDepthStencilValue))
		{
			BaseStencil = Component->CustomDepthStencilValue;
			TrackedComponent = Component;
		}
	});

	if (BaseStencil == INDEX_NONE)
	{
		UE_LOG(LogOxi, Warning,
			TEXT("StartOutlineMotionSmear: '%s' has no component rendering custom depth with a stencil value the outline palette defines, so there is no ink to trail."),
			*Actor->GetName());
		return false;
	}

	FActiveSmear Smear;
	Smear.Actor = Actor;
	Smear.TrackedComponent = TrackedComponent;
	Smear.SmearStencil = BaseStencil;
	Smear.BaseStencil = BaseStencil;
	Smear.bFollowMotion = true;
	Smear.TrailSeconds = TrailSeconds;
	Smear.Duration = Duration;
	Smear.TimeLeft = Duration;
	Smear.Start = GetSmearLocation(Smear);
	Smear.LastLocation = Smear.Start;

	ActiveSmears.Add(MoveTemp(Smear));
	PushSmears();
	return true;
}

void UOxiOutlineSubsystem::StopSmear(AActor* Actor)
{
	const int32 Index = ActiveSmears.IndexOfByPredicate(
		[Actor](const FActiveSmear& Smear) { return Smear.Actor.Get() == Actor; });
	if (Index == INDEX_NONE)
	{
		return;
	}

	const FActiveSmear& Smear = ActiveSmears[Index];
	for (int32 i = 0; i < Smear.Components.Num(); ++i)
	{
		if (UPrimitiveComponent* Component = Smear.Components[i].Get())
		{
			Component->SetCustomDepthStencilValue(Smear.OriginalStencils[i]);
		}
	}

	ActiveSmears.RemoveAt(Index);
	PushSmears();
}

bool UOxiOutlineSubsystem::Tick(float DeltaSeconds)
{
	if (ActiveSmears.IsEmpty())
	{
		return true;
	}

	bool bAnyFinished = false;
	for (int32 Index = ActiveSmears.Num() - 1; Index >= 0; --Index)
	{
		FActiveSmear& Smear = ActiveSmears[Index];
		Smear.TimeLeft -= DeltaSeconds;

		// Tracked here because GetComponentVelocity() reports nothing for movement that isn't physics driven.
		if (Smear.Actor.IsValid() && DeltaSeconds > 0.f)
		{
			const FVector Location = GetSmearLocation(Smear);
			Smear.FallbackVelocity = (Location - Smear.LastLocation) / DeltaSeconds;
			Smear.LastLocation = Location;
		}

		// A motion trail with no duration runs until it is stopped or its actor goes away.
		const bool bExpired = Smear.Duration > 0.f && Smear.TimeLeft <= 0.f;
		if (bExpired || !Smear.Actor.IsValid())
		{
			// Put the outline back. A destroyed actor has nothing left to restore.
			for (int32 i = 0; i < Smear.Components.Num(); ++i)
			{
				if (UPrimitiveComponent* Component = Smear.Components[i].Get())
				{
					Component->SetCustomDepthStencilValue(Smear.OriginalStencils[i]);
				}
			}

			ActiveSmears.RemoveAt(Index);
			bAnyFinished = true;
		}
	}

	if (bAnyFinished)
	{
		RefreshPalette();
	}

	PushSmears();
	return true;
}

void UOxiOutlineSubsystem::PushSmears()
{
	if (!Extension.IsValid())
	{
		return;
	}

	TArray<FOxiOutlineSmear> Smears;
	Smears.Reserve(ActiveSmears.Num());

	for (const FActiveSmear& Active : ActiveSmears)
	{
		const AActor* Actor = Active.Actor.Get();
		const FOxiOutlineStyle* Style = FindStyleByStencil(Active.BaseStencil);
		if (!Actor || !Style)
		{
			continue;
		}

		const UPrimitiveComponent* Tracked = Active.TrackedComponent.Get();

		// A dash trail starts from a location gameplay captured off the actor, so its end has to come from the
		// actor too or the trail would be offset by however far the mesh sits from the actor's origin.
		const FVector Location = Active.bFollowMotion ? GetSmearLocation(Active) : Actor->GetActorLocation();

		FOxiOutlineSmear& Smear = Smears.AddDefaulted_GetRef();
		Smear.End = Location;

		// Trail width follows how wide the object is, measured horizontally so a tall character doesn't get a
		// trail as wide as it is high.
		const FVector Extent = Tracked ? Tracked->Bounds.BoxExtent : Actor->GetComponentsBoundingBox(true).GetExtent();
		Smear.Radius = FMath::Max(FMath::Max(Extent.X, Extent.Y), 1.0) * Style->SmearWidthScale;

		if (Active.bFollowMotion)
		{
			// The trail spans where the object was a moment ago, and thins out as it slows to a stop.
			// Physics usually runs on a mesh component, while the actor's root sits still, so ask the component.
			FVector Velocity = Tracked ? Tracked->GetComponentVelocity() : Actor->GetVelocity();
			if (Velocity.IsNearlyZero())
			{
				Velocity = Active.FallbackVelocity;
			}

			const float Speed = Velocity.Size();
			const float FullSpeed = FMath::Max(Style->SmearFullSpeed, Style->SmearMinSpeed + 1.f);
			Smear.Start = Location - Velocity * Active.TrailSeconds;
			Smear.Strength = FMath::Clamp((Speed - Style->SmearMinSpeed) / (FullSpeed - Style->SmearMinSpeed), 0.f, 1.f);

			// A trail with a duration still fades out at the end of it.
			if (Active.Duration > 0.f)
			{
				Smear.Strength *= FMath::Clamp(Active.TimeLeft / Active.Duration, 0.f, 1.f);
			}
		}
		else
		{
			Smear.Start = Active.Start;
			Smear.Strength = FMath::Clamp(Active.TimeLeft / FMath::Max(Active.Duration, UE_KINDA_SMALL_NUMBER), 0.f, 1.f);
		}

		Smear.Falloff = Style->SmearFalloff;
		Smear.Opacity = Style->SmearOpacity;
		Smear.Taper = Style->SmearTaper;
		Smear.MaxLength = Style->SmearMaxLength;
		Smear.Stencil = static_cast<uint32>(Active.SmearStencil);
	}

	Extension->SetSmears_GameThread(MoveTemp(Smears));
}

FVector UOxiOutlineSubsystem::GetSmearLocation(const FActiveSmear& Smear)
{
	// Follow the outlined component: a shell casing simulates physics on a child mesh while the actor's root,
	// and so GetActorLocation(), never moves.
	if (const UPrimitiveComponent* Component = Smear.TrackedComponent.Get())
	{
		return Component->GetComponentLocation();
	}

	const AActor* Actor = Smear.Actor.Get();
	return Actor ? Actor->GetActorLocation() : FVector::ZeroVector;
}

const FOxiOutlineStyle* UOxiOutlineSubsystem::FindStyleByStencil(int32 StencilValue) const
{
	return Palette ? Palette->Styles.FindByPredicate(
		[StencilValue](const FOxiOutlineStyle& Style) { return Style.StencilValue == StencilValue; }) : nullptr;
}

int32 UOxiOutlineSubsystem::FindStencilValue(FName StyleName) const
{
	const FOxiOutlineStyle* Style = Palette ? Palette->FindStyle(StyleName) : nullptr;
	return Style ? Style->StencilValue : INDEX_NONE;
}

bool UOxiOutlineLibrary::SetOutlineStyle(UPrimitiveComponent* Component, FName StyleName)
{
	const UOxiOutlineSubsystem* Subsystem = UOxiOutlineSubsystem::Get();
	const int32 StencilValue = Subsystem ? Subsystem->FindStencilValue(StyleName) : INDEX_NONE;
	if (!Component || StencilValue == INDEX_NONE)
	{
		return false;
	}

	Component->SetRenderCustomDepth(true);
	Component->SetCustomDepthStencilValue(StencilValue);
	return true;
}

bool UOxiOutlineLibrary::SetActorOutlineStyle(AActor* Actor, FName StyleName)
{
	if (!Actor)
	{
		return false;
	}

	bool bAllSet = true;
	Actor->ForEachComponent<UPrimitiveComponent>(false, [&](UPrimitiveComponent* Component)
	{
		bAllSet &= SetOutlineStyle(Component, StyleName);
	});
	return bAllSet;
}

void UOxiOutlineLibrary::ClearOutline(UPrimitiveComponent* Component)
{
	if (Component)
	{
		Component->SetRenderCustomDepth(false);
		Component->SetCustomDepthStencilValue(0);
	}
}

void UOxiOutlineLibrary::SetOutlineFocus(FLinearColor Color, float Amount, float EmissiveIntensity)
{
	if (UOxiOutlineSubsystem* Subsystem = UOxiOutlineSubsystem::Get())
	{
		Subsystem->SetFocus(Color, EmissiveIntensity, Amount);
	}
}

void UOxiOutlineLibrary::SetOutlineFocusAmount(float Amount)
{
	if (UOxiOutlineSubsystem* Subsystem = UOxiOutlineSubsystem::Get())
	{
		Subsystem->SetFocus(Subsystem->GetFocusColor(), Subsystem->GetFocusEmissiveIntensity(), Amount);
	}
}

bool UOxiOutlineLibrary::StartOutlineSmear(AActor* Actor, FVector FromLocation, float Duration)
{
	UOxiOutlineSubsystem* Subsystem = UOxiOutlineSubsystem::Get();
	return Subsystem ? Subsystem->StartSmear(Actor, FromLocation, Duration) : false;
}

bool UOxiOutlineLibrary::StartOutlineMotionSmear(AActor* Actor, float TrailSeconds, float Duration)
{
	UOxiOutlineSubsystem* Subsystem = UOxiOutlineSubsystem::Get();
	return Subsystem ? Subsystem->StartMotionSmear(Actor, TrailSeconds, Duration) : false;
}

void UOxiOutlineLibrary::StopOutlineSmear(AActor* Actor)
{
	if (UOxiOutlineSubsystem* Subsystem = UOxiOutlineSubsystem::Get())
	{
		Subsystem->StopSmear(Actor);
	}
}

void UOxiOutlineLibrary::ClearActorOutline(AActor* Actor)
{
	if (Actor)
	{
		Actor->ForEachComponent<UPrimitiveComponent>(false, [](UPrimitiveComponent* Component) { ClearOutline(Component); });
	}
}
