// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Core/ML_UndoTypes.h"
#include "ML_RollBackSubsystem.generated.h"

class UML_MycelandDeveloperSettings;
class UML_BiomeTileSet;
class AML_PlayerController;
class AML_PlayerCharacter;
class AML_BoardSpawner;
class AML_Tile;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRollbackUndoAnimating, bool, IsUndoAnimating);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRollbackResetAnimating, bool, IsResetAnimating);

UCLASS()
class MYCELAND_API UML_RollBackSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void EnsureInitialized();

	void BeginTurnRecord(AML_Tile* OriginTile);
	void CommitTurnRecord();
	void DiscardTurnRecord();

	void RecordTileForUndo(AML_Tile* Tile, int32 DistanceFromOrigin, int32 PriorityIndex);
	void RecordSpawnedActor(AActor* Spawned, int32 DistanceFromOrigin, int32 PriorityIndex);

	UFUNCTION(BlueprintPure, Category="Undo")
	bool CanUndo() const;

	UFUNCTION(BlueprintCallable, Category="Undo")
	bool UndoLastAction_Animated();

	UFUNCTION()
	void FinishUndoAnimation();

	UFUNCTION()
	bool RestoreCollectibleDuringUndoMove(const FIntPoint& Axial);

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
	void ResetAllActions_ExcludingMoves_Instant(AML_BoardSpawner* Board);

	bool IsUndoInProgress() const { return bUndoInProgress; }
	bool IsUndoAnimating() const { return bIsUndoAnimating; }
	bool IsResetAnimating() const { return bIsResetAllAnimating; }

	UPROPERTY(BlueprintAssignable, Category="Undo")
	FOnRollbackUndoAnimating OnUndoAnimating;

	UPROPERTY(BlueprintAssignable, Category="Reset")
	FOnRollbackResetAnimating OnResetAnimating;

private:
	UPROPERTY()
	AML_PlayerController* PlayerController = nullptr;
	
	UPROPERTY()
	AML_PlayerCharacter* PlayerCharacter = nullptr;

	UPROPERTY()
	const UML_MycelandDeveloperSettings* DevSettings = nullptr;

	TMap<TWeakObjectPtr<AML_BoardSpawner>, TArray<FML_ActionUndoRecord>> BoardUndoStacks;

	TArray<FML_ActionUndoRecord>* GetCurrentBoardStack();

	UPROPERTY(Transient)
	FML_TurnUndoRecord CurrentTurnRecord;

	bool bHasActiveTurnRecord = false;
	int32 UndoSequenceCounter = 0;
	bool bBoardChangedDelegateBound = false;
	bool bUndoInProgress = false;
	bool bIsUndoAnimating = false;

	UPROPERTY(Transient)
	FML_TurnUndoRecord ActiveUndoRecord;

	UPROPERTY(Transient)
	TArray<FML_TileUndoDelta> PendingUndoTileDeltas;

	UPROPERTY(Transient)
	TArray<FML_SpawnUndoDelta> PendingUndoSpawnDeltas;

	FTimerHandle UndoWaveTimerHandle;

	UPROPERTY(Transient)
	bool bIsResetAllAnimating = false;

	UPROPERTY(Transient)
	bool bIsUndoUntilPlantAnimating = false;

	UPROPERTY(Transient)
	bool bUndoUntilPlantReachedPlant = false;

	UPROPERTY(Transient)
	bool bUndoTimeDilationApplied = false;

	// Time dilation currently applied for the active undo/reset operation.
	UPROPERTY(Transient)
	float AppliedTimeDilation = 1.0f;

	// Minimum real (non-dilated) seconds enforced between two undo/reset waves.
	UPROPERTY(Transient)
	float MinRealSecondsPerWave = 0.033f;

	// Cached once per turn (set alongside ActiveUndoRecord) to avoid recomputing
	// per wave group inside ApplyUndoWaveGroup.
	TSet<FIntPoint> CachedSpawnedCollectibleAxials;
	const UML_BiomeTileSet* CachedActiveUndoTileSet = nullptr;

	// Max real time spent per tick draining a wave group. Time-based rather than
	// item-count-based because per-item cost varies a lot (no-op tile flip vs.
	// actor destroy+spawn), so a fixed item count doesn't bound frame cost reliably.
	double MaxSecondsPerWaveTick = 0.008;

	UFUNCTION()
	void OnBoardChanged(const AML_Tile* OldTile, const AML_Tile* NewTile);
	
	void RunUndoWave();
	void ScheduleNextUndoWave(float Delay);
	bool ApplyUndoWaveGroup(int32 PriorityIndex, int32 DistanceFromOrigin, double BudgetSeconds);
	bool UndoSingleAction_Animated();
	bool UndoUntilPlant_Animated();
	void StartNextUndoUntilPlantStep();
	void ContinueUndoUntilPlantIfNeeded();
	void StartNextResetUndoStep();
	void ContinueResetAllIfNeeded();
	TSet<FIntPoint> GetSpawnedCollectibleAxials(const TArray<FML_SpawnUndoDelta>& SpawnDeltas) const;
	bool RestoreCollectibleActorOnTile(AML_Tile* Tile);
	void DestroyCollectibleActorOnTile(AML_Tile* Tile);
	void ApplyUndoTimeDilation(const TArray<FML_ActionUndoRecord>& Stack);
	void ApplyResetTimeDilation(const TArray<FML_ActionUndoRecord>& Stack);
	float EstimateUndoGameDuration(const TArray<FML_ActionUndoRecord>& Stack) const;
	float EstimateResetGameDuration(const TArray<FML_ActionUndoRecord>& Stack) const;
	float EstimateActionResetGameDuration(const FML_ActionUndoRecord& Action) const;
	float EstimateMoveResetGameDuration(const FML_MoveUndoRecord& Move) const;
	float EstimatePlantResetGameDuration(const FML_TurnUndoRecord& Turn) const;
	void ClearTimeDilation();
	void ResetRuntimeState();
};