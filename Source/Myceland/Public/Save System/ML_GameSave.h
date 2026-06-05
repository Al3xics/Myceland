// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Save System/ML_GameSaveData.h"
#include "ML_GameSave.generated.h"

UCLASS()
class MYCELAND_API UML_GameSave : public USaveGame
{
	GENERATED_BODY()

public:
	// General game settings (brightness, volume, resolution, …)
	UPROPERTY(SaveGame)
	FML_GameSaveData Settings;

	// One record per puzzle, keyed by the PuzzleID set on AML_BoardSpawner.
	UPROPERTY(SaveGame)
	TMap<FName, FML_PuzzleSaveRecord> PuzzleRecords;

	// Per-level ordered solve logs (level name → puzzle IDs oldest→newest).
	// Cascade resets are scoped here so resetting a puzzle in Level A never
	// affects puzzles solved in Level B.
	UPROPERTY(SaveGame)
	TMap<FName, FML_LevelSolveOrder> SolvedOrderByLevel;

	// Global insertion-ordered list of every solved puzzle ID across all levels.
	// Used only to derive LastSolvedPuzzleID after a level-scoped cascade removes
	// some entries — other levels' entries are left in place.
	UPROPERTY(SaveGame)
	TArray<FName> GlobalSolvedOrder;

	// PuzzleID of the most recently solved puzzle across all levels. Used to restore
	// the player's spawn position to that puzzle's exit tile on session load.
	// Always mirrors GlobalSolvedOrder.Last() (or NAME_None when empty).
	UPROPERTY(SaveGame)
	FName LastSolvedPuzzleID;
};
