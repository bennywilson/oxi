// OXI 2026

#include "OxiOutlinePalette.h"
#include "OxiOutlineSubsystem.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

// Defines the localization namespace used by LOCTEXT macros for editor data validation error messages.
#define LOCTEXT_NAMESPACE "OxiOutlinePalette"

const FOxiOutlineStyle* UOxiOutlinePalette::FindStyle(FName StyleName) const
{
	return Styles.FindByPredicate([StyleName](const FOxiOutlineStyle& Style) { return Style.Name == StyleName; });
}

#if WITH_EDITOR
void UOxiOutlinePalette::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (UOxiOutlineSubsystem* Subsystem = UOxiOutlineSubsystem::Get())
	{
		Subsystem->RefreshPalette();
	}
}

EDataValidationResult UOxiOutlinePalette::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	TSet<int32> SeenStencils;
	TSet<FName> SeenNames;
	for (const FOxiOutlineStyle& Style : Styles)
	{
		if (SeenStencils.Contains(Style.StencilValue))
		{
			Context.AddError(FText::Format(LOCTEXT("DuplicateStencil", "Stencil value {0} is used by more than one outline style."), Style.StencilValue));
			Result = EDataValidationResult::Invalid;
		}
		if (SeenNames.Contains(Style.Name))
		{
			Context.AddError(FText::Format(LOCTEXT("DuplicateName", "Outline style name '{0}' is used more than once."), FText::FromName(Style.Name)));
			Result = EDataValidationResult::Invalid;
		}
		SeenStencils.Add(Style.StencilValue);
		SeenNames.Add(Style.Name);
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
