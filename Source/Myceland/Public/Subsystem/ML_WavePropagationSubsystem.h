// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ML_WavePropagationSubsystem.generated.h"

struct FML_WaveChange;

class UML_MycelandDeveloperSettings;
class UML_WinLoseSubsystem;
class UML_RollBackSubsystem;
class AML_PlayerController;
class AML_Tile;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUndoAnimating, bool, IsUndoAnimating);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnResetAnimating, bool, IsResetAnimating);

UCLASS()
class MYCELAND_API UML_WavePropagationSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void EnsureInitialized();

	UFUNCTION(BlueprintCallable, Category="Myceland Wave Propagation")
	void BeginTileResolved(AML_Tile* HitTile);

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

	bool ResetAllActions_ExcludingMoves_Animated();
	void ResetAllActions_ExcludingMoves_Instant(class AML_BoardSpawner* Board);

	void CancelAllWaveTimers();
	bool IsResolvingTiles() const { return bIsResolvingTiles; }
	int32 GetCurrentPriorityIndexForRecording() const { return CurrentPriorityIndexForRecording; }
	void AbortPropagationRuntime();

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
	int32 CurrentWaveIndex = 0;

	FTimerHandle IntraWaveTimerHandle;
	FTimerHandle InterWaveTimerHandle;

	void RunWave();
	void ScheduleNextPriority();
	void ProcessNextWave();
	void EndTileResolved();

	// =========================================================================
	// Deterministic ordering for recorded deltas during forward gameplay.
	int32 CurrentPriorityIndexForRecording = 0;

	UFUNCTION()
	void HandleRollbackUndoAnimating(bool bIsAnimating);

	UFUNCTION()
	void HandleRollbackResetAnimating(bool bIsAnimating);

	bool bRollbackDelegatesBound = false;
};
