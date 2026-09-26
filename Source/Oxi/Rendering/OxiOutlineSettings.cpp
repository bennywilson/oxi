// OXI 2026

#include "OxiOutlineSettings.h"
#include "OxiOutlineSubsystem.h"

#if WITH_EDITOR
void UOxiOutlineSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (UOxiOutlineSubsystem* Subsystem = UOxiOutlineSubsystem::Get())
	{
		Subsystem->RefreshPalette();
	}
}
#endif
