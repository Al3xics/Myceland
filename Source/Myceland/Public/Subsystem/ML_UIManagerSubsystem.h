// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Core/ML_CoreData.h"
#include "ML_UIManagerSubsystem.generated.h"

class UML_WidgetBase;
class UML_RootWidgetBase;

UCLASS()
class MYCELAND_API UML_UIManagerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

private :
	// Current active context root widget (Menu or Game)
	UPROPERTY(Transient)
	UUserWidget* CurrentContextRoot = nullptr;
	
	UPROPERTY(Transient)
	FGameplayTag CurrentContextTag;
	
	// Widgets registered in the CURRENT context
	UPROPERTY(Transient)
	TMap<FGameplayTag, UUserWidget*> RegisteredWidgets;
	
	UPROPERTY(Transient)
	TArray<FGameplayTag> NavigationStack;
	
	UPROPERTY(Transient)
	FGameplayTag CurrentWidgetTag;
	
	void SwitchWidgetInternal(FGameplayTag InWidgetTag, bool bAddToStack);
	void ApplyInputModeFromWidget(UML_WidgetBase* Widget) const;

public:
	// Register the active context root widget (called when a level loads)
	UFUNCTION(BlueprintCallable, Category = "Myceland UI Manager|Register")
	void RegisterContextRoot(UPARAM(meta=(Categories="UI.Context")) FGameplayTag InContextTag, UUserWidget* InWidget);
    
	// Register a widget in the CURRENT active context
	UFUNCTION(BlueprintCallable, Category = "Myceland UI Manager|Register")
	void RegisterWidget(UPARAM(meta=(Categories="UI.Widget")) FGameplayTag InWidgetTag, UUserWidget* InWidget);
    
	// Unregister all widgets (called when switching levels)
	UFUNCTION(BlueprintCallable, Category = "Myceland UI Manager|Register")
	void UnregisterAllWidgets();
    
	UFUNCTION(BlueprintCallable, Category = "Myceland UI Manager|Navigation")
	void NavigateTo(UPARAM(meta=(Categories="UI.Widget")) FGameplayTag InWidgetTag);
    
	UFUNCTION(BlueprintCallable, Category = "Myceland UI Manager|Navigation")
	void GoBack();
    
	UFUNCTION(BlueprintCallable, Category = "Myceland UI Manager|Navigation")
	void GoBackTo(UPARAM(meta=(Categories="UI.Widget")) FGameplayTag InWidgetTag);
    
	UFUNCTION(BlueprintCallable, Category = "Myceland UI Manager|Navigation")
	void ClearNavigationStack();
    
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Myceland UI Manager|Info")
	FGameplayTag GetCurrentContextTag() const { return CurrentContextTag; }
    
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Myceland UI Manager|Info")
	FGameplayTag GetCurrentWidgetTag() const { return CurrentWidgetTag; }
    
	UFUNCTION(BlueprintCallable, Category="Myceland UI Manager|Levels", meta=(WorldContext="WorldContextObject"))
	void OpenLevelByTag(UPARAM(meta=(Categories="Level")) FGameplayTag Level, UObject* WorldContextObject);
};
