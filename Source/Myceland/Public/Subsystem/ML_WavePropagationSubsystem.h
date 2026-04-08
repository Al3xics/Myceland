// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Core/ML_UndoTypes.h"
#include "ML_WavePropagationSubsystem.generated.h"

struct FML_WaveChange;

class UML_MycelandDeveloperSettings;
class UML_WinLoseSubsystem;
class AML_PlayerController;
class AML_PlayerCharacter;
class AML_BoardSpawner;
class AML_Tile;
class AML_Collectible;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUndoAnimating, bool, IsUndoAnimating);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnResetAnimating, bool, IsResetAnimating);

UCLASS()
class MYCELAND_API UML_WavePropagationSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void EnsureInitialized();

	// Called by collectible wave BEFORE flipping HasCollectible flag.
	void RecordTileForUndo(AML_Tile* Tile, int32 DistanceFromOrigin);

	UFUNCTION(BlueprintCallable, Category="Myceland Wave Propagation")
	void BeginTileResolved(AML_Tile* HitTile);

	UFUNCTION(BlueprintPure, Category="Undo")
	bool CanUndo() const
	{
		return !bIsResolvingTiles && !bIsUndoAnimating && ActionUndoStack.Num() > 0;
	}

	// UI button
	UFUNCTION(BlueprintCallable, Category="Undo")
	bool UndoLastAction_Animated();

	// Called by PlayerController when an undo-move playback ends.
	UFUNCTION()
	void FinishUndoAnimation();

	// Called during undo-move playback to restore a collectible on a tile the player just left.
	UFUNCTION()
	bool RestoreCollectibleDuringUndoMove(const FIntPoint& Axial);

	// Called by PlayerController when a normal move ends successfully.
	void NotifyMoveCompleted(
		const FIntPoint& StartAxial,
		const FIntPoint& EndAxial,
		const TArray<FIntPoint>& AxialPath,
		const FVector& StartWorld,
		const FVector& EndWorld,
		const TArray<FIntPoint>& PickedCollectibleAxials
	);

	UFUNCTION(BlueprintCallable, Category="Reset")
	bool ResetAllActions_Animated();

	bool ResetAllActions_ExcludingMoves_Animated();

	UPROPERTY(BlueprintAssignable, Category="Undo")
	FOnUndoAnimating OnUndoAnimating;

	UPROPERTY(BlueprintAssignable, Category="Reset")
	FOnResetAnimating OnResetAnimating;

private:
	// =========================================================================
	// Core references / initialization
	// =========================================================================

	UPROPERTY()
	UML_WinLoseSubsystem* WinLoseSubsystem = nullptr;

	UPROPERTY()
	AML_PlayerController* PlayerController = nullptr;

	UPROPERTY()
	const UML_MycelandDeveloperSettings* DevSettings = nullptr;

	// =========================================================================
	// Forward wave propagation runtime
	// =========================================================================

	UPROPERTY(Transient)
	TArray<FML_WaveChange> PendingChanges;

	UPROPERTY(Transient)
	AML_Tile* CurrentOriginTile = nullptr;

	UPROPERTY(Transient)
	TArray<AML_Tile*> ParasitesThatAteGrass;

	bool bIsResolvingTiles = false;
	bool bCycleHasChanges = false;
	int32 CurrentWaveIndex = 0;

	FTimerHandle IntraWaveTimerHandle;
	FTimerHandle InterWaveTimerHandle;

	void RunWave();
	void ScheduleNextPriority();
	void ProcessNextWave();
	void EndTileResolved();
	void CancelAllWaveTimers();

	// =========================================================================
	// Undo history / turn recording
	// =========================================================================

	// Full action history used by Undo / Reset.
	UPROPERTY(Transient)
	TArray<FML_ActionUndoRecord> ActionUndoStack;

	// Record currently being built while a turn/action is in progress.
	UPROPERTY(Transient)
	FML_TurnUndoRecord CurrentTurnRecord;

	bool bHasActiveTurnRecord = false;

	// Deterministic ordering for recorded deltas during forward gameplay.
	int32 CurrentPriorityIndexForRecording = 0;
	int32 UndoSequenceCounter = 0;

	void BeginTurnRecord_Internal(AML_Tile* OriginTile);
	void CommitTurnRecord_Internal();
	void DiscardTurnRecord_Internal();

	void RecordTileBeforeChange(AML_Tile* Tile, int32 DistanceFromOrigin);
	void RecordSpawnedActor(AActor* Spawned, int32 DistanceFromOrigin);

	// =========================================================================
	// Undo animation runtime
	// =========================================================================

	bool bUndoInProgress = false;
	bool bIsUndoAnimating = false;

	// Undo record currently being played back visually.
	UPROPERTY(Transient)
	FML_TurnUndoRecord ActiveUndoRecord;

	// Remaining deltas to apply during animated undo.
	UPROPERTY(Transient)
	TArray<FML_TileUndoDelta> PendingUndoTileDeltas;

	UPROPERTY(Transient)
	TArray<FML_SpawnUndoDelta> PendingUndoSpawnDeltas;

	void RunUndoWave();
	void ScheduleNextUndoWave(float Delay);
	void ApplyUndoWaveGroup(int32 PriorityIndex, int32 DistanceFromOrigin);
	//void FinishUndoAnimation();

	// Removes the collectible actor currently associated with a tile (if any).
	void DestroyCollectibleActorOnTile(AML_Tile* Tile);

	// =========================================================================
	// Reset runtime
	// =========================================================================

	UPROPERTY(Transient)
	bool bIsResetAllAnimating = false;

	void StartNextResetUndoStep();
	void ContinueResetAllIfNeeded();

	// =========================================================================
	// Time dilation helpers
	// =========================================================================

	UPROPERTY(Transient)
	bool bUndoTimeDilationApplied = false;

	void ApplyUndoTimeDilation();
	void ApplyResetTimeDilation();
	void ClearTimeDilation();
};