// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Core/ML_CoreData.h"
#include "Engine/DeveloperSettings.h"
#include "InputMappingContext.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
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

	// IMC used during cinematic sequences — should contain only the skip action
	UPROPERTY(EditAnywhere, config, Category="Input")
	FML_InputMappingEntry CinematicInputMappingContext;

	UPROPERTY(EditAnywhere, config, Category="Input")
	float ExitBoardHoldDuration = 2.f;

	// Gamepad: inside a board, how long the stick must be held in a direction before the
	// tile selection starts auto-advancing to the next tile (the first tile is always
	// selected immediately on push).
	UPROPERTY(EditAnywhere, config, Category="Input|Gamepad", meta=(ClampMin="0.0", Tooltip="Seconds the stick must be held before tile selection starts auto-advancing."))
	float GamepadHoldRepeatDelay = 0.4f;

	// Gamepad: once auto-advance is active, seconds between each step to the next tile.
	// Lower = faster. Acts as the hold movement speed of the selection cursor.
	UPROPERTY(EditAnywhere, config, Category="Input|Gamepad", meta=(ClampMin="0.01", Tooltip="Seconds between each tile step while the stick is held (lower = faster)."))
	float GamepadHoldRepeatInterval = 0.15f;



	// ==================== UI ====================
	
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category="UI")
	float TimeShowWinUI = 3.f;
	
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category="UI")
	float DelayBeforeShowLoseUI = 0.f;
	
	
	
	// ==================== Levels ====================
	
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category="Levels", meta=(ForceInlineRow, Categories="Level"))
	TMap<FGameplayTag, TSoftObjectPtr<UWorld>> Levels;
	
	
	
	// ==================== Audio ====================
    
	UPROPERTY(EditAnywhere, config, Category="Audio|Sound Classes")
	TSoftObjectPtr<USoundClass> MasterSoundClass;
    
	UPROPERTY(EditAnywhere, config, Category="Audio|Sound Classes")
	TSoftObjectPtr<USoundClass> MusicSoundClass;
    
	UPROPERTY(EditAnywhere, config, Category="Audio|Sound Classes")
	TSoftObjectPtr<USoundClass> SFXSoundClass;
    
	UPROPERTY(EditAnywhere, config, Category="Audio|Sound Classes")
	TSoftObjectPtr<USoundClass> UISoundClass;
    
	UPROPERTY(EditAnywhere, config, Category="Audio|Sound Classes")
	TSoftObjectPtr<USoundClass> VoiceSoundClass;
    
	UPROPERTY(EditAnywhere, config, Category="Audio|Sound Mix")
	TSoftObjectPtr<USoundMix> GameSoundMix;
	
	
	
	// ==================== Wave Propagation ====================
	
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category="Waves Propagation")
	TArray<FML_WavePriorityEntry> WavesPriority;
	
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category="Wave Propagation", meta=(Tooltip="Delay between each global waves (grass, DELAY, parasite, DELAY, water, DELAY, etc..."))
	float InterWaveDelay = 1.f;
	
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category="Wave Propagation", meta=(Tooltip="Delay between each tiles in a wave (tile distance 1 (from clicked tile), DELAY, distance 2, DELAY, etc...)"))
	float IntraWaveDelay = 0.3f;

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category="Waves Propagation")
	float UndoSpeed = 3.0f;

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category="Waves Propagation", meta=(DisplayName="Undo Until Plant", Tooltip="When enabled, undo keeps going through Move actions until it also undoes the next Plant action."))
	bool bUndoUntilPlant = false;

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category="Waves Propagation")
	float ResetSpeed = 3.0f;

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category="Waves Propagation", meta=(DisplayName="Use Dynamic Rollback Speed", Tooltip="When enabled, undo and reset speeds are computed from the current rollback stack to target Reset Target Duration. When disabled, Undo Speed and Reset Speed are used directly."))
	bool bUseDynamicRollBackSpeed = true;

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category="Waves Propagation", meta=(ClampMin="0.1", Tooltip="Target real-time duration for a full animated reset. The reset time dilation is computed from the current undo stack so larger stacks rewind faster."))
	float ResetTargetDuration = 4.0f;
	

	// ==================== Win ====================

	UPROPERTY(EditAnywhere,Category = "Myceland WinLose")
	float WinDelay = 0.5f;
	
	
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
