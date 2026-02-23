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
	
	UFUNCTION(BlueprintNativeEvent, Category="UI")
	void OnActivated();
	virtual void OnActivated_Implementation();

	UFUNCTION(BlueprintNativeEvent, Category="UI")
	void OnDeactivated();
	virtual void OnDeactivated_Implementation();
};
