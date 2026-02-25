// Copyright Myceland Team, All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Core/ML_UndoTypes.h"
#include "Collectible/ML_Collectible.h"
#include "ML_WavePropagationSubsystem.generated.h"

struct FML_WaveChange;
class UML_MycelandDeveloperSettings;
class UML_WinLoseSubsystem;
class AML_PlayerController;
class AML_Tile;

UCLASS()
class MYCELAND_API UML_WavePropagationSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

private:
	UPROPERTY() UML_WinLoseSubsystem* WinLoseSubsystem = nullptr;
	UPROPERTY() AML_PlayerController* PlayerController = nullptr;
	UPROPERTY() const UML_MycelandDeveloperSettings* DevSettings = nullptr;

	UPROPERTY(Transient) TArray<FML_WaveChange> PendingChanges;
	UPROPERTY(Transient) AML_Tile* CurrentOriginTile = nullptr;
	UPROPERTY(Transient) TArray<AML_Tile*> ParasitesThatAteGrass;

	bool bIsResolvingTiles = false;
	int32 CurrentWaveIndex = 0;
	bool bCycleHasChanges = false;

	FTimerHandle IntraWaveTimerHandle;
	FTimerHandle InterWaveTimerHandle;

	// ---- Actions Undo stack ----
	UPROPERTY(Transient) TArray<FML_ActionUndoRecord> ActionUndoStack;

	UPROPERTY(Transient) FML_TurnUndoRecord CurrentTurnRecord;
	bool bHasActiveTurnRecord = false;

	bool bUndoInProgress = false;

	// ordering
	int32 CurrentPriorityIndexForRecording = 0;
	int32 UndoSequenceCounter = 0;

	// ---- Animated Undo runtime state (Plant/Waves only) ----
	UPROPERTY(Transient) FML_TurnUndoRecord ActiveUndoRecord;
	UPROPERTY(Transient) TArray<FML_TileUndoDelta> PendingUndoTileDeltas;
	UPROPERTY(Transient) TArray<FML_SpawnUndoDelta> PendingUndoSpawnDeltas;

	bool bIsUndoAnimating = false;

	// ---- Forward waves ----
	void RunWave();
	void ScheduleNextPriority();
	void ProcessNextWave();
	void EndTileResolved();
	void CancelAllWaveTimers();

	// ---- Turn record ----
	void BeginTurnRecord_Internal(AML_Tile* OriginTile);
	void CommitTurnRecord_Internal();
	void DiscardTurnRecord_Internal();

	void RecordTileBeforeChange(AML_Tile* Tile, int32 DistanceFromOrigin);
	void RecordSpawnedActor(AActor* Spawned, int32 DistanceFromOrigin);

	// ---- Animated undo (Plant/Waves) ----
	void RunUndoWave();
	void ScheduleNextUndoWave(float Delay);
	void FinishUndoAnimation();
	void ApplyUndoWaveGroup(int32 PriorityIndex, int32 DistanceFromOrigin);

public:
	void EnsureInitialized();

	// Called by collectible wave BEFORE flipping the HasCollectible flag
	void RecordTileForUndo(AML_Tile* Tile, int32 DistanceFromOrigin);

	UFUNCTION(BlueprintCallable, Category="Myceland Wave Propagation")
	void BeginTileResolved(AML_Tile* HitTile);

	UFUNCTION(BlueprintPure, Category="Myceland|Undo")
	bool CanUndo() const { return !bIsResolvingTiles && !bIsUndoAnimating && ActionUndoStack.Num() > 0; }

	// UI button
	UFUNCTION(BlueprintCallable, Category="Myceland|Undo")
	bool UndoLastAction_Animated();

	// Called by PlayerController when an undo-move playback ends
	UFUNCTION()
	void NotifyUndoMoveFinished();

	// Called by PlayerController when a NORMAL move ends (success)
	void NotifyMoveCompleted(
		const FIntPoint& StartAxial,
		const FIntPoint& EndAxial,
		const TArray<FIntPoint>& AxialPath,
		const FVector& StartWorld,
		const FVector& EndWorld,
		const TArray<FIntPoint>& PickedCollectibleAxials
	);
};