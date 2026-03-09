// Copyright Myceland Team, All Rights Reserved.


#include "Subsystem/ML_UIManagerSubsystem.h"

#include "Developer Settings/ML_MycelandDeveloperSettings.h"
#include "Kismet/GameplayStatics.h"
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
