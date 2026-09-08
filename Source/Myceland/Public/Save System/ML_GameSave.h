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
	// ==================== Slot metadata ====================
	// Filled by the save subsystem, never by gameplay. Read back without loading the whole
	// session so the save-slot list can show a meaningful row per slot.

	// Label shown in the save-slot list. Refreshed from the current level on every level load,
	// or replaced by the label passed to ExportActiveSlotAsDemo for a packaged demo save.
	UPROPERTY(SaveGame)
	FString DisplayName;

	// Wall-clock time of the last write. Sorts the slot list newest-first.
	UPROPERTY(SaveGame)
	FDateTime LastSaveTime;

	// Level the player was in when this slot was last written; "Continue" reopens it through
	// UML_UIManagerSubsystem::OpenLevelByTag. Stored as the tag's FName rather than an
	// FGameplayTag so it round-trips through the SaveGame archive as plain data.
	UPROPERTY(SaveGame)
	FName CurrentLevelTagName;

	// General game settings (brightness, volume, resolution, …)
	UPROPERTY(SaveGame)
	FML_GameSaveData Settings;

	// Player's overall progression (mirrored on UML_GameInstance). Written whenever the
	// GameInstance's ProgressionState changes; read back into it on load.
	UPROPERTY(SaveGame)
	EML_ProgressionState ProgressionState = EML_ProgressionState::W1L0;

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

	// Narrative triggers that have already played, keyed by their level-placed actor
	// name. Restored into AML_NarrativeTrigger::bHasBeenPlayed on BeginPlay so a
	// play-once cinematic never replays after loading a save.
	UPROPERTY(SaveGame)
	TSet<FName> PlayedNarrativeTriggers;
};
