// Copyright Myceland Team, All Rights Reserved.


#include "UI/ML_WidgetBase.h"

UWidget* UML_WidgetBase::GetDefaultFocusWidget_Implementation()
{
	// Fallback only — valid for widgets that are themselves directly focusable.
	// Pages containing buttons should override this in Blueprint to return their
	// first interactive primitive (see header note).
	return this;
}

void UML_WidgetBase::OnActivated_Implementation()
{
	SetVisibility(ESlateVisibility::Visible);
}

void UML_WidgetBase::OnDeactivated_Implementation()
{
	SetVisibility(ESlateVisibility::Collapsed);
}
