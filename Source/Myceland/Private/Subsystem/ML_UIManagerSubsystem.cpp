// Copyright Myceland Team, All Rights Reserved.


#include "Subsystem/ML_UIManagerSubsystem.h"

#include "Developer Settings/ML_MycelandDeveloperSettings.h"
#include "Kismet/GameplayStatics.h"
#include "UI/ML_RootWidgetBase.h"
#include "UI/ML_WidgetBase.h"

void UML_UIManagerSubsystem::SwitchWidgetInternal(FGameplayTag InWidgetTag, bool bAddToStack)
{
	const FML_WidgetRegistryKey* Key = WidgetTagToRegistryKey.Find(InWidgetTag);
	ensureMsgf(Key, TEXT("Widget not registered: %s"), *InWidgetTag.ToString());
	if (!Key) return;

	UUserWidget* ChildWidget = RegisteredWidgetsInRoot.FindRef(*Key);
	UUserWidget* RootWidget = RegisteredRootWidgets.FindRef(Key->RootTag);

	UML_RootWidgetBase* Root = Cast<UML_RootWidgetBase>(RootWidget);
	ensureMsgf(Root, TEXT("Root widget is not UML_RootWidgetBase"));
	if (!Root) return;
	
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
	ensureMsgf(Widget, TEXT("Trying to apply input mode from null widget"));
	if (!Widget) return;

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	ensureMsgf(PC, TEXT("Trying to apply input mode from null player controller"));
	if (!PC) return;

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
	ensureMsgf(InWidget, TEXT("Trying to register null root widget"));
	if (!InWidget) return;

	if (!RegisteredRootWidgets.Contains(InRootTag))
		RegisteredRootWidgets.Add(InRootTag, InWidget);
}

void UML_UIManagerSubsystem::RegisterWidgetInRootWidget(UPARAM(meta=(Categories="UI.Root")) FGameplayTag InRootTag, UPARAM(meta=(Categories="UI.Widget")) FGameplayTag InWidgetTag, UUserWidget* InWidget)
{
	ensureMsgf(InWidget, TEXT("Trying to register null widget"));
	if (!InWidget) return;
	ensureMsgf(RegisteredRootWidgets.Contains(InRootTag), TEXT("Root widget %s is not registered"), *InRootTag.ToString());
	if (!RegisteredRootWidgets.Contains(InRootTag)) return;

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
	ensureMsgf(Found, TEXT("Root widget not found: %s"), *InRootTag.ToString());
	if (!Found) return nullptr;
	return Found;
}

const UUserWidget* UML_UIManagerSubsystem::FindWidgetInRootByTag(UPARAM(meta=(Categories="UI.Root")) FGameplayTag InRootTag, UPARAM(meta=(Categories="UI.Widget")) FGameplayTag InWidgetTag)
{
	const FML_WidgetRegistryKey Key{InRootTag, InWidgetTag};
	const UUserWidget* Found = RegisteredWidgetsInRoot.FindRef(Key);
	ensureMsgf(Found, TEXT("Widget not found in root %s : %s"), *InRootTag.ToString(), *InWidgetTag.ToString());
	if (!Found) return nullptr;
	return Found;
}

void UML_UIManagerSubsystem::NavigateTo(UPARAM(meta=(Categories="UI.Widget")) FGameplayTag InWidgetTag)
{
	SwitchWidgetInternal(InWidgetTag, true);
}

void UML_UIManagerSubsystem::GoBack()
{
	ensureMsgf(NavigationStack.Num() > 0, TEXT("Navigation stack empty"));
	if (NavigationStack.Num() <= 0) return;
	const FGameplayTag PreviousTag = NavigationStack.Pop();
	SwitchWidgetInternal(PreviousTag, false);
}

void UML_UIManagerSubsystem::OpenLevelByTag(UPARAM(meta=(Categories="Level")) FGameplayTag Level, UObject* WorldContextObject)
{
	if (!WorldContextObject) return;

	const UML_MycelandDeveloperSettings* Settings = GetDefault<UML_MycelandDeveloperSettings>();
	if (!Settings) return;

	if (const TSoftObjectPtr<UWorld>* FoundLevel = Settings->Levels.Find(Level))
	{
		if (const UWorld* WorldAsset = FoundLevel->LoadSynchronous())
		{
			UGameplayStatics::OpenLevel(WorldContextObject, WorldAsset->GetFName());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Level '%s' failed to load."), *Level.ToString());
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Level '%s' not found in Developer Settings"), *Level.ToString());
	}
}
