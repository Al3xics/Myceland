// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "ML_TabButtonWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTabButtonClicked, int32, TabIndex);

UCLASS()
class MYCELAND_API UML_TabButtonWidget : public UUserWidget
{
	GENERATED_BODY()

private:
	UFUNCTION()
	void OnButtonClicked();
	
protected:
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	UButton* Button;
    
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	UTextBlock* TabText;
    
	virtual void NativeConstruct() override;
    
	UFUNCTION(BlueprintImplementableEvent, Category = "Tab|Visual")
	void OnVisualStateChanged(bool bInIsActive);
    
	UFUNCTION(BlueprintImplementableEvent, Category = "Tab|Animation")
	void PlayTabSwitchAnimation(int32 FromIndex, int32 ToIndex);

public:
	UPROPERTY(BlueprintAssignable, Category = "Tab")
	FOnTabButtonClicked OnTabClicked;
    
	UPROPERTY(BlueprintReadWrite, Category = "Tab")
	int32 TabIndex = 0;
    
	UPROPERTY(BlueprintReadWrite, Category = "Tab")
	bool bIsActive = false;
    
	UFUNCTION(BlueprintCallable, Category = "Tab")
	void SetTabData(const FText& InTabName, int32 InTabIndex);
    
	UFUNCTION(BlueprintCallable, Category = "Tab")
	void SetIsActive(bool bInActive);
	
	UFUNCTION(BlueprintCallable, Category = "Tab")
	void PlaySwitchAnimation(int32 FromIndex, int32 ToIndex);
};
