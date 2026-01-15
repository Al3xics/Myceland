// Fill out your copyright notice in the Description page of Project Settings.


#include "User Settings/MycelandGameUserSettings.h"

void UMycelandGameUserSettings::SetFrameRateLimit(float NewLimit)
{
	SetFrameRateLimitCVar(NewLimit);
	ApplyAndSaveSettings();
}

float UMycelandGameUserSettings::GetFrameRateLimit() const
{
	return FrameRateLimit;
}

int32 UMycelandGameUserSettings::GetOverallScalabilityLevel() const 
{
	return ScalabilityQuality.GetSingleQualityLevel();
}

void UMycelandGameUserSettings::SetOverallScalabilityLevel(int32 Value)
{
	ScalabilityQuality.SetFromSingleQualityLevel(Value);
	ApplyAndSaveSettings();
}

void UMycelandGameUserSettings::ApplyAndSaveSettings()
{
	ApplySettings(false);
	SaveSettings();
}

