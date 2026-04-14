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
	UFUNCTION(BlueprintCallable, Category="Myceland Save")
	void MarkPuzzleSolved(FName PuzzleID, const TArray<FML_TileSaveEntry>& SolvedEntries);

	// Clears the solved flag so the player can replay the puzzle. Saves to disk.
	UFUNCTION(BlueprintCallable, Category="Myceland Save")
	void ResetPuzzle(FName PuzzleID);

	// Returns true if the puzzle has been solved at least once.
	UFUNCTION(BlueprintPure, Category="Myceland Save")
	bool IsPuzzleSolved(FName PuzzleID) const;

private:
	UPROPERTY()
	UML_GameSave* SaveObject = nullptr;

	static const FString SlotName;
	static const int32   UserIndex;
};
