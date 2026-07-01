// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Save System/ML_GameSaveData.h"
#include "ML_SaveSubsystem.generated.h"

class UML_GameSave;

UCLASS()
class MYCELAND_API UML_SaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// Automatically called when the GameInstance starts; loads or creates the save file.
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// Flush the in-memory save object to disk.
	UFUNCTION(BlueprintCallable, Category="Myceland Save")
	void SaveToDisk();

	// ==================== Settings ====================

	UFUNCTION(BlueprintPure, Category="Myceland Save")
	FML_GameSaveData GetSettings() const;

	UFUNCTION(BlueprintCallable, Category="Myceland Save")
	void SetSettings(const FML_GameSaveData& NewSettings);

	// ==================== Puzzle State ====================

	// Returns the saved record for a puzzle (default-constructed if never seen before).
	UFUNCTION(BlueprintPure, Category="Myceland Save")
	FML_PuzzleSaveRecord GetPuzzleRecord(FName PuzzleID) const;

	// If no record exists yet for this puzzle, stores InitialEntries as the authored state.
	// No-op (and no save) when a record already exists. Called from BoardSpawner::BeginPlay.
	void EnsureInitialGridSaved(FName PuzzleID, const TArray<FML_TileSaveEntry>& InitialEntries);

	// Marks the puzzle as solved, stores the SolvedGrid snapshot, and saves to disk.
	// LevelName scopes the solve into the correct per-level order log.
	UFUNCTION(BlueprintCallable, Category="Myceland Save")
	void MarkPuzzleSolved(FName PuzzleID, const TArray<FML_TileSaveEntry>& SolvedEntries, FName LevelName);

	// Stores only the SolvedGrid snapshot for a puzzle and saves, WITHOUT touching the
	// solved flag, solve order, or LastSolvedPuzzleID. Used by the hub to persist its
	// evolving grid on each definitive tile change while it isn't yet fully revitalized.
	UFUNCTION(BlueprintCallable, Category="Myceland Save")
	void SaveGridSnapshot(FName PuzzleID, const TArray<FML_TileSaveEntry>& Entries);

	// Clears the solved flag for PuzzleID and every puzzle solved after it within
	// the same level (cascade is level-scoped — other levels are never affected).
	// Returns the full list of reset IDs so callers can restore the matching boards.
	// LastSolvedPuzzleID is re-derived from the global order after removal.
	UFUNCTION(BlueprintCallable, Category="Myceland Save")
	TArray<FName> ResetPuzzle(FName PuzzleID, FName LevelName);

	// Returns true if the puzzle has been solved at least once.
	UFUNCTION(BlueprintPure, Category="Myceland Save")
	bool IsPuzzleSolved(FName PuzzleID) const;

	// Returns the PuzzleID of the most recently completed puzzle (None if no puzzle solved yet).
	UFUNCTION(BlueprintPure, Category="Myceland Save")
	FName GetLastSolvedPuzzleID() const;

	// ==================== Narrative Triggers ====================

	// Records that a narrative trigger has played (keyed by its level-placed name) and saves.
	UFUNCTION(BlueprintCallable, Category="Myceland Save")
	void SetNarrativeTriggerPlayed(FName TriggerID);

	// Clears a narrative trigger's played flag (e.g. on ResetTrigger) and saves.
	UFUNCTION(BlueprintCallable, Category="Myceland Save")
	void ClearNarrativeTriggerPlayed(FName TriggerID);

	// Returns true if the narrative trigger has already played.
	UFUNCTION(BlueprintPure, Category="Myceland Save")
	bool IsNarrativeTriggerPlayed(FName TriggerID) const;

private:
	UPROPERTY()
	UML_GameSave* SaveObject = nullptr;

	static const FString SlotName;
	static const int32   UserIndex;
};
