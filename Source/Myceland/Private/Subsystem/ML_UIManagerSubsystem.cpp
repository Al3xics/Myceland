// Copyright Myceland Team, All Rights Reserved.


#include "Subsystem/ML_UIManagerSubsystem.h"
#include "UI/ML_RootWidgetBase.h"
#include "UI/ML_WidgetBase.h"

void UML_UIManagerSubsystem::SwitchWidgetInternal(FGameplayTag InWidgetTag, bool bAddToStack)
{
	const FML_WidgetRegistryKey* Key = WidgetTagToRegistryKey.Find(InWidgetTag);
	checkf(Key, TEXT("Widget not registered: %s"), *InWidgetTag.ToString());

	UUserWidget* ChildWidget = RegisteredWidgetsInRoot.FindRef(*Key);
	UUserWidget* RootWidget = RegisteredRootWidgets.FindRef(Key->RootTag);

	UML_RootWidgetBase* Root = Cast<UML_RootWidgetBase>(RootWidget);
	checkf(Root, TEXT("Root widget is not UML_RootWidgetBase"));
	
	// Initialize
	if (!CurrentWidgetTag.IsValid())
		CurrentWidgetTag = InWidgetTag;
	
	// Deactivate current widget
	if (UML_WidgetBase* Current = Cast<UML_WidgetBase>(RegisteredWidgetsInRoot.FindRef(WidgetTagToRegistryKey.FindRef(CurrentWidgetTag))))
		Current->OnDeactivated();

	// Navigation stack
	if (bAddToStack && CurrentWidgetTag.IsValid())
		NavigationStack.Push(CurrentWidgetTag);

	Root->SwitchToWidget(ChildWidget);

	// Activate new widget
	if (UML_WidgetBase* Widget = Cast<UML_WidgetBase>(ChildWidget))
	{
		Widget->OnActivated();
		ApplyInputModeFromWidget(Widget);
	}
	
	CurrentWidgetTag = InWidgetTag;
}

void UML_UIManagerSubsystem::ApplyInputModeFromWidget(UML_WidgetBase* Widget) const
{
	checkf(Widget, TEXT("Trying to apply input mode from null widget"));

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	checkf(PC, TEXT("Trying to apply input mode from null player controller"));

	switch (Widget->InputMode)
	{
		case EML_WidgetInputMode::GameOnly:
			{
				FInputModeGameOnly Mode;
				PC->SetInputMode(Mode);
				PC->SetShowMouseCursor(false);
				break;
			}

		case EML_WidgetInputMode::UIOnly:
			{
				FInputModeUIOnly Mode;
				Mode.SetWidgetToFocus(Widget->TakeWidget());
				Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
				PC->SetInputMode(Mode);
				PC->SetShowMouseCursor(true);
				break;
			}

		case EML_WidgetInputMode::GameAndUI:
			{
				FInputModeGameAndUI Mode;
				Mode.SetWidgetToFocus(Widget->TakeWidget());
				PC->SetInputMode(Mode);
				PC->SetShowMouseCursor(true);
				break;
			}
			
		default: 
			break;
	}

	Widget->SetFocus();
}

void UML_UIManagerSubsystem::RegisterRootWidget(UPARAM(meta=(Categories="UI.Root")) FGameplayTag InRootTag, UUserWidget* InWidget)
{
	checkf(InWidget, TEXT("Trying to register null root widget"));

	if (!RegisteredRootWidgets.Contains(InRootTag))
		RegisteredRootWidgets.Add(InRootTag, InWidget);
}

void UML_UIManagerSubsystem::RegisterWidgetInRootWidget(UPARAM(meta=(Categories="UI.Root")) FGameplayTag InRootTag, UPARAM(meta=(Categories="UI.Widget")) FGameplayTag InWidgetTag, UUserWidget* InWidget)
{
	checkf(InWidget, TEXT("Trying to register null widget"));
	checkf(RegisteredRootWidgets.Contains(InRootTag), TEXT("Root widget %s is not registered"), *InRootTag.ToString());

	if (RegisteredRootWidgets.Contains(InRootTag))
	{
		const FML_WidgetRegistryKey Key{InRootTag, InWidgetTag};

		RegisteredWidgetsInRoot.Add(Key, InWidget);
		WidgetTagToRegistryKey.Add(InWidgetTag, Key);
	}
}

const UUserWidget* UML_UIManagerSubsystem::FindRootWidgetByTag(UPARAM(meta=(Categories="UI.Root")) FGameplayTag InRootTag)
{
	const UUserWidget* Found = RegisteredRootWidgets.FindRef(InRootTag);
	checkf(Found, TEXT("Root widget not found: %s"), *InRootTag.ToString());
	return Found;
}

const UUserWidget* UML_UIManagerSubsystem::FindWidgetInRootByTag(UPARAM(meta=(Categories="UI.Root")) FGameplayTag InRootTag, UPARAM(meta=(Categories="UI.Widget")) FGameplayTag InWidgetTag)
{
	const FML_WidgetRegistryKey Key{InRootTag, InWidgetTag};
	const UUserWidget* Found = RegisteredWidgetsInRoot.FindRef(Key);
	checkf(Found, TEXT("Widget not found in root %s : %s"), *InRootTag.ToString(), *InWidgetTag.ToString());
	return Found;
}

void UML_UIManagerSubsystem::NavigateTo(UPARAM(meta=(Categories="UI.Widget")) FGameplayTag InWidgetTag)
{
	SwitchWidgetInternal(InWidgetTag, true);
}

void UML_UIManagerSubsystem::GoBack()
{
	checkf(NavigationStack.Num() > 0, TEXT("Navigation stack empty"));
	const FGameplayTag PreviousTag = NavigationStack.Pop();
	SwitchWidgetInternal(PreviousTag, false);
}
