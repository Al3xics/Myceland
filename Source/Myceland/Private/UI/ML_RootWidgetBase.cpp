// Copyright Myceland Team, All Rights Reserved.


#include "UI/ML_RootWidgetBase.h"

#include "Components/WidgetSwitcher.h"

void UML_RootWidgetBase::SwitchToWidget(UUserWidget* Widget)
{
	if (!WidgetSwitcher || !Widget)
		return;
	
	UUserWidget* Previous = Cast<UUserWidget>(WidgetSwitcher->GetActiveWidget());
	
	PlayTransition(Previous, Widget);

	const int32 Index = WidgetSwitcher->GetChildIndex(Widget);
	if (Index != INDEX_NONE)
		WidgetSwitcher->SetActiveWidget(Widget);
}
