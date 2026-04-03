// Copyright Myceland Team, All Rights Reserved.


#include "UI/ML_UserWidgetTemplate.h"

#include "Core/ML_CoreData.h"
#include "User Settings/ML_GameUserSettings.h"

void UML_UserWidgetTemplate::NativeConstruct()
{
	Super::NativeConstruct();

	if (UML_GameUserSettings* Settings = UML_GameUserSettings::GetMycelandGameUserSettings())
		Settings->OnSettingsLoaded.AddDynamic(this, &UML_UserWidgetTemplate::OnSettingsLoadedCallback);
	
	OnSettingsLoadedCallback(); // force init
}

void UML_UserWidgetTemplate::NativeDestruct()
{
	// Unbind to avoid memory leaks
	if (UML_GameUserSettings* Settings = UML_GameUserSettings::GetMycelandGameUserSettings())
		Settings->OnSettingsLoaded.RemoveDynamic(this, &UML_UserWidgetTemplate::OnSettingsLoadedCallback);
	
	Super::NativeDestruct();
}

void UML_UserWidgetTemplate::OnSettingsLoadedCallback()
{
	// Refresh widget when settings are loaded
	bIsInitializing = true;
	LoadFromSettings();
	bIsInitializing = false;
}

void UML_UserWidgetTemplate::LoadFromSettings_Implementation()
{
}

void UML_UserWidgetTemplate::GetSettingValue(float& OutFloat, bool& OutBool, int32& OutInt32, ESettingValueType& OutType)
{
	// Reset outputs
	OutFloat = 0.0f;
	OutBool = false;
	OutInt32 = 0;
	OutType = ESettingValueType::Unknown;

	const UML_GameUserSettings* Settings = UML_GameUserSettings::GetMycelandGameUserSettings();
	if (!Settings)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to get GameUserSettings"));
		return;
	}
	
	// Find property by name using reflection
	FProperty* Property = Settings->GetClass()->FindPropertyByName(PropertyName);
	if (!Property)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] Property '%s' not found in GameUserSettings"), *GetName(), *PropertyName.ToString());
		return;
	}
	
	// Get property value based on type
	if (const FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
	{
		OutFloat = FloatProp->GetPropertyValue_InContainer(Settings);
		OutType = ESettingValueType::Float;
		UE_LOG(LogTemp, Verbose, TEXT("[%s] Loaded float property '%s' = %.2f"), *GetName(), *PropertyName.ToString(), OutFloat);
	}
	else if (const FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
	{
		OutBool = BoolProp->GetPropertyValue_InContainer(Settings);
		OutType = ESettingValueType::Bool;
		UE_LOG(LogTemp, Verbose, TEXT("[%s] Loaded bool property '%s' = %s"), *GetName(), *PropertyName.ToString(), OutBool ? TEXT("true") : TEXT("false"));
	}
	else if (const FIntProperty* IntProp = CastField<FIntProperty>(Property))
	{
		OutInt32 = IntProp->GetPropertyValue_InContainer(Settings);
		OutType = ESettingValueType::Int32;
		UE_LOG(LogTemp, Verbose, TEXT("[%s] Loaded int32 property '%s' = %d"), *GetName(), *PropertyName.ToString(), OutInt32);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] Property '%s' has unsupported type"), *GetName(), *PropertyName.ToString());
	}
}

FString UML_UserWidgetTemplate::GetOptionTextValue() const
{
	const UML_GameUserSettings* Settings = UML_GameUserSettings::GetMycelandGameUserSettings();
	if (!Settings) return TEXT("");
    
	// Check for special properties
	if (PropertyName == TEXT("ResolutionValue"))
		return Settings->GetCurrentResolutionText();
	if (PropertyName == TEXT("FrameLimit"))
		return Settings->GetCurrentFrameRateLimitText();
    
	return TEXT(""); // Not a special property
}

int32 UML_UserWidgetTemplate::FindTextOptionIndex(const TArray<FText>& Options, const FString& SearchText)
{
	for (int32 i = 0; i < Options.Num(); ++i)
		if (Options[i].ToString().Equals(SearchText, ESearchCase::IgnoreCase))
			return i;
    
	return INDEX_NONE;
}
