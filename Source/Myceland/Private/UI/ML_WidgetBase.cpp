// Copyright Myceland Team, All Rights Reserved.


#include "UI/ML_WidgetBase.h"

#include "Animation/WidgetAnimation.h"

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

	// Per-page entry animation. Replaces the single global fade that WB_RootMenu used to play from
	// PlayTransition on every navigation — each page now owns its own (or has none at all).
	if (EnterAnim)
		PlayAnimation(EnterAnim);
}

void UML_WidgetBase::OnDeactivated_Implementation()
{
	SetVisibility(ESlateVisibility::Collapsed);
}
