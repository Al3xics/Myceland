// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/HorizontalBox.h"
#include "Components/WidgetSwitcher.h"
#include "Core/ML_CoreData.h"
#include "ML_TabContainerWidget.generated.h"

class UML_TabButtonWidget;

UCLASS()
class MYCELAND_API UML_TabContainerWidget : public UUserWidget
{
	GENERATED_BODY()

private:
	int32 PreviousTabIndex = 0;
	int32 CurrentTabIndex = 0;
	
	UPROPERTY()
	TArray<UML_TabButtonWidget*> TabButtons;
    
	UFUNCTION()
	void OnTabButtonClicked(int32 TabIndex);
    
	void UpdateTabButtonStates();
	
protected:
	UPROPERTY(meta=(BindWidget))
	UHorizontalBox* TabButtonsContainer;
    
	UPROPERTY(meta=(BindWidget))
	UWidgetSwitcher* ContentSwitcher;
	
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Myceland Tab")
	TArray<FML_TabData> Tabs;
    
	UPROPERTY(EditInstanceOnly, Category = "Myceland Tab")
	TSubclassOf<UML_TabButtonWidget> TabButtonClass;
    
	virtual void NativeConstruct() override;

public:
	UFUNCTION(BlueprintCallable, Category = "Myceland Tab")
	void InitializeTabs();
    
	UFUNCTION(BlueprintCallable, Category = "Myceland Tab")
	void SwitchToTab(int32 TabIndex);
    
	UFUNCTION(BlueprintCallable, Category = "Myceland Tab")
	int32 GetCurrentTabIndex() const { return CurrentTabIndex; }
};
