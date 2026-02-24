// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Core/ML_CoreData.h"
#include "Engine/DeveloperSettings.h"
#include "InputMappingContext.h"
#include "ML_MycelandDeveloperSettings.generated.h"

class AML_Collectible;
class UML_PropagationWaves;

UCLASS(config=Game, defaultconfig, meta=(DisplayName="Myceland"))
class MYCELAND_API UML_MycelandDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// ==================== Input ====================
	
	UPROPERTY(EditAnywhere, config, Category="Input")
	FML_InputMappingEntry DefaultInputMappingContext;
	
	
	
	// ==================== Levels ====================
	
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category="Levels", meta=(ForceInlineRow, Categories="Level"))
	TMap<FGameplayTag, TSoftObjectPtr<UWorld>> Levels;
	
	
	
	// ==================== Wave Propagation ====================
	
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category="Waves Propagation")
	TArray<TSubclassOf<UML_PropagationWaves>> WavesPriority;
	
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category="Wave Propagation", meta=(Tooltip="Delay between each global waves (grass, DELAY, parasite, DELAY, water, DELAY, etc..."))
	float InterWaveDelay = 1.f;
	
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category="Wave Propagation", meta=(Tooltip="Delay between each tiles in a wave (tile distance 1 (from clicked tile), DELAY, distance 2, DELAY, etc...)"))
	float IntraWaveDelay = 0.3f;
	
	
	
	// ==================== Helper ====================
	
	UFUNCTION(BlueprintPure, Category="Myceland Settings")
	static const UML_MycelandDeveloperSettings* GetMycelandDeveloperSettings()
	{
		return GetDefault<UML_MycelandDeveloperSettings>();
	}
	
	UFUNCTION(BlueprintPure, Category="Myceland Settings")
	UInputMappingContext* GetDefaultInputMappingContext(int32& Priority) const
	{
		if (DefaultInputMappingContext.Mapping.IsNull())
			return nullptr;

		return DefaultInputMappingContext.Mapping.LoadSynchronous();
	}
};
