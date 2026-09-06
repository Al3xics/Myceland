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

USTRUCT(BlueprintType)
struct FML_LevelAmbiencePuzzleCount
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ambience", meta=(Categories="Level"))
	FGameplayTag Level;

	// Total number of puzzle wins needed for this level to reach 100% living ambience.
	// Include the central hub as the final puzzle when the level uses one.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ambience", meta=(ClampMin="1"))
	int32 PuzzleCount = 1;
};

UCLASS(config=Game, defaultconfig, meta=(DisplayName="Myceland"))
class MYCELAND_API UML_MycelandDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// ==================== Input ====================

	// All gameplay IMCs, mapped together while not in a cinematic. Keep BOTH the mouse/keyboard and the
	// gamepad IMC here: device detection piggy-backs on their action callbacks, so both must stay mapped
	// for the gamepad <-> mouse/keyboard switch to work in both directions.
	UPROPERTY(EditAnywhere, config, Category="Input|IMC")
	TArray<FML_InputMappingEntry> GameplayInputMappingContexts;

	// IMC used during cinematic sequences — should contain only the skip action
	UPROPERTY(EditAnywhere, config, Category="Input|IMC")
	FML_InputMappingEntry CinematicInputMappingContext;

	UPROPERTY(EditAnywhere, config, Category="Input|IMC")
	FML_InputMappingEntry TeleportInputMappingContext;

	// Mouse/keyboard: how long the click must be held outside the board before the exit resolves. 0 = instant.
	// Shares the exit hold tick rate (ExitHoldTickRate) with the gamepad.
	UPROPERTY(EditAnywhere, config, Category="Input|Mouse", meta=(ClampMin="0.0", Tooltip="Mouse/keyboard: seconds the click must be held outside the board before leaving. 0 = instant. Shares ExitHoldTickRate with the gamepad."))
	float ExitBoardHoldDurationMouse = 0.f;

	// Gamepad: how long the stick must be held toward the exit plane before the exit resolves. 0 = instant.
	// Shares the exit hold tick rate (ExitHoldTickRate) with the mouse/keyboard.
	UPROPERTY(EditAnywhere, config, Category="Input|Gamepad", meta=(ClampMin="0.0", Tooltip="Gamepad: seconds the stick must be held toward the exit plane before leaving. 0 = instant. Shares ExitHoldTickRate with the mouse."))
	float ExitBoardHoldDurationGamepad = 0.f;

	// ---------- Gamepad · Left Stick (Move) ----------

	// Gamepad left stick (inside board): seconds the stick must be held before the player starts
	// auto-stepping to the next tile (the first tile is always stepped immediately on push).
	UPROPERTY(EditAnywhere, config, Category="Input|Gamepad|Left Stick (Move)", meta=(ClampMin="0.0", Tooltip="Seconds the left stick must be held before the player starts auto-stepping to the next tile (first step is immediate)."))
	float GamepadMoveHoldRepeatDelay = 0.3f;

	// Gamepad left stick: once auto-stepping is active, seconds between each step to the next tile.
	// Lower = faster.
	UPROPERTY(EditAnywhere, config, Category="Input|Gamepad|Left Stick (Move)", meta=(ClampMin="0.01", Tooltip="Seconds between each tile step while the left stick is held (lower = faster)."))
	float GamepadMoveHoldRepeatInterval = 0.1f;

	// Gamepad left stick (inside board): minimum stick/direction alignment (dot product) required to step
	// the player toward a neighbor tile. cos 60 deg = 0.5, cos 30 deg = 0.866.
	UPROPERTY(EditAnywhere, config, Category="Input|Gamepad|Left Stick (Move)", meta=(ClampMin="0.0", ClampMax="1.0", Tooltip="Minimum alignment (dot product) for the left stick to step toward a neighbor tile. 0.5 = cos 60 deg."))
	float GamepadMoveAlignmentThreshold = 0.5f;

	// ---------- Gamepad · Right Stick (Select) ----------

	// Gamepad right stick (inside board): seconds the stick must be held before the selection cursor starts
	// auto-advancing to the next tile (the first tile is always selected immediately on push).
	UPROPERTY(EditAnywhere, config, Category="Input|Gamepad|Right Stick (Select)", meta=(ClampMin="0.0", Tooltip="Seconds the right stick must be held before the tile selection starts auto-advancing (first selection is immediate)."))
	float GamepadSelectHoldRepeatDelay = 0.3f;

	// Gamepad right stick: once auto-advance is active, seconds between each step to the next tile.
	// Lower = faster. Acts as the hold movement speed of the selection cursor.
	UPROPERTY(EditAnywhere, config, Category="Input|Gamepad|Right Stick (Select)", meta=(ClampMin="0.01", Tooltip="Seconds between each tile step while the right stick is held (lower = faster)."))
	float GamepadSelectHoldRepeatInterval = 0.1f;

	// Gamepad right stick (inside board): minimum stick/direction alignment (dot product) required to
	// select a tile around the player. cos 60 deg = 0.5, cos 30 deg = 0.866.
	UPROPERTY(EditAnywhere, config, Category="Input|Gamepad|Right Stick (Select)", meta=(ClampMin="0.0", ClampMax="1.0", Tooltip="Minimum alignment (dot product) for the right stick to select a tile. 0.5 = cos 60 deg."))
	float GamepadSelectAlignmentThreshold = 0.5f;

	UPROPERTY(EditAnywhere, Config, Category="Input|Gamepad|Right Stick (Select)", meta=(ClampMin="1", Tooltip="The distance from the player a tile can be selected with the gamepad."))
	uint8 GamepadSelectRingDistance = 1;



	// ==================== User Settings (Graphics whitelists) ====================

	// Resolutions offered in the graphics settings dropdown. The saved resolution
	// snaps to the closest entry when the game boots (ValidateSettings).
	UPROPERTY(EditAnywhere, config, Category="User Settings|Graphics")
	TArray<FIntPoint> ValidResolutions = {
		FIntPoint(1280, 720),
		FIntPoint(1600, 900),
		FIntPoint(1920, 1080),
		FIntPoint(2560, 1440),
		FIntPoint(3840, 2160)
	};

	// Frame rate limits offered in the graphics settings dropdown. 0 = Unlimited.
	UPROPERTY(EditAnywhere, config, Category="User Settings|Graphics")
	TArray<float> ValidFrameLimits = {
		30.f,
		60.f,
		120.f,
		144.f,
		0.f
	};



	// ==================== Tick Rates ====================

	// Every cursor detection timer shares this rate: tile hover preview (glow + path preview)
	// and ground hover detection (board exit).
	UPROPERTY(EditAnywhere, config, Category="Tick Rates", meta=(ClampMin="1.0", Units="Hz", Tooltip="Rate of the cursor detection timers (tile hover preview + ground hover). 30 Hz is enough for cursor feedback."))
	float CursorDetectionTickRate = 30.f;

	// Rate of the exit hold progression timer (the hold-to-leave-the-board gauge).
	UPROPERTY(EditAnywhere, config, Category="Tick Rates", meta=(ClampMin="1.0", Units="Hz", Tooltip="Rate of the exit hold progression timer (hold-to-leave-the-board gauge)."))
	float ExitHoldTickRate = 60.f;

	// Rate of the turn-toward-tile rotation timer (character turning to face the plant target).
	UPROPERTY(EditAnywhere, config, Category="Tick Rates", meta=(ClampMin="1.0", Units="Hz", Tooltip="Rate of the turn-toward-tile rotation timer (character turning to face the plant target)."))
	float TurnTowardTileTickRate = 60.f;

	// Timer-ready intervals (seconds). Clamped so a bad config value can never yield a zero/negative rate.
	float GetCursorDetectionTickInterval() const { return 1.f / FMath::Max(CursorDetectionTickRate, 1.f); }
	float GetExitHoldTickInterval() const { return 1.f / FMath::Max(ExitHoldTickRate, 1.f); }
	float GetTurnTowardTileTickInterval() const { return 1.f / FMath::Max(TurnTowardTileTickRate, 1.f); }



	// ==================== UI ====================

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category="UI")
	float TimeShowWinUI = 3.f;

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category="UI")
	float DelayBeforeShowLoseUI = 0.f;



	// ==================== Levels ====================

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category="Levels", meta=(ForceInlineRow, Categories="Level"))
	TMap<FGameplayTag, TSoftObjectPtr<UWorld>> Levels;



	// ==================== Audio ====================

	UPROPERTY(EditAnywhere, config, Category="Audio|FMOD VCA")
	FString MasterFMODVCAPath = TEXT("vca:/Master");

	UPROPERTY(EditAnywhere, config, Category="Audio|FMOD VCA")
	FString MusicFMODVCAPath = TEXT("vca:/Music");

	UPROPERTY(EditAnywhere, config, Category="Audio|FMOD VCA")
	FString SFXFMODVCAPath = TEXT("vca:/SFX");

	UPROPERTY(EditAnywhere, config, Category="Audio|FMOD VCA")
	FString VoiceFMODVCAPath = TEXT("vca:/Voice");

	UPROPERTY(EditAnywhere, config, Category="Audio|Ambience Enviro")
	TArray<FString> LivingAmbienceEventPaths;

	UPROPERTY(EditAnywhere, config, Category="Audio|Ambience Enviro")
	TArray<FString> DeadAmbienceEventPaths;

	UPROPERTY(EditAnywhere, config, Category="Audio|Ambience Enviro", meta=(ClampMin="0.0"))
	float AmbienceMinDelay = 3.0f;

	UPROPERTY(EditAnywhere, config, Category="Audio|Ambience Enviro", meta=(ClampMin="0.0"))
	float AmbienceMaxDelay = 5.0f;

	UPROPERTY(EditAnywhere, config, Category="Audio|Ambience Enviro")
	bool bAutoStartAmbienceEnviro = true;

	UPROPERTY(EditAnywhere, config, Category="Audio|Ambience Enviro")
	bool bFallbackToBoardSpawnerCount = true;

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category="Audio|Ambience Enviro", meta=(TitleProperty="Level"))
	TArray<FML_LevelAmbiencePuzzleCount> AmbiencePuzzleCounts;

	// ==================== Audio · Music Progression ====================

	// Ordered list of FMOD event paths (event:/...), one exclusive track per progression step.
	// Only one plays at a time: index 0 ("Musique 1") plays from 0 puzzles won, index 1
	// ("Musique 2") from 1 puzzle won, etc. Winning a puzzle switches the track instead of
	// layering on top of it. Progress past the last entry just keeps the last track playing.
	UPROPERTY(EditAnywhere, config, Category="Audio|Music Progression")
	TArray<FString> MusicTrackEventPaths;

	UPROPERTY(EditAnywhere, config, Category="Audio|Music Progression")
	bool bAutoStartMusicProgression = true;

	// ---------- Ancienne logique (layers additifs) — conservée en commentaire pour référence ----------
	// UPROPERTY(EditAnywhere, config, Category="Audio|Music Layers")
	// TArray<FString> MusicLayerEventPaths;
	//
	// UPROPERTY(EditAnywhere, config, Category="Audio|Music Layers")
	// bool bAutoStartMusicLayers = true;



	// ==================== Wave Propagation ====================

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category="Waves Propagation")
	TArray<FML_WavePriorityEntry> WavesPriority;

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category="Wave Propagation", meta=(Tooltip="Delay between each global waves (grass, DELAY, parasite, DELAY, water, DELAY, etc..."))
	float InterWaveDelay = 1.f;

	UPROPERTY(EditAnywhere, config, BlueprintReadWrite, Category="Wave Propagation", meta=(Tooltip="Delay between each tiles in a wave (tile distance 1 (from clicked tile), DELAY, distance 2, DELAY, etc...)"))
	float IntraWaveDelay = 0.3f;

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category="Wave Propagation",
	meta=(ClampMin="0.0", Tooltip="Delay before a tile visually becomes Grass."))
	float GrassSpawnDelay = 0.3f;

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category="Wave Propagation",
		meta=(ClampMin="0.0", Tooltip="Delay between Grass StartTransition and the tile becoming Parasite."))
	float GrassToParasiteDelay = 0.5f;

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category="Waves Propagation")
	float UndoSpeed = 3.0f;

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category="Waves Propagation", meta=(DisplayName="Undo Until Plant", Tooltip="When enabled, undo keeps going through Move actions until it also undoes the next Plant action."))
	bool bUndoUntilPlant = false;

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category="Waves Propagation")
	float ResetSpeed = 3.0f;

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category="Waves Propagation", meta=(DisplayName="Use Dynamic Rollback Speed", Tooltip="When enabled, undo and reset speeds are computed from the current rollback stack to target Reset Target Duration. When disabled, Undo Speed and Reset Speed are used directly."))
	bool bUseDynamicRollBackSpeed = true;

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category="Waves Propagation", meta=(DisplayName="Rollback Minimum Speed", ClampMin="0.01", Tooltip="Minimum time dilation used by animated undo and reset."))
	float RollBackMinSpeed = 3.0f;

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category="Waves Propagation", meta=(DisplayName="Rollback Maximum Speed", ClampMin="0.01", Tooltip="Maximum time dilation used by animated undo and reset. Limits movement speed to prevent the player from overshooting tiles or leaving the board."))
	float RollBackMaxSpeed = 20.0f;

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category="Waves Propagation", meta=(ClampMin="0.1", Tooltip="Target real-time duration for a full animated reset. The reset time dilation is computed from the current undo stack so larger stacks rewind faster."))
	float ResetTargetDuration = 4.0f;


	// ==================== Frame Budgets ====================
	// Per-frame CPU budgets (milliseconds) for the subsystems that time-slice their
	// work across frames. When the work of one step exceeds the budget, the
	// remainder continues on the following frames.

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category="Frame Budgets", meta=(ClampMin="0.1", Tooltip="Max CPU time (milliseconds) the wave propagation may spend applying tile changes in a single frame. When a ring has more tiles than fit in the budget, the remaining tiles are applied on the following frames."))
	float WavePropagationFrameBudgetMs = 2.0f;

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category="Frame Budgets", meta=(ClampMin="0.1", Tooltip="Max CPU time (milliseconds) the animated undo/reset may spend reverting tiles and destroying spawned actors in a single frame. When an undo wave group is bigger than the budget, the remainder continues on the following frames."))
	float RollbackFrameBudgetMs = 8.0f;

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category="Frame Budgets", meta=(ClampMin="0.1", Tooltip="Max CPU time (milliseconds) the win propagation wave may spend applying tile changes in a single frame. When a ring has more tiles than fit in the budget, the remaining tiles are applied on the following frames."))
	float WinFrameBudgetMs = 2.0f;


	// ==================== Win ====================

	UPROPERTY(EditAnywhere,Category = "WinLose")
	float WinDelay = 0.5f;

	UPROPERTY(EditAnywhere,Category = "WinLose", meta=(ToolTip="Delay between each glow tile to show the win path (connected goals)."))
	float WinTileDelay = 0.1f;


	// ==================== Helper ====================
	UFUNCTION(BlueprintCallable, Category="Myceland Settings")
	static void SetIntraWaveDelay(float NewDelay)
	{
		UML_MycelandDeveloperSettings* Settings =
			GetMutableDefault<UML_MycelandDeveloperSettings>();

		Settings->IntraWaveDelay = NewDelay;
	}
	UFUNCTION(BlueprintPure, Category="Myceland Settings")
	static const UML_MycelandDeveloperSettings* GetMycelandDeveloperSettings()
	{
		return GetDefault<UML_MycelandDeveloperSettings>();
	}

	UFUNCTION(BlueprintPure, Category="Myceland Settings")
	UInputMappingContext* GetInputMappingContext(EInputMappingType Type, int32& Priority) const
	{
		const FML_InputMappingEntry* Entry;

		switch (Type)
		{
			case EInputMappingType::Teleport:
				Entry = &TeleportInputMappingContext;
				break;
			case EInputMappingType::Cinematic:
			default:
				Entry = &CinematicInputMappingContext;
				break;
		}

		Priority = Entry->Priority;

		// Resolve the soft reference: returns the asset if already in memory, loads it now
		// otherwise (IMCs are tiny assets). Returns nullptr if the entry is unset.
		return Entry->Mapping.LoadSynchronous();
	}
};
