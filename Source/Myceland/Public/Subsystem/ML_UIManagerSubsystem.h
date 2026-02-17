// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Core/CoreData.h"
#include "ML_UIManagerSubsystem.generated.h"

class UML_WidgetBase;
class UML_RootWidgetBase;

UCLASS()
class MYCELAND_API UML_UIManagerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

private :
	UPROPERTY(Transient)
	TMap<FGameplayTag, UUserWidget*> RegisteredRootWidgets;

	UPROPERTY(Transient)
	TMap<FML_WidgetRegistryKey, UUserWidget*> RegisteredWidgetsInRoot;
	
	UPROPERTY(Transient)
	TMap<FGameplayTag, FML_WidgetRegistryKey> WidgetTagToRegistryKey;
	
	UPROPERTY(Transient)
	TArray<FGameplayTag> NavigationStack;
	
	UPROPERTY(Transient)
	FGameplayTag CurrentWidgetTag;
	
	void SwitchWidgetInternal(FGameplayTag InWidgetTag, bool bAddToStack);
	void ApplyInputModeFromWidget(UML_WidgetBase* Widget) const;

public:
	UFUNCTION(BlueprintCallable, Category = "Myceland UI Manager|Register")
	void RegisterRootWidget(UPARAM(meta=(Categories="UI.Root")) FGameplayTag InRootTag, UUserWidget* InWidget);
	
	UFUNCTION(BlueprintCallable, Category = "Myceland UI Manager|Register")
	void RegisterWidgetInRootWidget(UPARAM(meta=(Categories="UI.Root")) FGameplayTag InRootTag, UPARAM(meta=(Categories="UI.Widget")) FGameplayTag InWidgetTag, UUserWidget* InWidget);
	
	UFUNCTION(BlueprintCallable, Category = "Myceland UI Manager|Finder")
	const UUserWidget* FindRootWidgetByTag(UPARAM(meta=(Categories="UI.Root")) FGameplayTag InRootTag);
	
	UFUNCTION(BlueprintCallable, Category = "Myceland UI Manager|Finder")
	const UUserWidget* FindWidgetInRootByTag(UPARAM(meta=(Categories="UI.Root")) FGameplayTag InRootTag, UPARAM(meta=(Categories="UI.Widget")) FGameplayTag InWidgetTag);
	
	UFUNCTION(BlueprintCallable, Category = "Myceland UI Manager|Navigation")
	void NavigateTo(UPARAM(meta=(Categories="UI.Widget")) FGameplayTag InWidgetTag);
	
	UFUNCTION(BlueprintCallable, Category = "Myceland UI Manager|Navigation")
	void GoBack();
	
	UFUNCTION(BlueprintCallable, Category="Myceland UI Manager|Levels", meta=(WorldContext="WorldContextObject"))
	static void OpenLevelByTag(UPARAM(meta=(Categories="Level")) FGameplayTag Level, UObject* WorldContextObject);
};
