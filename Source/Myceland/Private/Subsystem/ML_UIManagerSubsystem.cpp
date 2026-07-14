// Copyright Myceland Team, All Rights Reserved.


#include "Subsystem/ML_UIManagerSubsystem.h"

#include "Developer Settings/ML_MycelandDeveloperSettings.h"
#include "Input/ML_InputDeviceManager.h"
#include "Kismet/GameplayStatics.h"
#include "Player/ML_PlayerController.h"
#include "UI/ML_RootWidgetBase.h"
#include "UI/ML_WidgetBase.h"

void UML_UIManagerSubsystem::SwitchWidgetInternal(FGameplayTag InWidgetTag, bool bAddToStack)
{
	UUserWidget* ChildWidget = RegisteredWidgets.FindRef(InWidgetTag);
	if (!ChildWidget)
	{
		ensureMsgf(false, TEXT("Widget not registered: %s"), *InWidgetTag.ToString());
		return;
	}

	UML_RootWidgetBase* Root = Cast<UML_RootWidgetBase>(CurrentContextRoot);
	ensureMsgf(Root, TEXT("Context root widget is not UML_RootWidgetBase"));
	if (!Root) return;
    
	// Initialize
	if (!CurrentWidgetTag.IsValid())
		CurrentWidgetTag = InWidgetTag;
    
	// Deactivate current widget
	if (UML_WidgetBase* Current = Cast<UML_WidgetBase>(RegisteredWidgets.FindRef(CurrentWidgetTag)))
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

void UML_UIManagerSubsystem::ApplyInputModeFromWidget(UML_WidgetBase* Widget)
{
	ensureMsgf(Widget, TEXT("Trying to apply input mode from null widget"));
	if (!Widget) return;

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	ensureMsgf(PC, TEXT("Trying to apply input mode from null player controller"));
	if (!PC) return;

	// Initial focus anchor for the input mode. We target the interactive widget
	// (GetDefaultFocusWidget — e.g. the inner SButton) rather than the page container,
	// so input has a valid focus the instant the mode is applied. Slate frequently drops
	// this focus because the widget hasn't had its first layout pass yet; SetFocusDeferred()
	// below re-applies the real focus a fraction of a second later (this is the call that sticks).
	UWidget* FocusTarget = Widget->GetDefaultFocusWidget();
	TSharedRef<SWidget> FocusSlateWidget = FocusTarget ? FocusTarget->TakeWidget() : Widget->TakeWidget();

	// Only show the OS cursor when the mouse/keyboard is the active device. On gamepad the menu is
	// navigated by focus (D-pad / stick), so forcing the cursor on would wrongly bring it back
	// (e.g. after winning a puzzle while playing on gamepad).
	bool bShowCursor = true;
	if (const AML_PlayerController* MLPC = Cast<AML_PlayerController>(PC))
		bShowCursor = !MLPC->IsGamepadActive();

	switch (Widget->InputMode)
	{
		case EML_WidgetInputMode::GameOnly:
			{
				FInputModeGameOnly Mode;
				Mode.SetConsumeCaptureMouseDown(false);
				PC->SetInputMode(Mode);
				PC->SetShowMouseCursor(bShowCursor);
				break;
			}

		case EML_WidgetInputMode::UIOnly:
			{
				FInputModeUIOnly Mode;
				Mode.SetWidgetToFocus(FocusSlateWidget);
				Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
				PC->SetInputMode(Mode);
				PC->SetShowMouseCursor(bShowCursor);
				break;
			}

		case EML_WidgetInputMode::GameAndUI:
			{
				FInputModeGameAndUI Mode;
				Mode.SetWidgetToFocus(FocusSlateWidget);
				Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
				Mode.SetHideCursorDuringCapture(false);
				PC->SetInputMode(Mode);
				PC->SetShowMouseCursor(bShowCursor);
				break;
			}

		default:
			break;
	}

	SetFocusDeferred(Widget);
}

void UML_UIManagerSubsystem::SetFocusDeferred(UML_WidgetBase* Widget)
{
	if (!Widget) return;
	UWorld* World = GetWorld();
	if (!World) return;

	TWeakObjectPtr<UML_WidgetBase> WeakWidget = Widget;
	World->GetTimerManager().ClearTimer(PendingFocusTimer);
	World->GetTimerManager().SetTimer(PendingFocusTimer, [WeakWidget]()
	{
		if (!WeakWidget.IsValid()) return;
		// Resolve the focus target now (not when the timer was scheduled): the widget tree is
		// settled by this point. GetDefaultFocusWidget() must return an interactive *primitive*
		// (e.g. a UButton, whose cached SWidget is the focusable SButton) — never a UUserWidget
		// wrapper, whose SObjectWidget would receive focus instead of the button and swallow Enter.
		if (UWidget* FocusTarget = WeakWidget->GetDefaultFocusWidget())
			FocusTarget->SetFocus();
	}, 0.1f, false);
}

void UML_UIManagerSubsystem::SubscribeToDeviceChanges()
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC) return;

	UML_InputDeviceManager* DevMgr = PC->FindComponentByClass<UML_InputDeviceManager>();
	if (!DevMgr) return;

	DevMgr->OnInputDeviceChanged.RemoveDynamic(this, &UML_UIManagerSubsystem::HandleInputDeviceChanged);
	DevMgr->OnInputDeviceChanged.AddDynamic(this, &UML_UIManagerSubsystem::HandleInputDeviceChanged);
}

void UML_UIManagerSubsystem::UnsubscribeFromDeviceChanges()
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC) return;

	UML_InputDeviceManager* DevMgr = PC->FindComponentByClass<UML_InputDeviceManager>();
	if (!DevMgr) return;

	DevMgr->OnInputDeviceChanged.RemoveDynamic(this, &UML_UIManagerSubsystem::HandleInputDeviceChanged);
}

void UML_UIManagerSubsystem::HandleInputDeviceChanged(EML_InputDevice NewDevice)
{
	if (NewDevice != EML_InputDevice::Gamepad) return;
	if (!CurrentWidgetTag.IsValid()) return;

	UML_WidgetBase* Widget = Cast<UML_WidgetBase>(RegisteredWidgets.FindRef(CurrentWidgetTag));
	if (!Widget || !Widget->bReceivesGamepadFocus) return;

	SetFocusDeferred(Widget);
}

void UML_UIManagerSubsystem::RegisterContextRoot(FGameplayTag InContextTag, UUserWidget* InWidget)
{
	ensureMsgf(InWidget, TEXT("Trying to register null context root widget"));
	if (!InWidget) return;
    
	// Clear previous context and widgets when switching contexts (e.g., Menu -> Game)
	if (CurrentContextRoot && CurrentContextRoot != InWidget)
		UnregisterAllWidgets();
    
	CurrentContextRoot = InWidget;
	CurrentContextTag = InContextTag;
	CurrentWidgetTag = FGameplayTag();

	SubscribeToDeviceChanges();

	UE_LOG(LogTemp, Log, TEXT("UI Manager: Registered context root '%s'"), *InContextTag.ToString());
}

void UML_UIManagerSubsystem::RegisterWidget(FGameplayTag InWidgetTag, UUserWidget* InWidget)
{
	ensureMsgf(InWidget, TEXT("Trying to register null widget"));
	if (!InWidget) return;
    
	ensureMsgf(CurrentContextRoot, TEXT("No context root registered! Call RegisterContextRoot first."));
	if (!CurrentContextRoot) return;
    
	RegisteredWidgets.Add(InWidgetTag, InWidget);
    
	UE_LOG(LogTemp, Log, TEXT("UI Manager: Registered widget '%s' in context '%s'"), *InWidgetTag.ToString(), *CurrentContextTag.ToString());
}

void UML_UIManagerSubsystem::UnregisterAllWidgets()
{
	UnsubscribeFromDeviceChanges();

	RegisteredWidgets.Empty();
	NavigationStack.Empty();
	CurrentWidgetTag = FGameplayTag();

	UE_LOG(LogTemp, Log, TEXT("UI Manager: Unregistered all widgets"));
}

void UML_UIManagerSubsystem::NavigateTo(FGameplayTag InWidgetTag)
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

void UML_UIManagerSubsystem::GoBackTo(FGameplayTag InWidgetTag)
{
	ensureMsgf(NavigationStack.Num() > 0, TEXT("Navigation stack empty"));
	if (NavigationStack.Num() <= 0) return;
	
	for (int32 i = NavigationStack.Num() - 1; i >= 0; i--)
	{
		if (NavigationStack[i] == InWidgetTag)
		{
			const int32 RemoveStartIndex = i + 1;
			const int32 RemoveCount = NavigationStack.Num() - RemoveStartIndex;
			
			if (RemoveCount > 0)
				NavigationStack.RemoveAt(RemoveStartIndex, RemoveCount);
			
			SwitchWidgetInternal(InWidgetTag, false);
			return;
		}
	}
}

void UML_UIManagerSubsystem::ClearNavigationStack()
{
	NavigationStack.Empty();
}

void UML_UIManagerSubsystem::OpenLevelByTag(FGameplayTag Level, UObject* WorldContextObject)
{
	if (!WorldContextObject) return;

	const UML_MycelandDeveloperSettings* Settings = GetDefault<UML_MycelandDeveloperSettings>();
	if (!Settings) return;

	if (const TSoftObjectPtr<UWorld>* FoundLevel = Settings->Levels.Find(Level))
	{
		// Clear navigation before switching levels
		ClearNavigationStack();
        
		// Get the long package name (e.g., "/Game/Myceland/Level/LevelSelectionMenu")
		FString PackageName = FoundLevel->GetLongPackageName();
		if (PackageName.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("Level '%s' has an empty package name"), *Level.ToString());
			return;
		}
        
		UE_LOG(LogTemp, Log, TEXT("Opening level package: %s"), *PackageName);
		UGameplayStatics::OpenLevel(WorldContextObject, FName(*PackageName));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Level '%s' not found in Developer Settings"), *Level.ToString());
	}
}
