// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ML_WavePropagationSubsystem.generated.h"

class UML_CinematicSubsystem;
struct FML_WaveChange;
enum class EML_BoardActionReason : uint8;

class UML_MycelandDeveloperSettings;
class UML_WinLoseSubsystem;
class UML_RollBackSubsystem;
class AML_PlayerController;
class AML_Tile;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUndoAnimating, bool, IsUndoAnimating);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnResetAnimating, bool, IsResetAnimating);

// Broadcast once when a full tile-resolution action (plant + all wave propagation) has finished and
// the board has reached its final state. Consumers that display board-dependent UI (e.g. the gamepad
// plantable highlight) refresh here instead of reacting to every individual tile change mid-propagation.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWavePropagationFinished);

UCLASS()
class MYCELAND_API UML_WavePropagationSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// Ticks only while a ring is being time-sliced across frames (see IsTickable).
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;

	void EnsureInitialized();

	UFUNCTION(BlueprintCallable, Category="Myceland Wave Propagation")
	void BeginTileResolved(AML_Tile* HitTile);
	void BeginTileResolvedWithoutAvatarSurprise(AML_Tile* HitTile);

	void RecordTileForUndo(AML_Tile* Tile, int32 DistanceFromOrigin);

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

	void ResetAllActions_ExcludingMoves_Instant(class AML_BoardSpawner* Board);

	void CancelAllWaveTimers();
	bool IsResolvingTiles() const { return bIsResolvingTiles; }
	int32 GetCurrentPriorityIndexForRecording() const { return CurrentPriorityIndexForRecording; }
	void AbortPropagationRuntime();

	UPROPERTY(BlueprintAssignable, Category="Undo")
	FOnUndoAnimating OnUndoAnimating;

	UPROPERTY(BlueprintAssignable, Category="Reset")
	FOnResetAnimating OnResetAnimating;

	UPROPERTY(BlueprintAssignable, Category="Myceland Wave Propagation")
	FOnWavePropagationFinished OnWavePropagationFinished;

private:
	// =========================================================================
	// Core references / initialization
	// =========================================================================

	UPROPERTY()
	UML_WinLoseSubsystem* WinLoseSubsystem = nullptr;

	UPROPERTY()
	const UML_CinematicSubsystem* CinematicSubsystem = nullptr;

	UPROPERTY()
	AML_PlayerController* PlayerController = nullptr;

	UPROPERTY()
	const UML_MycelandDeveloperSettings* DevSettings = nullptr;

	UPROPERTY()
	UML_RollBackSubsystem* RollBackSubsystem = nullptr;

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

	// True if anything changed on the board since BeginTileResolved. Unlike
	// bCycleHasChanges (reset every cycle), it covers the whole action: used to
	// skip the goal-path recompute in EndTileResolved when nothing changed.
	bool bAnyChangeThisAction = false;
	int32 CurrentWaveIndex = 0;
	int32 CurrentNatureReactionCount = 0;
	int32 CurrentParasiteReactionCount = 0;
	int32 CurrentWaterReactionCount = 0;
	int32 TotalReactionTileCount = 0;
	bool bPlayAvatarSurpriseVocalThisAction = true;

	// Read cursor into PendingChanges (consumed in place, no removal).
	int32 PendingChangesIndex = 0;

	// Time-slicing state: a ring (all changes at the same DistanceFromOrigin) is applied
	// under a per-frame CPU budget (DevSettings->WavePropagationFrameBudgetMs). If the ring
	// doesn't fit in one frame, the remainder continues in Tick on the following frames.
	bool bRingInProgress = false;
	int32 CurrentRingDistance = 0;

	FTimerHandle IntraWaveTimerHandle;
	FTimerHandle InterWaveTimerHandle;

	void RunWave();
	void ApplyChange(const FML_WaveChange& Change);
	void ProcessRingSlice(double Deadline);
	void FinishRing();
	double MakeSliceDeadline() const;
	void ScheduleNextPriority();
	void ProcessNextWave();
	void EndTileResolved();
	void BeginTileResolvedInternal(AML_Tile* HitTile, bool bPlayAvatarSurpriseVocal);

	// =========================================================================
	// Touch wave — pure visual feedback, no gameplay effect.
	//
	// PendingChanges only contains the tiles whose TYPE changes, so if only 3
	// tiles react, only 3 tiles would animate. The touch wave is what makes the
	// whole board feel the wave passing: BuildTouchQueue BFS-walks the entire
	// board from the clicked tile (sorted by distance), then FireNextTouchRing
	// calls OnWaveTouched() — a BP feedback event on AML_Tile (e.g. the tile
	// "hop") — on every tile, ring by ring, even those that don't change.
	// =========================================================================

	UPROPERTY(Transient)
	TArray<FML_WaveChange> PendingTouched;

	int32 TouchIndex = 0;
	FTimerHandle TouchTimerHandle;

	// Same time-slicing as the forward wave, for the touch rings.
	bool bTouchRingInProgress = false;
	int32 CurrentTouchDistance = 0;

	void BuildTouchQueue(AML_Tile* OriginTile);
	void FireNextTouchRing();
	void ProcessTouchSlice(double Deadline);

	// =========================================================================
	// Deterministic ordering for recorded deltas during forward gameplay.
	int32 CurrentPriorityIndexForRecording = 0;

	UFUNCTION()
	void HandleRollbackUndoAnimating(bool bIsAnimating);

	UFUNCTION()
	void HandleRollbackResetAnimating(bool bIsAnimating);

	// Holds (or releases) the board lock for the duration of a rollback animation.
	void HoldBoardForRollback(bool bIsAnimating, EML_BoardActionReason Reason);

	bool bRollbackDelegatesBound = false;
};
