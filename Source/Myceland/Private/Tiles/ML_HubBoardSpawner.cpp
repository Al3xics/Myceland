// Copyright Myceland Team, All Rights Reserved.

#include "Tiles/ML_HubBoardSpawner.h"
#include "Components/ChildActorComponent.h"
#include "Subsystem/ML_WinLoseSubsystem.h"
#include "Subsystem/ML_RollBackSubsystem.h"
#include "Subsystem/ML_WavePropagationSubsystem.h"
#include "Tiles/ML_Tile.h"
#include "Tiles/TileBase/ML_TileDirt.h"
#include "Tiles/TileBase/ML_TileGrass.h"
#include "Tiles/TileBase/ML_TileObstacle.h"
#include "Tiles/TileBase/ML_TileParasite.h"
#include "Tiles/TileBase/ML_TileTree.h"
#include "Tiles/TileBase/ML_TileWater.h"

// Resolves the active typed class from a tile change based on its TargetType.
// Returns UClass* to avoid requiring complete type definitions of each tile subclass.
static UClass* GetClassForChange(const FML_HubTileChange& Change)
{
	switch (Change.TargetType)
	{
	case EML_TileType::Dirt:      return Change.DirtClass.Get();
	case EML_TileType::Grass:     return Change.GrassClass.Get();
	case EML_TileType::Parasite:  return Change.ParasiteClass.Get();
	case EML_TileType::Water:
	case EML_TileType::WaterPath: return Change.WaterClass.Get();
	case EML_TileType::Obstacle:  return Change.ObstacleClass.Get();
	case EML_TileType::Tree:      return Change.TreeClass.Get();
	default:                      return nullptr;
	}
}

AML_HubBoardSpawner::AML_HubBoardSpawner()
{
	PrimaryActorTick.bCanEverTick = false;
}

// ==================== Lifecycle ====================

void AML_HubBoardSpawner::BeginPlay()
{
	Super::BeginPlay();

	WinLoseSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UML_WinLoseSubsystem>() : nullptr;
	RollBackSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UML_RollBackSubsystem>() : nullptr;
	WavePropagationSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UML_WavePropagationSubsystem>() : nullptr;

	BindDelegates();
	RehydrateAlreadySolvedPuzzles();
}

void AML_HubBoardSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindDelegates();
	CancelPendingSpawnTimers();
	Super::EndPlay(EndPlayReason);
}

// ==================== Delegate Binding ====================

void AML_HubBoardSpawner::BindDelegates()
{
	if (WinLoseSubsystem)
	{
		// Remove before add to prevent double-binding
		WinLoseSubsystem->OnWin.RemoveDynamic(this, &AML_HubBoardSpawner::HandleWin);
		WinLoseSubsystem->OnWin.AddDynamic(this, &AML_HubBoardSpawner::HandleWin);
	}

	if (RollBackSubsystem)
	{
		RollBackSubsystem->OnResetAnimating.RemoveDynamic(this, &AML_HubBoardSpawner::HandleResetAnimating);
		RollBackSubsystem->OnResetAnimating.AddDynamic(this, &AML_HubBoardSpawner::HandleResetAnimating);
	}
}

void AML_HubBoardSpawner::UnbindDelegates()
{
	if (WinLoseSubsystem)
		WinLoseSubsystem->OnWin.RemoveDynamic(this, &AML_HubBoardSpawner::HandleWin);

	if (RollBackSubsystem)
		RollBackSubsystem->OnResetAnimating.RemoveDynamic(this, &AML_HubBoardSpawner::HandleResetAnimating);
}

// ==================== Win Handling ====================

// Called on every board win; ignores the hub itself, then records the current hub tile states and starts the tile change sequence for the matching puzzle entry.
void AML_HubBoardSpawner::HandleWin()
{
	if (!IsValid(WinLoseSubsystem)) return;

	AML_BoardSpawner* WonBoard = WinLoseSubsystem->CurrentBoardSpawner;
	if (!IsValid(WonBoard) || WonBoard == this) return;

	const int32 EntryIndex = FindEntryIndexForBoard(WonBoard);
	if (EntryIndex == INDEX_NONE || AppliedEntryIndices.Contains(EntryIndex)) return;

	AppliedEntryIndices.Add(EntryIndex);
	TakeEntrySnapshot(EntryIndex);
	ActivatePuzzleEntry(EntryIndex);
}

// ==================== Reset Handling ====================

// Captures the resetting board when animation starts, then reverts its hub tiles and removes it from applied entries when animation ends.
void AML_HubBoardSpawner::HandleResetAnimating(bool bIsResetAnimating)
{
	if (!IsValid(WinLoseSubsystem)) return;

	if (bIsResetAnimating)
	{
		// Capture the board before any transitions occur
		PendingResetBoard = WinLoseSubsystem->CurrentBoardSpawner;
		return;
	}

	if (!IsValid(PendingResetBoard) || PendingResetBoard == this)
	{
		PendingResetBoard = nullptr;
		return;
	}

	const int32 EntryIndex = FindEntryIndexForBoard(PendingResetBoard);
	PendingResetBoard = nullptr;

	if (EntryIndex == INDEX_NONE || !AppliedEntryIndices.Contains(EntryIndex)) return;

	CancelPendingSpawnTimers();
	RevertPuzzleEntryTiles(EntryIndex);
	AppliedEntryIndices.Remove(EntryIndex);
	OnHubPuzzleTilesReverted.Broadcast(EntryIndex);
}

// ==================== Tile Spawning ====================

// Fires OnBeforeHubTilesSpawn, then schedules tile changes after DelayBeforeFirstTile — all at once or one-by-one based on bSequentialSpawn.
void AML_HubBoardSpawner::ActivatePuzzleEntry(int32 EntryIndex)
{
	if (!PuzzleEntries.IsValidIndex(EntryIndex)) return;
	const FML_HubPuzzleEntry& Entry = PuzzleEntries[EntryIndex];

	OnBeforeHubTilesSpawn.Broadcast(EntryIndex);

	const float Delay = FMath::Max(0.01f, Entry.DelayBeforeFirstTile);
	FTimerHandle Handle;

	if (Entry.bSequentialSpawn)
	{
		GetWorld()->GetTimerManager().SetTimer(
			Handle,
			FTimerDelegate::CreateUObject(this, &AML_HubBoardSpawner::ApplySingleTileChange, EntryIndex, 0),
			Delay, false
		);
	}
	else
	{
		GetWorld()->GetTimerManager().SetTimer(
			Handle,
			FTimerDelegate::CreateUObject(this, &AML_HubBoardSpawner::ApplyAllTileChanges, EntryIndex),
			Delay, false
		);
	}

	SpawnTimerHandles.Add(Handle);
}

// Applies all tile changes for the entry simultaneously, then calls FinalizeTileChanges.
void AML_HubBoardSpawner::ApplyAllTileChanges(int32 EntryIndex)
{
	if (!PuzzleEntries.IsValidIndex(EntryIndex)) return;

	AML_Tile* FirstPlacedTile = nullptr;
	for (const FML_HubTileChange& Change : PuzzleEntries[EntryIndex].TileChanges)
	{
		UClass* const MLTileClass = GetClassForChange(Change);
		if (!IsValid(Change.TargetTile) || !MLTileClass) continue;
		Change.TargetTile->UpdateClassAtRuntime(Change.TargetType, MLTileClass);
		if (!FirstPlacedTile) FirstPlacedTile = Change.TargetTile;
	}

	// Trigger a full board propagation pass from any placed tile.
	// Waves scan the entire board, so one call is sufficient regardless of how many tiles changed.
	if (IsValid(FirstPlacedTile) && WavePropagationSubsystem && !WavePropagationSubsystem->IsResolvingTiles())
		WavePropagationSubsystem->BeginTileResolvedWithoutAvatarSurprise(FirstPlacedTile);

	FTimerHandle Handle;
	GetWorld()->GetTimerManager().SetTimer(
		Handle,
		FTimerDelegate::CreateUObject(this, &AML_HubBoardSpawner::FinalizeTileChanges, EntryIndex),
		FMath::Max(0.01f, PuzzleEntries[EntryIndex].DelayAfterLastTile), false
	);
	SpawnTimerHandles.Add(Handle);
}

// Applies the tile change at TileChangeIndex, then schedules the next one after SequentialDelay; calls FinalizeTileChanges when all are done.
void AML_HubBoardSpawner::ApplySingleTileChange(int32 EntryIndex, int32 TileChangeIndex)
{
	if (!PuzzleEntries.IsValidIndex(EntryIndex)) return;
	const FML_HubPuzzleEntry& Entry = PuzzleEntries[EntryIndex];

	if (!Entry.TileChanges.IsValidIndex(TileChangeIndex)) return;
	const FML_HubTileChange& Change = Entry.TileChanges[TileChangeIndex];

	UClass* const MLTileClass = GetClassForChange(Change);
	if (IsValid(Change.TargetTile) && MLTileClass)
	{
		Change.TargetTile->UpdateClassAtRuntime(Change.TargetType, MLTileClass);

		if (WavePropagationSubsystem && !WavePropagationSubsystem->IsResolvingTiles())
			WavePropagationSubsystem->BeginTileResolvedWithoutAvatarSurprise(Change.TargetTile);
	}

	const int32 NextIndex = TileChangeIndex + 1;

	FTimerHandle Handle;
	if (Entry.TileChanges.IsValidIndex(NextIndex))
	{
		GetWorld()->GetTimerManager().SetTimer(
			Handle,
			FTimerDelegate::CreateUObject(this, &AML_HubBoardSpawner::ApplySingleTileChange, EntryIndex, NextIndex),
			FMath::Max(0.01f, Entry.SequentialDelay), false
		);
	}
	else
	{
		GetWorld()->GetTimerManager().SetTimer(
			Handle,
			FTimerDelegate::CreateUObject(this, &AML_HubBoardSpawner::FinalizeTileChanges, EntryIndex),
			FMath::Max(0.01f, Entry.DelayAfterLastTile), false
		);
	}
	SpawnTimerHandles.Add(Handle);
}

// Broadcasts OnHubTileChangesEnd for the entry; also broadcasts OnHubAllTilesPlaced once every entry has been applied.
void AML_HubBoardSpawner::FinalizeTileChanges(int32 EntryIndex)
{
	OnHubTileChangesEnd.Broadcast(EntryIndex);

	if (PuzzleEntries.Num() > 0 && AppliedEntryIndices.Num() == PuzzleEntries.Num())
		OnHubAllTilesPlaced.Broadcast();

	if (!IsValid(WinLoseSubsystem)) return;
	WinLoseSubsystem->TriggerConnectedGoalAnimationForBoard(this);
}

// ==================== Tile Revert ====================

// Restores each hub tile in the entry to its pre-win type and class using the snapshot taken at win time.
void AML_HubBoardSpawner::RevertPuzzleEntryTiles(int32 EntryIndex)
{
	if (!PuzzleEntries.IsValidIndex(EntryIndex)) return;
	const FML_HubPuzzleEntry& Entry = PuzzleEntries[EntryIndex];

	const FML_HubEntrySnapshot* EntrySnap = EntrySnapshots.Find(EntryIndex);
	if (!EntrySnap) return;

	for (int32 i = 0; i < Entry.TileChanges.Num() && i < EntrySnap->Snapshots.Num(); ++i)
	{
		const FML_HubTileChange& Change = Entry.TileChanges[i];
		const FML_HubTileSnapshot& Snapshot = EntrySnap->Snapshots[i];

		if (!IsValid(Change.TargetTile) || !Snapshot.OriginalClass) continue;
		Change.TargetTile->UpdateClassAtRuntime(Snapshot.OriginalType, Snapshot.OriginalClass);
	}

	EntrySnapshots.Remove(EntryIndex);
}

// ==================== Initialization ====================

// On BeginPlay, silently applies tile changes for any linked puzzle already marked solved — restores hub visuals without triggering delegates or delays.
// Requires bIsPuzzleSolved to be set on each linked puzzle before this hub's BeginPlay runs (e.g. by a save system, not during the puzzle's own BeginPlay).
void AML_HubBoardSpawner::RehydrateAlreadySolvedPuzzles()
{
	for (int32 i = 0; i < PuzzleEntries.Num(); ++i)
	{
		const FML_HubPuzzleEntry& Entry = PuzzleEntries[i];
		if (!IsValid(Entry.LinkedPuzzle) || !Entry.LinkedPuzzle->bIsPuzzleSolved) continue;
		if (AppliedEntryIndices.Contains(i)) continue;

		TakeEntrySnapshot(i);
		AppliedEntryIndices.Add(i);

		for (const FML_HubTileChange& Change : Entry.TileChanges)
		{
			UClass* const MLTileClass = GetClassForChange(Change);
			if (!IsValid(Change.TargetTile) || !MLTileClass) continue;
			Change.TargetTile->UpdateClassAtRuntime_Silent(Change.TargetType, MLTileClass);
		}
	}
}

// Captures the current type and class of each hub tile in the entry so they can be restored if the associated puzzle is reset.
void AML_HubBoardSpawner::TakeEntrySnapshot(int32 EntryIndex)
{
	if (!PuzzleEntries.IsValidIndex(EntryIndex)) return;

	FML_HubEntrySnapshot EntrySnap;

	for (const FML_HubTileChange& Change : PuzzleEntries[EntryIndex].TileChanges)
	{
		FML_HubTileSnapshot Snapshot;

		if (IsValid(Change.TargetTile))
		{
			Snapshot.OriginalType = Change.TargetTile->GetCurrentType();

			if (const UChildActorComponent* Child = Change.TargetTile->GetTileChildActor())
				Snapshot.OriginalClass = Child->GetChildActorClass().Get();
		}

		EntrySnap.Snapshots.Add(Snapshot);
	}

	EntrySnapshots.Add(EntryIndex, MoveTemp(EntrySnap));
}

// ==================== Utilities ====================

void AML_HubBoardSpawner::CancelPendingSpawnTimers()
{
	if (UWorld* World = GetWorld())
	{
		for (FTimerHandle& Handle : SpawnTimerHandles)
			World->GetTimerManager().ClearTimer(Handle);
	}
	
	SpawnTimerHandles.Empty();
}

int32 AML_HubBoardSpawner::FindEntryIndexForBoard(const AML_BoardSpawner* Board) const
{
	if (!IsValid(Board)) return INDEX_NONE;

	for (int32 i = 0; i < PuzzleEntries.Num(); ++i)
	{
		if (PuzzleEntries[i].LinkedPuzzle == Board)
			return i;
	}

	return INDEX_NONE;
}
