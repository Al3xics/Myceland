// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "MycelandGameUserSettings.generated.h"

/**
 * 
 */
UCLASS(Blueprintable, BlueprintType)
class MYCELAND_API UMycelandGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()
	public:
	
	virtual float GetFrameRateLimit() const override;
	virtual void SetFrameRateLimit(float NewLimit) override;

	virtual int32 GetOverallScalabilityLevel() const override;
	virtual void SetOverallScalabilityLevel(int32 Value) override;

	void ApplyAndSaveSettings();
	
};
