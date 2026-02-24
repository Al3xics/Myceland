// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Collectible/ML_Collectible.h"
#include "Tiles/ML_Tile.h"
#include "Core/ML_UndoTypes.h"
#include "ML_WavePropagationSubsystem.generated.h"

struct FML_WaveChange;
class UML_MycelandDeveloperSettings;
class AML_Tile;
class AML_PlayerController;
class UML_WinLoseSubsystem;


UCLASS()
class MYCELAND_API UML_WavePropagationSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

private:
	UPROPERTY()
	UML_WinLoseSubsystem* WinLoseSubsystem = nullptr;

	UPROPERTY()
	AML_PlayerController* PlayerController = nullptr;

	UPROPERTY()
	const UML_MycelandDeveloperSettings* DevSettings = nullptr;

	UPROPERTY(Transient)
	TArray<FML_WaveChange> PendingChanges;

	UPROPERTY(Transient)
	AML_Tile* CurrentOriginTile = nullptr;

	UPROPERTY(Transient)
	TArray<AML_Tile*> ParasitesThatAteGrass;

	bool bIsResolvingTiles = false;
	int32 CurrentWaveIndex = 0;
	bool bCycleHasChanges = false;

	// ---- Timers (so we can cancel safely) ----
	FTimerHandle IntraWaveTimerHandle;
	FTimerHandle InterWaveTimerHandle;

	// ---- Undo ----
	UPROPERTY(Transient)
	TArray<FML_TurnUndoRecord> UndoStack;

	UPROPERTY(Transient)
	FML_TurnUndoRecord CurrentTurnRecord;

	bool bHasActiveTurnRecord = false;

	// Needed to avoid re-triggering propagation during undo
	bool bUndoInProgress = false;

	// ---- Internal helpers ----
	void RunWave();
	void ScheduleNextPriority();
	void ProcessNextWave();
	void EndTileResolved();

	void CancelAllWaveTimers();

	void BeginTurnRecord_Internal(AML_Tile* OriginTile);
	void CommitTurnRecord_Internal();
	void DiscardTurnRecord_Internal();

	void RecordTileBeforeChange(AML_Tile* Tile, int32 DistanceFromOrigin);
	void RecordSpawnedActor(AActor* Spawned, int32 DistanceFromOrigin);

	void ApplyUndoRecord(const FML_TurnUndoRecord& Record);

	int32 CurrentPriorityIndexForRecording = 0;
	int32 UndoSequenceCounter = 0;

	// ---- Animated Undo runtime state ----
	UPROPERTY(Transient)
	FML_TurnUndoRecord ActiveUndoRecord;

	UPROPERTY(Transient)
	TArray<FML_TileUndoDelta> PendingUndoTileDeltas;

	UPROPERTY(Transient)
	TArray<FML_SpawnUndoDelta> PendingUndoSpawnDeltas;

	bool bIsUndoAnimating = false;

	void RunUndoWave();
	void ScheduleNextUndoWave(float Delay);
	void FinishUndoAnimation();
	void ApplyUndoWaveGroup(int32 PriorityIndex, int32 DistanceFromOrigin);

public:
	void EnsureInitialized();

	void RecordTileForUndo(AML_Tile* Tile, int32 DistanceFromOrigin);

	// Entry point for starting the Wave Propagation Cycle
	UFUNCTION(BlueprintCallable, Category="Myceland Wave Propagation")
	void BeginTileResolved(AML_Tile* HitTile);

	// Call this from your UI button later
	UFUNCTION(BlueprintCallable, Category="Myceland|Undo")
	bool UndoLastTurn();

	// Optional: expose for UI enabling/disabling
	UFUNCTION(BlueprintPure, Category="Myceland|Undo")
	bool CanUndo() const { return !bIsResolvingTiles && UndoStack.Num() > 0; }

	// NEW: Animated Undo
	UFUNCTION(BlueprintCallable, Category="Myceland|Undo")
	bool UndoLastTurn_Animated();
};