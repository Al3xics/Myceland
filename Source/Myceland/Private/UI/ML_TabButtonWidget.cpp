// Copyright Myceland Team, All Rights Reserved.


#include "UI/ML_TabButtonWidget.h"

void UML_TabButtonWidget::OnButtonClicked()
{
	OnTabClicked.Broadcast(TabIndex);
}

void UML_TabButtonWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	if (Button)
		Button->OnClicked.AddDynamic(this, &UML_TabButtonWidget::OnButtonClicked);
}

void UML_TabButtonWidget::SetTabData(const FText& InTabName, int32 InTabIndex)
{
	TabIndex = InTabIndex;
	
	if (TabText)
		TabText->SetText(InTabName);
}

void UML_TabButtonWidget::SetIsActive(bool bInActive)
{
	bIsActive = bInActive;
	OnVisualStateChanged(bInActive);
}

void UML_TabButtonWidget::PlaySwitchAnimation(int32 FromIndex, int32 ToIndex)
{
	PlayTabSwitchAnimation(FromIndex, ToIndex);
}
