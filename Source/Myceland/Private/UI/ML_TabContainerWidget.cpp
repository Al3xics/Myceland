// Copyright Myceland Team, All Rights Reserved.


#include "UI/ML_TabContainerWidget.h"

#include "Components/HorizontalBoxSlot.h"
#include "Components/WidgetSwitcherSlot.h"
#include "UI/ML_TabButtonWidget.h"

void UML_TabContainerWidget::OnTabButtonClicked(int32 TabIndex)
{
	SwitchToTab(TabIndex);
}

void UML_TabContainerWidget::UpdateTabButtonStates()
{
	for (int32 i = 0; i < TabButtons.Num(); ++i)
	{
		if (UML_TabButtonWidget* TabButton = TabButtons[i])
		{
			TabButton->SetIsActive(i == CurrentTabIndex);
			if (i == CurrentTabIndex)
				TabButton->PlaySwitchAnimation(PreviousTabIndex, CurrentTabIndex);
		}
	}
}

void UML_TabContainerWidget::NativeConstruct()
{
	Super::NativeConstruct();
	InitializeTabs();
}

void UML_TabContainerWidget::InitializeTabs()
{
	ensureMsgf(TabButtonsContainer && ContentSwitcher, TEXT("TabButtonsContainer or ContentSwitcher not bound in UMG!"));
	if (!TabButtonsContainer || !ContentSwitcher)
		return;
    
	TabButtonsContainer->ClearChildren();
	ContentSwitcher->ClearChildren();
	TabButtons.Empty();
    
	for (int32 i = 0; i < Tabs.Num(); ++i)
	{
		const FML_TabData& TabData = Tabs[i];
        
		// Create a tab button
		if (TabButtonClass)
		{
			if (UML_TabButtonWidget* TabButtonWidget = CreateWidget<UML_TabButtonWidget>(this, TabButtonClass))
			{
				// Bind click event - NO SWITCH, NO SEPARATE METHODS!
				TabButtonWidget->OnTabClicked.AddDynamic(this, &UML_TabContainerWidget::OnTabButtonClicked);
                
				TabButtons.Add(TabButtonWidget);
				
				// Add to the container and configure the slot
				if (UHorizontalBoxSlot* HBoxSlot = Cast<UHorizontalBoxSlot>(TabButtonsContainer->AddChild(TabButtonWidget)))
				{
					const FML_TabSlotSettings& Settings = TabData.SlotSettings;
                
					HBoxSlot->SetHorizontalAlignment(Settings.HorizontalAlignment);
					HBoxSlot->SetVerticalAlignment(Settings.VerticalAlignment);
					HBoxSlot->SetPadding(Settings.Padding);
                
					if (Settings.bAutoSize)
						HBoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
					else
					{
						FSlateChildSize Size = FSlateChildSize(ESlateSizeRule::Fill);
						Size.Value = Settings.Size;
						HBoxSlot->SetSize(Size);
					}
				}
				
				// Set tab data
				TabButtonWidget->SetTabData(TabData.TabName, i);
			}
		}
        
		// Create a content widget
		if (TabData.ContentWidgetClass)
		{
			if (UUserWidget* ContentWidget = CreateWidget<UUserWidget>(this, TabData.ContentWidgetClass))
			{
				// Configure the content switcher slot as well
				if (UWidgetSwitcherSlot* ContentSlot = Cast<UWidgetSwitcherSlot>(ContentSwitcher->AddChild(ContentWidget)))
				{
					ContentSlot->SetHorizontalAlignment(HAlign_Fill);
					ContentSlot->SetVerticalAlignment(VAlign_Fill);
				}
			}
		}
	}
    
	// Activate the first tab
	if (Tabs.Num() > 0)
		SwitchToTab(0);
}

void UML_TabContainerWidget::SwitchToTab(int32 TabIndex)
{
	if (!Tabs.IsValidIndex(TabIndex) || !ContentSwitcher)
		return;
    
	PreviousTabIndex = CurrentTabIndex;
	CurrentTabIndex = TabIndex;
	ContentSwitcher->SetActiveWidgetIndex(TabIndex);
    
	UpdateTabButtonStates();
}
