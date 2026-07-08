// Copyright Myceland Team, All Rights Reserved.

#include "Tiles/ML_HubBoardSpawner.h"
#include "Actors/ML_WaterNavPath.h"
#include "Components/ChildActorComponent.h"
#include "Data Asset/ML_BiomeTileSet.h"
#include "Save System/ML_SaveSubsystem.h"
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

	// The base BeginPlay's solved-restore — which respawns AssociatedWaterPaths on load — is
	// skipped for the hub (ShouldAutoRestoreSolvedGrid()==false). Replay the hub's own water-
	// bridge activation here the same way as the regular board: gated on the hub being marked
	// solved in the save (i.e. fully revitalized), deferred one tick so each WaterNavPath has
	// finished its own BeginPlay before we reveal it.
	if (UML_SaveSubsystem* SaveSys = GetSaveSubsystem())
	{
		if (PuzzleID.IsValid() && SaveSys->IsPuzzleSolved(PuzzleID.GetTagName()))
		{
			TArray<AActor*> PathsToSpawn = GetAssociatedWaterPaths();
			GetWorld()->GetTimerManager().SetTimerForNextTick([PathsToSpawn]()
			{
				for (AActor* PathActor : PathsToSpawn)
				{
					if (AML_WaterNavPath* NavPath = Cast<AML_WaterNavPath>(PathActor))
						NavPath->Spawn();
				}
			});

			// Hide/disable the hub's associated obstacle on load, the same way the base board
			// does in its solved-restore branch (which the hub skips).
			if (AActor* Obstacle = GetAssociatedObstacle())
			{
				Obstacle->SetActorHiddenInGame(true);
				Obstacle->SetActorEnableCollision(false);
			}
		}
	}

	// On load, reveal the connected goals for the puzzles that were already solved.
	// Deferred one tick so the tile Blueprints that draw the links have finished BeginPlay.
	// bImmediate=true broadcasts synchronously: it avoids the shared connected-goal queue/
	// timer, which the player's initial board placement would otherwise wipe via
	// ResetConnectedGoalPathState before the reveal finished.
	if (AppliedEntryIndices.Num() > 0)
	{
		if (UWorld* World = GetWorld())
		{
			TWeakObjectPtr<AML_HubBoardSpawner> WeakThis(this);
			World->GetTimerManager().SetTimerForNextTick([WeakThis]()
			{
				AML_HubBoardSpawner* Hub = WeakThis.Get();
				if (Hub && IsValid(Hub->WinLoseSubsystem))
					Hub->WinLoseSubsystem->TriggerConnectedGoalAnimationForBoard(Hub, /*bImmediate=*/true);
			});
		}
	}
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

	// Persist the reverted state so the change survives a reload. Never marked solved —
	// a revert by definition means the hub is no longer fully revitalized.
	PersistHubGrid(/*bMarkSolved=*/false);

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
		WavePropagationSubsystem->BeginTileResolved(FirstPlacedTile);

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
			WavePropagationSubsystem->BeginTileResolved(Change.TargetTile);
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

	const bool bFullyRevitalized = PuzzleEntries.Num() > 0 && AppliedEntryIndices.Num() == PuzzleEntries.Num();

	// The tiles (and any wave propagation they triggered) have now settled — this is the
	// definitive change we persist. Marking the hub solved only happens once every entry
	// is placed, so the hub becomes the respawn anchor only when fully revitalized.
	PersistHubGrid(/*bMarkSolved=*/bFullyRevitalized);

	if (bFullyRevitalized)
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

// On BeginPlay (and after a cascade reset), restores hub visuals for every linked puzzle
// currently marked solved — without triggering delegates or delays.
//
// Solved state is read from the save (authoritative and order-independent), so this no
// longer depends on each linked puzzle's own BeginPlay having run first. Snapshots for
// revert are taken while tiles are still authored (the base auto-restore is skipped),
// then the board is repainted from the persisted grid when available.
void AML_HubBoardSpawner::RehydrateAlreadySolvedPuzzles()
{
	UML_SaveSubsystem* SaveSys = GetSaveSubsystem();

	bool bAnyApplied = false;
	for (int32 i = 0; i < PuzzleEntries.Num(); ++i)
	{
		const FML_HubPuzzleEntry& Entry = PuzzleEntries[i];
		if (!IsValid(Entry.LinkedPuzzle)) continue;
		if (AppliedEntryIndices.Contains(i)) continue;

		const bool bSolved = SaveSys
			? SaveSys->IsPuzzleSolved(Entry.LinkedPuzzle->PuzzleID.GetTagName())
			: Entry.LinkedPuzzle->bIsPuzzleSolved;
		if (!bSolved) continue;

		TakeEntrySnapshot(i);
		AppliedEntryIndices.Add(i);
		bAnyApplied = true;
	}

	if (!bAnyApplied) return;

	// Prefer the persisted grid (captures wave-propagation results the per-entry
	// TileChanges can't reproduce). Fall back for saves written before hub-grid persistence.
	if (RestoreSavedHubGrid()) return;

	for (int32 i = 0; i < PuzzleEntries.Num(); ++i)
	{
		if (!AppliedEntryIndices.Contains(i)) continue;
		for (const FML_HubTileChange& Change : PuzzleEntries[i].TileChanges)
		{
			UClass* const MLTileClass = GetClassForChange(Change);
			if (!IsValid(Change.TargetTile) || !MLTileClass) continue;
			Change.TargetTile->UpdateClassAtRuntime_Silent(Change.TargetType, MLTileClass);
		}
	}
}

// Repaints the board from the hub's persisted grid snapshot. Returns false when no
// snapshot exists so the caller can fall back to replaying the per-entry TileChanges.
bool AML_HubBoardSpawner::RestoreSavedHubGrid()
{
	if (!PuzzleID.IsValid()) return false;

	UML_SaveSubsystem* SaveSys = GetSaveSubsystem();
	UML_BiomeTileSet* Biome = GetBiomeTileSet();
	if (!SaveSys || !Biome) return false;

	const FML_PuzzleSaveRecord Record = SaveSys->GetPuzzleRecord(PuzzleID.GetTagName());
	if (Record.SolvedGrid.Num() == 0) return false;

	const TMap<FIntPoint, AML_Tile*>& Grid = GetGridMapRef();
	for (const FML_TileSaveEntry& Entry : Record.SolvedGrid)
	{
		if (AML_Tile* Tile = Grid.FindRef(Entry.Axial))
			Tile->UpdateClassAtRuntime_Silent(Entry.TileType, Biome->GetClassFromTileType(Entry.TileType));
	}
	return true;
}

// Writes the current hub grid to the save. bMarkSolved additionally flags the hub as
// solved (respawn anchor) via MarkPuzzleSolved; otherwise only the grid is stored.
void AML_HubBoardSpawner::PersistHubGrid(bool bMarkSolved)
{
	if (!PuzzleID.IsValid()) return;

	UML_SaveSubsystem* SaveSys = GetSaveSubsystem();
	if (!SaveSys) return;

	if (bMarkSolved)
		SaveSys->MarkPuzzleSolved(PuzzleID.GetTagName(), SnapshotGrid(), GetLevelKey());
	else
		SaveSys->SaveGridSnapshot(PuzzleID.GetTagName(), SnapshotGrid());
}

// The hub is never reset as a single board. When a cascade reset reaches it, drop the
// per-entry bookkeeping, let the base repaint the authored grid, then repaint from the
// linked puzzles that are still solved — rather than blanking the whole hub to dirt.
void AML_HubBoardSpawner::RestoreToInitialState()
{
	CancelPendingSpawnTimers();
	AppliedEntryIndices.Empty();
	EntrySnapshots.Empty();

	Super::RestoreToInitialState();

	RehydrateAlreadySolvedPuzzles();
	PersistHubGrid(/*bMarkSolved=*/false);

	// Super cleared ALL of the hub's goal-link visuals; RehydrateAlreadySolvedPuzzles has
	// just repainted the grid for whichever linked puzzles are still solved. Re-reveal so
	// those remaining connections light back up (bImmediate — the grid is already settled).
	if (AppliedEntryIndices.Num() > 0 && IsValid(WinLoseSubsystem))
		WinLoseSubsystem->TriggerConnectedGoalAnimationForBoard(this, /*bImmediate=*/true);
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
