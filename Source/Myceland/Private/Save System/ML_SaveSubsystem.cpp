// Copyright Myceland Team, All Rights Reserved.

#include "Save System/ML_SaveSubsystem.h"
#include "Save System/ML_GameSave.h"
#include "Kismet/GameplayStatics.h"

const FString UML_SaveSubsystem::SlotName  = TEXT("MycelandSave");
const int32   UML_SaveSubsystem::UserIndex = 0;

void UML_SaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex))
	{
		SaveObject = Cast<UML_GameSave>(UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex));
	}

	if (!SaveObject)
	{
		SaveObject = Cast<UML_GameSave>(UGameplayStatics::CreateSaveGameObject(UML_GameSave::StaticClass()));
	}
}

void UML_SaveSubsystem::SaveToDisk()
{
	if (!SaveObject) return;
	UGameplayStatics::SaveGameToSlot(SaveObject, SlotName, UserIndex);
}

// ==================== Settings ====================

FML_GameSaveData UML_SaveSubsystem::GetSettings() const
{
	return SaveObject ? SaveObject->Settings : FML_GameSaveData{};
}

void UML_SaveSubsystem::SetSettings(const FML_GameSaveData& NewSettings)
{
	if (!SaveObject) return;
	SaveObject->Settings = NewSettings;
	SaveToDisk();
}

// ==================== Puzzle State ====================

FML_PuzzleSaveRecord UML_SaveSubsystem::GetPuzzleRecord(FName PuzzleID) const
{
	if (!SaveObject || PuzzleID.IsNone()) return FML_PuzzleSaveRecord{};
	if (const FML_PuzzleSaveRecord* Record = SaveObject->PuzzleRecords.Find(PuzzleID))
	{
		return *Record;
	}
	return FML_PuzzleSaveRecord{};
}

void UML_SaveSubsystem::EnsureInitialGridSaved(FName PuzzleID, const TArray<FML_TileSaveEntry>& InitialEntries)
{
	if (!SaveObject || PuzzleID.IsNone()) return;

	// Only store the initial grid the very first time we see this puzzle.
	if (SaveObject->PuzzleRecords.Contains(PuzzleID)) return;

	FML_PuzzleSaveRecord NewRecord;
	NewRecord.InitialGrid = InitialEntries;
	SaveObject->PuzzleRecords.Add(PuzzleID, NewRecord);
	SaveToDisk();
}

void UML_SaveSubsystem::MarkPuzzleSolved(FName PuzzleID, const TArray<FML_TileSaveEntry>& SolvedEntries, FName LevelName)
{
	if (!SaveObject || PuzzleID.IsNone()) return;

	FML_PuzzleSaveRecord& Record = SaveObject->PuzzleRecords.FindOrAdd(PuzzleID);
	Record.bIsSolved  = true;
	Record.SolvedGrid = SolvedEntries;

	// Per-level order (for cascade-scoping on reset).
	if (!LevelName.IsNone())
		SaveObject->SolvedOrderByLevel.FindOrAdd(LevelName).PuzzleIDs.AddUnique(PuzzleID);

	// Global order (for LastSolvedPuzzleID / spawn-position tracking across all levels).
	// AddUnique guards against a double-solve without a prior reset.
	SaveObject->GlobalSolvedOrder.AddUnique(PuzzleID);
	SaveObject->LastSolvedPuzzleID = PuzzleID;

	SaveToDisk();

	UE_LOG(LogTemp, Log, TEXT("[Save] Puzzle '%s' (level '%s') solved — saved to disk."),
		*PuzzleID.ToString(), *LevelName.ToString());
}

TArray<FName> UML_SaveSubsystem::ResetPuzzle(FName PuzzleID, FName LevelName)
{
	TArray<FName> ResetIDs;
	if (!SaveObject || PuzzleID.IsNone()) return ResetIDs;

	// ---- Level-scoped cascade ----
	// Only puzzles in the same level that were solved after PuzzleID are reset.
	// Puzzles in other levels are completely unaffected.
	if (!LevelName.IsNone())
	{
		if (FML_LevelSolveOrder* LevelOrder = SaveObject->SolvedOrderByLevel.Find(LevelName))
		{
			const int32 Idx = LevelOrder->PuzzleIDs.IndexOfByKey(PuzzleID);
			if (Idx != INDEX_NONE)
			{
				// Collect this puzzle + everything solved after it within this level.
				for (int32 i = Idx; i < LevelOrder->PuzzleIDs.Num(); ++i)
					ResetIDs.Add(LevelOrder->PuzzleIDs[i]);

				// Truncate the per-level order to before the reset point.
				LevelOrder->PuzzleIDs.SetNum(Idx);
			}
		}
	}

	// Fall back: if the puzzle wasn't in any level order, still reset it individually.
	if (ResetIDs.IsEmpty() && SaveObject->PuzzleRecords.Contains(PuzzleID))
		ResetIDs.Add(PuzzleID);

	if (ResetIDs.IsEmpty())
	{
		SaveToDisk();
		return ResetIDs;
	}

	// Clear solved state for every affected puzzle (InitialGrid is intentionally kept).
	for (const FName& ID : ResetIDs)
	{
		if (FML_PuzzleSaveRecord* Record = SaveObject->PuzzleRecords.Find(ID))
		{
			Record->bIsSolved = false;
			Record->SolvedGrid.Empty();
		}
	}

	// Remove the affected IDs from the global order. Other levels' entries are preserved,
	// so their relative position is unaffected (TArray::Remove keeps remaining order).
	for (const FName& ID : ResetIDs)
		SaveObject->GlobalSolvedOrder.Remove(ID);

	// Re-derive the spawn-position anchor from whatever remains globally.
	SaveObject->LastSolvedPuzzleID = SaveObject->GlobalSolvedOrder.IsEmpty()
		? NAME_None
		: SaveObject->GlobalSolvedOrder.Last();

	SaveToDisk();

	UE_LOG(LogTemp, Log, TEXT("[Save] Reset puzzle '%s' (level '%s') + %d cascaded. New last solved: '%s'."),
		*PuzzleID.ToString(), *LevelName.ToString(), ResetIDs.Num() - 1,
		*SaveObject->LastSolvedPuzzleID.ToString());

	return ResetIDs;
}

bool UML_SaveSubsystem::IsPuzzleSolved(FName PuzzleID) const
{
	if (!SaveObject || PuzzleID.IsNone()) return false;
	if (const FML_PuzzleSaveRecord* Record = SaveObject->PuzzleRecords.Find(PuzzleID))
	{
		return Record->bIsSolved;
	}
	return false;
}

FName UML_SaveSubsystem::GetLastSolvedPuzzleID() const
{
	return SaveObject ? SaveObject->LastSolvedPuzzleID : NAME_None;
}

// ==================== Narrative Triggers ====================

void UML_SaveSubsystem::SetNarrativeTriggerPlayed(FName TriggerID)
{
	if (!SaveObject || TriggerID.IsNone()) return;

	bool bAlreadyPresent = false;
	SaveObject->PlayedNarrativeTriggers.Add(TriggerID, &bAlreadyPresent);

	// Avoid a redundant disk write if it was already recorded.
	if (!bAlreadyPresent)
		SaveToDisk();
}

void UML_SaveSubsystem::ClearNarrativeTriggerPlayed(FName TriggerID)
{
	if (!SaveObject || TriggerID.IsNone()) return;

	if (SaveObject->PlayedNarrativeTriggers.Remove(TriggerID) > 0)
		SaveToDisk();
}

bool UML_SaveSubsystem::IsNarrativeTriggerPlayed(FName TriggerID) const
{
	if (!SaveObject || TriggerID.IsNone()) return false;
	return SaveObject->PlayedNarrativeTriggers.Contains(TriggerID);
}
