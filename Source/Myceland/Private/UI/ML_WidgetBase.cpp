// Copyright Myceland Team, All Rights Reserved.


#include "UI/ML_WidgetBase.h"

void UML_WidgetBase::OnActivated_Implementation()
{
	SetVisibility(ESlateVisibility::Visible);
}

void UML_WidgetBase::OnDeactivated_Implementation()
{
	SetVisibility(ESlateVisibility::Collapsed);
}
