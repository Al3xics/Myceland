// Copyright Myceland Team, All Rights Reserved.

#include "Save System/ML_SaveSubsystem.h"
#include "Save System/ML_GameSave.h"
#include "Core/ML_GameplayTags.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

const FString UML_SaveSubsystem::DemoSlotPrefix   = TEXT("demo:");
const FString UML_SaveSubsystem::SlotNamePrefix   = TEXT("Slot_");
const FString UML_SaveSubsystem::FallbackSlotName = TEXT("MycelandSave");
const int32   UML_SaveSubsystem::UserIndex        = 0;
const int32   UML_SaveSubsystem::MaxSaveSlots     = 10;

FString UML_SaveSubsystem::GetDemoSaveDir()
{
	// Staged raw next to the cooked content by DirectoriesToAlwaysStageAsNonUFS in
	// DefaultGame.ini, so the same path resolves in the editor and in a packaged build.
	return FPaths::ProjectContentDir() / TEXT("DemoSaves");
}

bool UML_SaveSubsystem::IsDemoSlotID(const FString& InSlotName)
{
	return InSlotName.StartsWith(DemoSlotPrefix);
}

void UML_SaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Deliberately no save is loaded here: the menu runs before any slot is picked, and
	// loading one at startup would silently make it the target of every later write.
	// New Game / Continue set the slot, and EnsureActiveSlot covers entering a gameplay
	// level directly from the editor.

#if !UE_BUILD_SHIPPING
	ExportDemoSaveCommand = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("ml.ExportDemoSave"),
		TEXT("Write the active save slot to Content/DemoSaves/<Name>.sav so it ships with the build. ")
		TEXT("Usage: ml.ExportDemoSave <Name> [Label shown in the slot list]"),
		FConsoleCommandWithArgsDelegate::CreateWeakLambda(this, [this](const TArray<FString>& Args)
		{
			if (Args.Num() == 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("[Save] Usage: ml.ExportDemoSave <Name> [Label]"));
				return;
			}

			// The console splits on spaces and keeps any quotes the user typed, so strip them
			// rather than letting a stray " end up in a file name.
			auto Unquote = [](const FString& In)
			{
				FString Out = In;
				Out.TrimQuotesInline();
				return Out.TrimStartAndEnd();
			};

			// Everything after the name is the label, so it can contain spaces unquoted.
			FString Label;
			for (int32 i = 1; i < Args.Num(); ++i)
				Label += (i > 1 ? TEXT(" ") : TEXT("")) + Unquote(Args[i]);

			ExportActiveSlotAsDemo(Unquote(Args[0]), Label);
		}),
		ECVF_Default);

	// Replaces the old ml.ResetNarrativeTriggersInEditor CVar, which cleared the flags on every
	// slot activation - Continue included, whose SaveToDisk then wrote the wipe straight back
	// into the slot file. Replaying a narration in dev is now asked for, not the default.
	ResetStoryBeatsCommand = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("ml.ResetStoryBeats"),
		TEXT("Forget every play-once story beat in the active slot (narrations, level intros) ")
		TEXT("so they all play again on the next level load."),
		FConsoleCommandDelegate::CreateWeakLambda(this, [this]()
		{
			if (!SaveObject)
			{
				UE_LOG(LogTemp, Warning, TEXT("[Save] No active slot - nothing to reset."));
				return;
			}
			ClearAllStoryBeats();
		}),
		ECVF_Default);
#endif
}

void UML_SaveSubsystem::Deinitialize()
{
#if !UE_BUILD_SHIPPING
	if (ExportDemoSaveCommand)
	{
		IConsoleManager::Get().UnregisterConsoleObject(ExportDemoSaveCommand);
		ExportDemoSaveCommand = nullptr;
	}

	if (ResetStoryBeatsCommand)
	{
		IConsoleManager::Get().UnregisterConsoleObject(ResetStoryBeatsCommand);
		ResetStoryBeatsCommand = nullptr;
	}
#endif

	Super::Deinitialize();
}

void UML_SaveSubsystem::SaveToDisk()
{
	if (!SaveObject || ActiveSlotName.IsEmpty()) return;

	// Belt and braces: ContinueFromSlot already duplicates a demo before activating it, so the
	// active slot should never be one. If that ever changes, fail loudly rather than overwrite
	// a demo file the whole presentation depends on.
	if (IsDemoSlotID(ActiveSlotName))
	{
		UE_LOG(LogTemp, Error, TEXT("[Save] Refused to write to read-only demo slot '%s'."), *ActiveSlotName);
		return;
	}

	SaveObject->LastSaveTime = FDateTime::Now();
	UGameplayStatics::SaveGameToSlot(SaveObject, ActiveSlotName, UserIndex);
}

// ==================== Slots ====================

void UML_SaveSubsystem::SetActiveSlot(const FString& InSlotName)
{
	ActiveSlotName = InSlotName;
}

bool UML_SaveSubsystem::MakeSlotInfo(const UML_GameSave* Save, const FString& InSlotName, bool bReadOnly, FML_SaveSlotInfo& OutInfo)
{
	if (!Save) return false;

	OutInfo.SlotName     = InSlotName;
	OutInfo.DisplayName  = Save->DisplayName.IsEmpty() ? InSlotName : Save->DisplayName;
	OutInfo.LastSaveTime = Save->LastSaveTime;
	OutInfo.CurrentLevel = FGameplayTag::RequestGameplayTag(Save->CurrentLevelTagName, /*ErrorIfNotFound=*/false);
	OutInfo.bIsReadOnly  = bReadOnly;
	return true;
}

UML_GameSave* UML_SaveSubsystem::LoadDemoSave(const FString& DemoName)
{
	// Read the bytes ourselves rather than going through the slot helpers: those resolve into
	// Saved/SaveGames, which is exactly the directory a demo must never end up in.
	TArray<uint8> Bytes;
	const FString Path = GetDemoSaveDir() / (DemoName + TEXT(".sav"));
	if (!FFileHelper::LoadFileToArray(Bytes, *Path))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Save] Could not read demo save '%s'."), *Path);
		return nullptr;
	}

	return Cast<UML_GameSave>(UGameplayStatics::LoadGameFromMemory(Bytes));
}

TArray<FML_SaveSlotInfo> UML_SaveSubsystem::GetAllSaveSlots() const
{
	IFileManager& FileManager = IFileManager::Get();

	// ---- Packaged demo saves (read-only) ----
	TArray<FML_SaveSlotInfo> DemoSlots;
	{
		TArray<FString> Files;
		FileManager.FindFiles(Files, *(GetDemoSaveDir() / TEXT("*.sav")), /*Files=*/true, /*Directories=*/false);

		for (const FString& File : Files)
		{
			const FString DemoName = FPaths::GetBaseFilename(File);

			FML_SaveSlotInfo Info;
			if (MakeSlotInfo(LoadDemoSave(DemoName), DemoSlotPrefix + DemoName, /*bReadOnly=*/true, Info))
				DemoSlots.Add(Info);
		}
	}

	// ---- Normal slots on disk ----
	TArray<FML_SaveSlotInfo> PlayerSlots;
	{
		TArray<FString> Files;
		FileManager.FindFiles(Files, *(FPaths::ProjectSavedDir() / TEXT("SaveGames") / TEXT("*.sav")), true, false);

		for (const FString& File : Files)
		{
			const FString Slot = FPaths::GetBaseFilename(File);

			FML_SaveSlotInfo Info;
			const UML_GameSave* Save = Cast<UML_GameSave>(UGameplayStatics::LoadGameFromSlot(Slot, UserIndex));
			if (MakeSlotInfo(Save, Slot, /*bReadOnly=*/false, Info))
				PlayerSlots.Add(Info);
		}
	}

	TArray<FML_SaveSlotInfo> Result = MoveTemp(DemoSlots);
	Result.Append(PlayerSlots);

	// One rule for the whole list, so the UI can render it in order without sorting again:
	// demos first (they are the fixed entry points, the player's own saves scroll below them),
	// then newest-first within each group - the row the player most likely wants is at the top.
	Result.Sort([](const FML_SaveSlotInfo& A, const FML_SaveSlotInfo& B)
	{
		if (A.bIsReadOnly != B.bIsReadOnly)
			return A.bIsReadOnly;

		return A.LastSaveTime > B.LastSaveTime;
	});

	return Result;
}

int32 UML_SaveSubsystem::CountPlayerSlots()
{
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files,
		*(FPaths::ProjectSavedDir() / TEXT("SaveGames") / TEXT("*.sav")), /*Files=*/true, /*Directories=*/false);
	return Files.Num();
}

FString UML_SaveSubsystem::GenerateNewSlotName() const
{
	// The limit counts every file the slot list shows, not just the auto-named Slot_N ones:
	// the legacy MycelandSave is a playthrough like any other and takes a place. Counting only
	// free Slot_N names would let the player past the limit by exactly that one slot.
	// Demo saves live outside this directory and never count.
	if (CountPlayerSlots() >= MaxSaveSlots) return FString();

	// Guaranteed to find one: fewer than MaxSaveSlots files exist, so at most MaxSaveSlots - 1
	// of these names can be taken.
	// Stays silent on failure: CanCreateNewGameSlot polls this every time the menu is shown,
	// so the "limit reached" warning belongs to the callers that actually wanted a new slot.
	for (int32 i = 1; i <= MaxSaveSlots; ++i)
	{
		const FString Candidate = SlotNamePrefix + FString::FromInt(i);
		if (!UGameplayStatics::DoesSaveGameExist(Candidate, UserIndex))
			return Candidate;
	}

	return FString();
}

bool UML_SaveSubsystem::CanCreateNewGameSlot() const
{
	return !GenerateNewSlotName().IsEmpty();
}

FString UML_SaveSubsystem::CreateNewGameSlot()
{
	const FString NewSlot = GenerateNewSlotName();
	if (NewSlot.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Save] Slot limit reached (%d). Delete a slot before creating another."), MaxSaveSlots);
		return FString();
	}

	// A brand-new save object is the reset: no puzzle records, no solve order, no played
	// narrative triggers, progression back to W1L0. Boards re-capture their authored grid
	// through EnsureInitialGridSaved on their next BeginPlay.
	SaveObject = Cast<UML_GameSave>(UGameplayStatics::CreateSaveGameObject(UML_GameSave::StaticClass()));
	if (!SaveObject) return FString();

	SaveObject->DisplayName         = TEXT("New Game");
	SaveObject->CurrentLevelTagName = ML_GameplayTags::Level_World1_Level0.GetTag().GetTagName();

	SetActiveSlot(NewSlot);
	SaveToDisk();

	UE_LOG(LogTemp, Log, TEXT("[Save] Created new game slot '%s'."), *NewSlot);
	return NewSlot;
}

bool UML_SaveSubsystem::ContinueFromSlot(const FString& InSlotName, FGameplayTag& OutLevelTag)
{
	OutLevelTag = FGameplayTag::EmptyTag;
	if (InSlotName.IsEmpty()) return false;

	const bool bIsDemo = IsDemoSlotID(InSlotName);

	UML_GameSave* Loaded = bIsDemo
		? LoadDemoSave(InSlotName.RightChop(DemoSlotPrefix.Len()))
		: Cast<UML_GameSave>(UGameplayStatics::LoadGameFromSlot(InSlotName, UserIndex));

	if (!Loaded)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Save] Could not load slot '%s'."), *InSlotName);
		return false;
	}

	FString TargetSlot = InSlotName;

	if (bIsDemo)
	{
		// The demo file stays exactly as packaged: the session continues in a fresh copy, so
		// every later autosave lands there instead. Failing here (slot limit) is better than
		// falling back to writing into the demo.
		TargetSlot = GenerateNewSlotName();
		if (TargetSlot.IsEmpty())
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[Save] Slot limit reached (%d) — cannot duplicate demo '%s'. Delete a slot first."),
				MaxSaveSlots, *InSlotName);
			return false;
		}

		Loaded->DisplayName = FString::Printf(TEXT("%s - %s"),
			*Loaded->DisplayName, *FDateTime::Now().ToString(TEXT("%d/%m %H:%M")));
	}

	SaveObject = Loaded;
	SetActiveSlot(TargetSlot);
	SaveToDisk();

	OutLevelTag = FGameplayTag::RequestGameplayTag(SaveObject->CurrentLevelTagName, /*ErrorIfNotFound=*/false);
	if (!OutLevelTag.IsValid())
		OutLevelTag = ML_GameplayTags::Level_World1_Level0;

	UE_LOG(LogTemp, Log, TEXT("[Save] Continuing '%s' in slot '%s' (level '%s')."),
		*InSlotName, *TargetSlot, *OutLevelTag.ToString());

	return true;
}

bool UML_SaveSubsystem::DeleteSlot(const FString& InSlotName)
{
	if (InSlotName.IsEmpty()) return false;

	if (IsDemoSlotID(InSlotName))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Save] Demo slot '%s' cannot be deleted."), *InSlotName);
		return false;
	}

	if (!UGameplayStatics::DeleteGameInSlot(InSlotName, UserIndex))
		return false;

	// Deleting the slot we are playing leaves nothing to write to; drop it rather than
	// silently recreating the file on the next autosave.
	if (ActiveSlotName == InSlotName)
	{
		ActiveSlotName.Empty();
		SaveObject = nullptr;
	}

	UE_LOG(LogTemp, Log, TEXT("[Save] Deleted slot '%s'."), *InSlotName);
	return true;
}

void UML_SaveSubsystem::EnsureActiveSlot()
{
	if (!ActiveSlotName.IsEmpty()) return;

	if (UGameplayStatics::DoesSaveGameExist(FallbackSlotName, UserIndex))
		SaveObject = Cast<UML_GameSave>(UGameplayStatics::LoadGameFromSlot(FallbackSlotName, UserIndex));

	if (!SaveObject)
		SaveObject = Cast<UML_GameSave>(UGameplayStatics::CreateSaveGameObject(UML_GameSave::StaticClass()));

	SetActiveSlot(FallbackSlotName);

	UE_LOG(LogTemp, Log, TEXT("[Save] No slot picked (level entered without the menu) — using fallback slot '%s'."),
		*FallbackSlotName);
}

// ==================== Level ====================

FGameplayTag UML_SaveSubsystem::GetCurrentLevel() const
{
	if (!SaveObject) return FGameplayTag::EmptyTag;
	return FGameplayTag::RequestGameplayTag(SaveObject->CurrentLevelTagName, /*ErrorIfNotFound=*/false);
}

void UML_SaveSubsystem::SetCurrentLevel(FGameplayTag LevelTag, const FString& InDisplayName)
{
	if (!SaveObject || !LevelTag.IsValid()) return;

	const FName NewTagName = LevelTag.GetTagName();
	const bool bUnchanged = SaveObject->CurrentLevelTagName == NewTagName
		&& SaveObject->DisplayName == InDisplayName;
	if (bUnchanged) return;

	SaveObject->CurrentLevelTagName = NewTagName;
	if (!InDisplayName.IsEmpty())
		SaveObject->DisplayName = InDisplayName;

	SaveToDisk();
}

// ==================== Demo authoring (dev only) ====================

bool UML_SaveSubsystem::ExportActiveSlotAsDemo(const FString& DemoName, const FString& Label)
{
	if (!SaveObject)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Save] Nothing to export: no save is loaded."));
		return false;
	}

	// DemoName becomes a file name, so refuse anything the filesystem would reject instead of
	// failing later on with a generic write error (a quote or a space is the usual mistake:
	// the label goes in the *second* argument, unquoted).
	if (DemoName.IsEmpty() || DemoName != FPaths::MakeValidFileName(DemoName))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Save] '%s' is not a usable demo save name. Use a plain name with no spaces or quotes, ")
			TEXT("and put the displayed label after it: ml.ExportDemoSave Demo_B Demo - Playtest Level 2"),
			*DemoName);
		return false;
	}

	// The label only belongs to the exported copy, so restore the live object afterwards.
	const FString PreviousDisplayName = SaveObject->DisplayName;
	if (!Label.IsEmpty())
		SaveObject->DisplayName = Label;

	TArray<uint8> Bytes;
	bool bSuccess = UGameplayStatics::SaveGameToMemory(SaveObject, Bytes);

	if (bSuccess)
	{
		const FString Dir = GetDemoSaveDir();
		IFileManager::Get().MakeDirectory(*Dir, /*Tree=*/true);
		bSuccess = FFileHelper::SaveArrayToFile(Bytes, *(Dir / (DemoName + TEXT(".sav"))));
	}

	SaveObject->DisplayName = PreviousDisplayName;

	UE_LOG(LogTemp, Log, TEXT("[Save] Export of demo save '%s' %s."),
		*DemoName, bSuccess ? TEXT("succeeded") : TEXT("FAILED"));

	return bSuccess;
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

const FML_PuzzleSaveRecord* UML_SaveSubsystem::FindPuzzleRecord(FName PuzzleID) const
{
	if (!SaveObject || PuzzleID.IsNone()) return nullptr;
	return SaveObject->PuzzleRecords.Find(PuzzleID);
}

FML_PuzzleSaveRecord UML_SaveSubsystem::GetPuzzleRecord(FName PuzzleID) const
{
	const FML_PuzzleSaveRecord* Record = FindPuzzleRecord(PuzzleID);
	return Record ? *Record : FML_PuzzleSaveRecord{};
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

void UML_SaveSubsystem::SaveGridSnapshot(FName PuzzleID, const TArray<FML_TileSaveEntry>& Entries)
{
	if (!SaveObject || PuzzleID.IsNone()) return;

	// Only the grid is updated — solved flag, solve order and LastSolvedPuzzleID are
	// intentionally left untouched (the hub isn't "solved" until fully revitalized).
	FML_PuzzleSaveRecord& Record = SaveObject->PuzzleRecords.FindOrAdd(PuzzleID);
	Record.SolvedGrid = Entries;

	SaveToDisk();
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
	const FML_PuzzleSaveRecord* Record = FindPuzzleRecord(PuzzleID);
	return Record && Record->bIsSolved;
}

FName UML_SaveSubsystem::GetLastSolvedPuzzleID() const
{
	return SaveObject ? SaveObject->LastSolvedPuzzleID : NAME_None;
}

// ==================== Progression ====================

EML_ProgressionState UML_SaveSubsystem::GetProgressionState() const
{
	return SaveObject ? SaveObject->ProgressionState : EML_ProgressionState::W1L0;
}

void UML_SaveSubsystem::SetProgressionState(EML_ProgressionState NewState)
{
	if (!SaveObject) return;
	SaveObject->ProgressionState = NewState;
	SaveToDisk();
}

// ==================== Story beats ====================

bool UML_SaveSubsystem::HasStoryBeatPlayed(FName BeatID) const
{
	if (!SaveObject || BeatID.IsNone()) return false;
	return SaveObject->PlayedNarrativeTriggers.Contains(BeatID);
}

void UML_SaveSubsystem::MarkStoryBeatPlayed(FName BeatID)
{
	if (!SaveObject || BeatID.IsNone()) return;

	bool bAlreadyPresent = false;
	SaveObject->PlayedNarrativeTriggers.Add(BeatID, &bAlreadyPresent);

	// Avoid a redundant disk write if it was already recorded.
	if (!bAlreadyPresent)
		SaveToDisk();
}

void UML_SaveSubsystem::ClearStoryBeatPlayed(FName BeatID)
{
	if (!SaveObject || BeatID.IsNone()) return;

	if (SaveObject->PlayedNarrativeTriggers.Remove(BeatID) > 0)
		SaveToDisk();
}

void UML_SaveSubsystem::ClearAllStoryBeats(bool bWriteToDisk)
{
	if (!SaveObject || SaveObject->PlayedNarrativeTriggers.IsEmpty()) return;

	UE_LOG(LogTemp, Log, TEXT("[Save] Cleared %d played story beat(s)."),
		SaveObject->PlayedNarrativeTriggers.Num());

	SaveObject->PlayedNarrativeTriggers.Empty();

	if (bWriteToDisk)
		SaveToDisk();
}

// ==================== Narrative triggers (legacy names) ====================

void UML_SaveSubsystem::SetNarrativeTriggerPlayed(FName TriggerID)
{
	MarkStoryBeatPlayed(TriggerID);
}

void UML_SaveSubsystem::ClearNarrativeTriggerPlayed(FName TriggerID)
{
	ClearStoryBeatPlayed(TriggerID);
}

bool UML_SaveSubsystem::IsNarrativeTriggerPlayed(FName TriggerID) const
{
	return HasStoryBeatPlayed(TriggerID);
}

void UML_SaveSubsystem::ClearAllNarrativeTriggersPlayed(bool bWriteToDisk)
{
	ClearAllStoryBeats(bWriteToDisk);
}
