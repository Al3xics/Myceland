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

void UML_SaveSubsystem::MarkPuzzleSolved(FName PuzzleID, const TArray<FML_TileSaveEntry>& SolvedEntries)
{
	if (!SaveObject || PuzzleID.IsNone()) return;

	FML_PuzzleSaveRecord& Record = SaveObject->PuzzleRecords.FindOrAdd(PuzzleID);
	Record.bIsSolved  = true;
	Record.SolvedGrid = SolvedEntries;
	SaveToDisk();

	UE_LOG(LogTemp, Log, TEXT("[Save] Puzzle '%s' solved — saved to disk."), *PuzzleID.ToString());
}

void UML_SaveSubsystem::ResetPuzzle(FName PuzzleID)
{
	if (!SaveObject || PuzzleID.IsNone()) return;

	if (FML_PuzzleSaveRecord* Record = SaveObject->PuzzleRecords.Find(PuzzleID))
	{
		Record->bIsSolved = false;
		Record->SolvedGrid.Empty();
		SaveToDisk();
	}
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
