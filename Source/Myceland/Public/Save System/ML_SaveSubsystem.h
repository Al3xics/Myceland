// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Save System/ML_GameSaveData.h"
#include "ML_SaveSubsystem.generated.h"

class UML_GameSave;
class IConsoleObject;

/**
 * Owns the player's save data and decides which slot it is written to.
 *
 * Slots
 *   A slot is one continuous playthrough, not a point in time: every solve, grid snapshot and
 *   narrative trigger writes straight through to the active slot's file. Normal slots live in
 *   Saved/SaveGames and are created by CreateNewGameSlot / deleted by DeleteSlot.
 *
 * Demo saves
 *   The .sav files under Content/DemoSaves are packaged with the build (see
 *   DirectoriesToAlwaysStageAsNonUFS in DefaultGame.ini) and are strictly read-only: they are
 *   loaded from their own directory into memory and never become the active slot. Continuing
 *   one duplicates it into a fresh normal slot first, so the demo file itself can never be
 *   overwritten no matter how long the session runs.
 */
UCLASS()
class MYCELAND_API UML_SaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Flush the in-memory save object to the active slot. No-op when no slot is active, and
	// hard-refuses to write while a demo save is loaded (see the class comment).
	UFUNCTION(BlueprintCallable, Category="Myceland Save")
	void SaveToDisk();

	// ==================== Slots ====================

	// Every slot the player can pick from: the packaged demo saves first (alphabetical), then
	// the normal slots newest-first. Reads each file, so call it when the list opens, not per tick.
	UFUNCTION(BlueprintCallable, Category="Myceland Save|Slots")
	TArray<FML_SaveSlotInfo> GetAllSaveSlots() const;

	// Creates a blank slot, makes it active and writes it. Returns its SlotName, or an empty
	// string when the slot limit is already reached (branch on that in the menu).
	UFUNCTION(BlueprintCallable, Category="Myceland Save|Slots")
	FString CreateNewGameSlot();

	// Loads InSlotName and makes it the active slot, so gameplay resumes from it.
	// A demo slot is duplicated into a new normal slot first and the copy becomes active.
	// OutLevelTag is the level to open; feed it to UML_UIManagerSubsystem::OpenLevelByTag.
	UFUNCTION(BlueprintCallable, Category="Myceland Save|Slots")
	bool ContinueFromSlot(const FString& InSlotName, FGameplayTag& OutLevelTag);

	// Deletes a normal slot's file. Returns false for a demo slot, which is never deletable.
	UFUNCTION(BlueprintCallable, Category="Myceland Save|Slots")
	bool DeleteSlot(const FString& InSlotName);

	UFUNCTION(BlueprintPure, Category="Myceland Save|Slots")
	FString GetActiveSlotName() const { return ActiveSlotName; }

	// Number of normal slots the player is allowed to keep at once (demo saves don't count).
	UFUNCTION(BlueprintPure, Category="Myceland Save|Slots")
	int32 GetMaxSaveSlots() const { return MaxSaveSlots; }

	// Activates the fallback slot if — and only if — no slot has been chosen yet. Called when a
	// gameplay level loads, so entering a level straight from the editor (skipping the menu)
	// still saves and restores instead of silently discarding everything.
	void EnsureActiveSlot();

	// ==================== Level ====================

	// Level this save resumes into (None until a level has been loaded at least once).
	UFUNCTION(BlueprintPure, Category="Myceland Save|Level")
	FGameplayTag GetCurrentLevel() const;

	// Records the level the player is in, refreshes the slot's display label and saves.
	// Called from UML_GameInstance on every gameplay level load.
	UFUNCTION(BlueprintCallable, Category="Myceland Save|Level")
	void SetCurrentLevel(FGameplayTag LevelTag, const FString& InDisplayName);

	// ==================== Demo authoring (dev only) ====================

	// Writes the active slot to Content/DemoSaves/<DemoName>.sav so it ships with the build.
	// Label overrides the row shown in the slot list; pass empty to keep the level name.
	// Also available in the console as: ml.ExportDemoSave <Name> [Label]
	UFUNCTION(BlueprintCallable, Category="Myceland Save|Demo")
	bool ExportActiveSlotAsDemo(const FString& DemoName, const FString& Label);

	// ==================== Settings ====================

	UFUNCTION(BlueprintPure, Category="Myceland Save")
	FML_GameSaveData GetSettings() const;

	UFUNCTION(BlueprintCallable, Category="Myceland Save")
	void SetSettings(const FML_GameSaveData& NewSettings);

	// ==================== Puzzle State ====================

	// Returns the saved record for a puzzle (default-constructed if never seen before).
	// Copies the record (incl. its grids); Blueprint-facing. C++ hot paths that only read the
	// record should prefer FindPuzzleRecord() to avoid the copy.
	UFUNCTION(BlueprintPure, Category="Myceland Save")
	FML_PuzzleSaveRecord GetPuzzleRecord(FName PuzzleID) const;

	// C++-only fast path: pointer to the stored record, or nullptr if this puzzle has none.
	// Avoids copying the record's grids on the load/reset hot paths. The returned pointer is
	// only valid until the next mutation of the save object.
	const FML_PuzzleSaveRecord* FindPuzzleRecord(FName PuzzleID) const;

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

	// ==================== Progression ====================

	// Returns the persisted progression state (W1L0 when there is no save object).
	UFUNCTION(BlueprintPure, Category="Myceland Save")
	EML_ProgressionState GetProgressionState() const;

	// Stores the progression state and saves to disk. Called by UML_GameInstance whenever
	// its ProgressionState changes.
	UFUNCTION(BlueprintCallable, Category="Myceland Save")
	void SetProgressionState(EML_ProgressionState NewState);

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

	// Clears every narrative trigger's played flag so all play-once cinematics can fire again.
	// Done automatically at startup in the editor (see Initialize); never in a cooked build.
	UFUNCTION(BlueprintCallable, Category="Myceland Save")
	void ClearAllNarrativeTriggersPlayed(bool bWriteToDisk = true);

private:
	// Directory holding the packaged read-only demo saves, and the prefix that marks their
	// SlotName so a demo can never be mistaken for a writable slot.
	static FString GetDemoSaveDir();
	static const FString DemoSlotPrefix;

	// Auto-generated names for normal slots ("Slot_1", "Slot_2", …).
	static const FString SlotNamePrefix;

	// Slot the very first New Game predates: the single hardcoded file the game used before
	// slots existed. Also the fallback when a gameplay level is entered without picking a slot.
	static const FString FallbackSlotName;

	static const int32 UserIndex;
	static const int32 MaxSaveSlots;

	static bool IsDemoSlotID(const FString& InSlotName);

	// Loads a packaged demo save by its bare name (no "demo:" prefix) straight from
	// Content/DemoSaves. Deliberately not routed through UGameplayStatics slot helpers, which
	// would look in Saved/SaveGames and could be written back to.
	static UML_GameSave* LoadDemoSave(const FString& DemoName);

	// Reads one save file's metadata into a UI row. Returns false when it can't be read.
	static bool MakeSlotInfo(const UML_GameSave* Save, const FString& InSlotName, bool bReadOnly, FML_SaveSlotInfo& OutInfo);

	// First unused "Slot_N", or an empty string once MaxSaveSlots normal slots exist.
	FString GenerateNewSlotName() const;

	// Points the subsystem at a slot once SaveObject already holds the matching data (loaded,
	// freshly created, or duplicated from a demo). Also applies the editor-only narrative
	// trigger reset, which has to happen per activated slot rather than once at startup.
	void SetActiveSlot(const FString& InSlotName);

	UPROPERTY()
	UML_GameSave* SaveObject = nullptr;

	// Empty until New Game / Continue picks one, or a gameplay level falls back (EnsureActiveSlot).
	FString ActiveSlotName;

#if !UE_BUILD_SHIPPING
	IConsoleObject* ExportDemoSaveCommand = nullptr;
#endif
};
