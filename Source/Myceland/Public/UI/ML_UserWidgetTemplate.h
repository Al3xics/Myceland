// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/ML_CoreData.h"
#include "ML_UserWidgetTemplate.generated.h"

enum class ESettingValueType : uint8;

UCLASS()
class MYCELAND_API UML_UserWidgetTemplate : public UUserWidget
{
	GENERATED_BODY()
	
private :
	// Flag to prevent triggering events during initialization
	bool bIsInitializing = false;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	
	UFUNCTION()
	void OnSettingsLoadedCallback();

public:
	/**
	 * Name of the property to load from GameUserSettings
	 * Must match EXACTLY (case-sensitive, no spaces)
	 * Examples: "MasterVolume", "bShowGrid", "ColorblindMode"
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Myceland Settings", meta = (ExposeOnSpawn = "true"))
	FName PropertyName;
	
	/**
	 * Called to load and display the current setting value
	 * Override this in child Blueprint widgets
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Myceland Settings")
	void LoadFromSettings();
	virtual void LoadFromSettings_Implementation();
	
	UFUNCTION(BlueprintPure, Category = "Myceland Settings")
	bool IsInitializing() const { return bIsInitializing; }

	/**
	 * Get setting value using the widget's PropertyName variable using reflection
	 * Property name must match EXACTLY (case-sensitive, no spaces)
	 * Example: "MasterVolume", "bShowGrid", "ColorblindMode"
	 */
	UFUNCTION(BlueprintCallable, Category = "Myceland Settings", meta = (ExpandEnumAsExecs = "OutType"))
	void GetSettingValue(float& OutFloat, bool& OutBool, int32& OutInt32, ESettingValueType& OutType);
	
	UFUNCTION(BlueprintPure, Category = "Myceland Settings")
	FString GetOptionTextValue() const;
	
	UFUNCTION(BlueprintPure, Category = "Myceland Settings")
	int32 FindTextOptionIndex(const TArray<FText>& Options, const FString& SearchText);
};
