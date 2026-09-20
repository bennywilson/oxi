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
}

void UOxiOutlineSubsystem::Deinitialize()
{
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
	Palette = Settings->Palette.LoadSynchronous();
	if (!Palette && !Settings->Palette.IsNull())
	{
		UE_LOG(LogOxi, Warning, TEXT("Outline palette '%s' failed to load; outlines are disabled."), *Settings->Palette.ToString());
	}

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
			GPUStyle.Flags.X = EOxiOutlineFlags::Enabled
				| (Style.bShowThroughWalls ? EOxiOutlineFlags::ThroughWalls : 0)
				| (Style.bInteriorLines ? EOxiOutlineFlags::InteriorLines : 0);
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

void UOxiOutlineLibrary::ClearActorOutline(AActor* Actor)
{
	if (Actor)
	{
		Actor->ForEachComponent<UPrimitiveComponent>(false, [](UPrimitiveComponent* Component) { ClearOutline(Component); });
	}
}
