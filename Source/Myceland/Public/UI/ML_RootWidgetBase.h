// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ML_RootWidgetBase.generated.h"

class UWidgetSwitcher;

UCLASS()
class MYCELAND_API UML_RootWidgetBase : public UUserWidget
{
	GENERATED_BODY()
	
protected:
	UPROPERTY(meta = (BindWidget))
	UWidgetSwitcher* WidgetSwitcher;

public:
	void SwitchToWidget(UUserWidget* Widget);
	
	UFUNCTION(BlueprintImplementableEvent, Category="Myceland UI")
	void PlayTransition(UUserWidget* FromWidget, UUserWidget* ToWidget);
};
