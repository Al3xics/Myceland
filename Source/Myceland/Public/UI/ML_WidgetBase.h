// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/ML_CoreData.h"
#include "ML_WidgetBase.generated.h"

UCLASS()
class MYCELAND_API UML_WidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Myceland UI")
	EML_WidgetInputMode InputMode = EML_WidgetInputMode::UIOnly;

	// If true, this widget will claim gamepad focus when the player switches to gamepad.
	// Set to false on HUD widgets where gamepad buttons are bound to gameplay actions.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Myceland UI")
	bool bReceivesGamepadFocus = true;

	// Returns the widget that should receive keyboard/gamepad focus when this widget activates.
	// Override in Blueprint to return your first interactive button. IMPORTANT: return an
	// interactive *primitive* (a UButton, etc.) whose backing SWidget is focusable — NOT a
	// UUserWidget wrapper, otherwise its SObjectWidget takes focus and Enter/accept won't fire.
	UFUNCTION(BlueprintNativeEvent, Category="Myceland UI")
	UWidget* GetDefaultFocusWidget();
	virtual UWidget* GetDefaultFocusWidget_Implementation();

	UFUNCTION(BlueprintNativeEvent, Category="UI")
	void OnActivated();
	virtual void OnActivated_Implementation();

	UFUNCTION(BlueprintNativeEvent, Category="UI")
	void OnDeactivated();
	virtual void OnDeactivated_Implementation();
};
