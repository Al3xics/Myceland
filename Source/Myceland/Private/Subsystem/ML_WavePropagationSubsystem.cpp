// Copyright Myceland Team, All Rights Reserved.


#include "Subsystem/ML_WavePropagationSubsystem.h"

#include "Developer Settings/ML_MycelandDeveloperSettings.h"
#include "Player/ML_PlayerController.h"
#include "Player/ML_PlayerCharacter.h"
#include "Subsystem/ML_WinLoseSubsystem.h"
#include "Tiles/ML_Tile.h"
#include "Data Asset/ML_BiomeTileSet.h"
#include "Waves/ML_PropagationWaves.h"
#include "Waves/ChildWaves/ML_WaveCollectible.h"

void UML_WavePropagationSubsystem::EnsureInitialized()
{
	WinLoseSubsystem = GetWorld()->GetSubsystem<UML_WinLoseSubsystem>();
	PlayerController = Cast<AML_PlayerController>(GetWorld()->GetFirstPlayerController());
	DevSettings = UML_MycelandDeveloperSettings::GetMycelandDeveloperSettings();
	ensure(PlayerController && WinLoseSubsystem);
}

void UML_WavePropagationSubsystem::CancelAllWaveTimers()
{
	if (!GetWorld()) return;
	FTimerManager& TM = GetWorld()->GetTimerManager();
	TM.ClearTimer(IntraWaveTimerHandle);
	TM.ClearTimer(InterWaveTimerHandle);
}

void UML_WavePropagationSubsystem::BeginTurnRecord_Internal(AML_Tile* OriginTile)
{
	if (!PlayerController) return;

	AML_PlayerCharacter* PC = Cast<AML_PlayerCharacter>(PlayerController->GetPawn());
	if (!PC || !PC->CurrentTileOn) return;

	bHasActiveTurnRecord = true;
	CurrentTurnRecord = FML_TurnUndoRecord{};
	CurrentTurnRecord.EnergyBefore = PlayerController->CurrentEnergy;
	CurrentTurnRecord.PlayerAxialBefore = PC->CurrentTileOn->GetAxialCoord();
	CurrentTurnRecord.PlayerWorldBefore = PC->GetActorLocation();
	CurrentTurnRecord.OriginTile = OriginTile;
}

void UML_WavePropagationSubsystem::CommitTurnRecord_Internal()
{
	if (!bHasActiveTurnRecord) return;
	UndoStack.Add(CurrentTurnRecord);
	bHasActiveTurnRecord = false;
	CurrentTurnRecord = FML_TurnUndoRecord{};
}

void UML_WavePropagationSubsystem::DiscardTurnRecord_Internal()
{
	bHasActiveTurnRecord = false;
	CurrentTurnRecord = FML_TurnUndoRecord{};
}

void UML_WavePropagationSubsystem::RecordTileBeforeChange(AML_Tile* Tile, int32 DistanceFromOrigin)
{
	if (!bHasActiveTurnRecord || !IsValid(Tile)) return;

	// for (const FML_TileUndoDelta& D : CurrentTurnRecord.TileDeltas)
	// 	if (D.Tile.Get() == Tile)
	// 		return;

	FML_TileUndoDelta Delta;
	Delta.Tile = Tile;
	Delta.OldType = Tile->GetCurrentType();
	Delta.bOldConsumedGrass = Tile->bConsumedGrass;
	Delta.bOldHasCollectible = Tile->HasCollectible();

	// ordering
	Delta.PriorityIndex = CurrentPriorityIndexForRecording;
	Delta.DistanceFromOrigin = DistanceFromOrigin;
	Delta.Sequence = UndoSequenceCounter++;

	CurrentTurnRecord.TileDeltas.Add(Delta);
}

void UML_WavePropagationSubsystem::RecordSpawnedActor(AActor* Spawned, int32 DistanceFromOrigin)
{
	if (!bHasActiveTurnRecord || !IsValid(Spawned)) return;

	FML_SpawnUndoDelta D;
	D.SpawnedActor = Spawned;

	D.PriorityIndex = CurrentPriorityIndexForRecording;
	D.DistanceFromOrigin = DistanceFromOrigin;
	D.Sequence = UndoSequenceCounter++;

	CurrentTurnRecord.SpawnDeltas.Add(D);
}

void UML_WavePropagationSubsystem::EndTileResolved()
{
	bIsResolvingTiles = false;

	// Only commit if we actually started a record (i.e. a “turn” happened)
	CommitTurnRecord_Internal();

	if (PlayerController)
		PlayerController->EnableInput(PlayerController);
}

void UML_WavePropagationSubsystem::BeginTileResolved(AML_Tile* HitTile)
{
	if (!HitTile || bIsResolvingTiles) return;
	if (!PlayerController) EnsureInitialized();
	if (!PlayerController || !DevSettings) return;

	bIsResolvingTiles = true;
	PlayerController->DisableInput(PlayerController);

	CurrentOriginTile = HitTile;
	CurrentWaveIndex = 0;
	bCycleHasChanges = false;

	ParasitesThatAteGrass.Empty();
	PendingChanges.Empty();

	// Start undo record BEFORE waves mutate the world
	BeginTurnRecord_Internal(HitTile);

	ProcessNextWave();
}

void UML_WavePropagationSubsystem::RunWave()
{
	if (PendingChanges.Num() == 0)
	{
		ScheduleNextPriority();
		return;
	}

	const int32 CurrentDistance = PendingChanges[0].DistanceFromOrigin;

	TArray<FML_WaveChange> CurrentWave;
	int32 Index = 0;
	while (Index < PendingChanges.Num() && PendingChanges[Index].DistanceFromOrigin == CurrentDistance)
	{
		CurrentWave.Add(PendingChanges[Index]);
		Index++;
	}

	PendingChanges.RemoveAt(0, CurrentWave.Num());

	for (const FML_WaveChange& Change : CurrentWave)
	{
		if (Change.Tile)
		{
			AML_Tile* Tile = Change.Tile;
			if (!IsValid(Tile)) continue;

			// ---- Record before mutation (Undo) ----
			RecordTileBeforeChange(Tile, Change.DistanceFromOrigin);

			const UML_BiomeTileSet* TileSet = Tile->GetBoardSpawnerFromTile()->GetBiomeTileSet();
			ensureMsgf(TileSet, TEXT("TileSet is not set for BoardSpawner : %s"), *Tile->GetBoardSpawnerFromTile()->GetName());
			if (!TileSet) return;

			// IMPORTANT: use "silent" update during undo, but normal during forward
			if (!bUndoInProgress)
			{
				Tile->UpdateClassAtRuntime(Change.TargetType, TileSet->GetClassFromTileType(Change.TargetType));
			}
			else
			{
				Tile->UpdateClassAtRuntime_Silent(Change.TargetType, TileSet->GetClassFromTileType(Change.TargetType));
			}

			// your parasite bookkeeping
			if (Tile->GetCurrentType() == EML_TileType::Parasite && Tile->bConsumedGrass)
			{
				ParasitesThatAteGrass.Add(Tile);
				Tile->bConsumedGrass = false;
			}

			// cycle change detection
			if (Tile->GetCurrentType() != Change.TargetType) // defensive; but normally equals
				bCycleHasChanges = true;
			else
			{
				// Use OldType capture if you prefer strict logic:
				// if (OldType != Change.TargetType) bCycleHasChanges = true;
				// (OldType was recorded in delta anyway)
			}
		}
		else if (Change.CollectibleClass)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			AActor* Spawned = GetWorld()->SpawnActor<AActor>(Change.CollectibleClass, Change.SpawnLocation, FRotator::ZeroRotator, Params);

			RecordSpawnedActor(Spawned, Change.DistanceFromOrigin);
			bCycleHasChanges = true;
		}
	}

	if (PendingChanges.Num() > 0)
	{
		GetWorld()->GetTimerManager().SetTimer(
			IntraWaveTimerHandle,
			this,
			&UML_WavePropagationSubsystem::RunWave,
			DevSettings->IntraWaveDelay,
			false
		);
	}
	else
	{
		ScheduleNextPriority();
	}
}

void UML_WavePropagationSubsystem::ScheduleNextPriority()
{
	GetWorld()->GetTimerManager().SetTimer(
		InterWaveTimerHandle,
		this,
		&UML_WavePropagationSubsystem::ProcessNextWave,
		DevSettings->InterWaveDelay,
		false
	);
}

void UML_WavePropagationSubsystem::ProcessNextWave()
{
	if (!DevSettings || !CurrentOriginTile)
	{
		EndTileResolved();
		return;
	}

	if (CurrentWaveIndex >= DevSettings->WavesPriority.Num())
	{
		if (bCycleHasChanges)
		{
			CurrentWaveIndex = 0;
			bCycleHasChanges = false;
			ProcessNextWave();
		}
		else
		{
			EndTileResolved();
		}
		return;
	}

	UML_PropagationWaves* WaveLogic = DevSettings->WavesPriority[CurrentWaveIndex]->GetDefaultObject<UML_PropagationWaves>();
	ensureMsgf(WaveLogic, TEXT("Wave logic is not set!"));
	if (!WaveLogic)
	{
		CurrentWaveIndex++;
		ProcessNextWave();
		return;
	}

	CurrentPriorityIndexForRecording = CurrentWaveIndex;
	CurrentWaveIndex++;

	PendingChanges.Empty();

	if (UML_WaveCollectible* CollectibleWave = Cast<UML_WaveCollectible>(WaveLogic))
	{
		CollectibleWave->ComputeWaveForCollectibles(CurrentOriginTile, ParasitesThatAteGrass, PendingChanges);
		ParasitesThatAteGrass.Empty();
	}
	else
	{
		WaveLogic->ComputeWave(CurrentOriginTile, PendingChanges);
	}

	if (PendingChanges.Num() == 0)
	{
		ProcessNextWave();
		return;
	}

	PendingChanges.Sort([](const FML_WaveChange& A, const FML_WaveChange& B)
	{
		return A.DistanceFromOrigin < B.DistanceFromOrigin;
	});

	RunWave();
}

void UML_WavePropagationSubsystem::ApplyUndoRecord(const FML_TurnUndoRecord& Record)
{
	if (!PlayerController) return;

	// Restore energy
	PlayerController->CurrentEnergy = Record.EnergyBefore;

	// Destroy spawned collectibles (reverse order is safer)
	for (int32 i = Record.SpawnDeltas.Num() - 1; i >= 0; --i)
	{
		if (AActor* A = Record.SpawnDeltas[i].SpawnedActor.Get())
		{
			if (IsValid(A))
				A->Destroy();
		}
	}

	// Restore tiles (reverse order usually safer if you have dependencies)
	bUndoInProgress = true;
	for (int32 i = Record.TileDeltas.Num() - 1; i >= 0; --i)
	{
		const FML_TileUndoDelta& D = Record.TileDeltas[i];
		AML_Tile* Tile = D.Tile.Get();
		if (!IsValid(Tile)) continue;

		const UML_BiomeTileSet* TileSet = Tile->GetBoardSpawnerFromTile()->GetBiomeTileSet();
		if (!TileSet) continue;

		Tile->UpdateClassAtRuntime_Silent(D.OldType, TileSet->GetClassFromTileType(D.OldType));
		Tile->SetHasCollectible(D.bOldHasCollectible);
		Tile->bConsumedGrass = D.bOldConsumedGrass;
	}
	bUndoInProgress = false;

	// Restore player position (use path back to axial to keep your movement style)
	PlayerController->MovePlayerToAxial(Record.PlayerAxialBefore, /*bUsePath=*/true, /*bFallbackTeleport=*/true, Record.PlayerWorldBefore);
}

bool UML_WavePropagationSubsystem::UndoLastTurn()
{
	EnsureInitialized();
	if (!PlayerController) return false;

	// If waves are running, don’t undo (or cancel timers + clean state)
	if (bIsResolvingTiles)
		return false;

	if (UndoStack.Num() == 0)
		return false;

	CancelAllWaveTimers();

	// Pop last record
	const FML_TurnUndoRecord Record = UndoStack.Pop();

	ApplyUndoRecord(Record);
	return true;
}

void UML_WavePropagationSubsystem::RecordTileForUndo(AML_Tile* Tile, int32 DistanceFromOrigin)
{
	RecordTileBeforeChange(Tile, DistanceFromOrigin);
}

bool UML_WavePropagationSubsystem::UndoLastTurn_Animated()
{
	EnsureInitialized();
	if (!PlayerController || !DevSettings) return false;

	// Don't undo while forward resolving OR while another undo animation is running
	if (bIsResolvingTiles || bIsUndoAnimating) return false;
	if (UndoStack.Num() == 0) return false;

	CancelAllWaveTimers();
	PlayerController->DisableInput(PlayerController);

	bIsUndoAnimating = true;

	// Pop record and stage
	ActiveUndoRecord = UndoStack.Pop();

	// Restore energy immediately (UI feedback)
	PlayerController->CurrentEnergy = ActiveUndoRecord.EnergyBefore;

	// Copy deltas
	PendingUndoTileDeltas = ActiveUndoRecord.TileDeltas;
	PendingUndoSpawnDeltas = ActiveUndoRecord.SpawnDeltas;

	// Sort in reverse order:
	// - PriorityIndex desc (last wave undone first)
	// - DistanceFromOrigin desc (farther rings undone first)
	// - Sequence desc (stable ordering)
	PendingUndoTileDeltas.Sort([](const FML_TileUndoDelta& A, const FML_TileUndoDelta& B)
	{
		if (A.PriorityIndex != B.PriorityIndex) return A.PriorityIndex > B.PriorityIndex;
		if (A.DistanceFromOrigin != B.DistanceFromOrigin) return A.DistanceFromOrigin > B.DistanceFromOrigin;
		return A.Sequence > B.Sequence;
	});

	PendingUndoSpawnDeltas.Sort([](const FML_SpawnUndoDelta& A, const FML_SpawnUndoDelta& B)
	{
		if (A.PriorityIndex != B.PriorityIndex) return A.PriorityIndex > B.PriorityIndex;
		if (A.DistanceFromOrigin != B.DistanceFromOrigin) return A.DistanceFromOrigin > B.DistanceFromOrigin;
		return A.Sequence > B.Sequence;
	});

	// Kick off first undo wave immediately
	RunUndoWave();
	return true;
}

void UML_WavePropagationSubsystem::RunUndoWave()
{
	// If both empty -> finish
	if (PendingUndoTileDeltas.Num() == 0 && PendingUndoSpawnDeltas.Num() == 0)
	{
		FinishUndoAnimation();
		return;
	}

	// Determine next group key (PriorityIndex, DistanceFromOrigin) from the "front" of sorted arrays
	// Since arrays are sorted DESC, index 0 is next to apply.
	auto PeekKey = [](int32& OutPri, int32& OutDist, const bool bHasTile, const FML_TileUndoDelta& TD, const bool bHasSpawn, const FML_SpawnUndoDelta& SD)
	{
		// Pick the "highest" key among available heads
		// Compare (Pri, Dist) lexicographically desc
		if (bHasTile && !bHasSpawn)
		{
			OutPri = TD.PriorityIndex;
			OutDist = TD.DistanceFromOrigin;
			return;
		}
		if (!bHasTile && bHasSpawn)
		{
			OutPri = SD.PriorityIndex;
			OutDist = SD.DistanceFromOrigin;
			return;
		}

		// Both exist: compare heads
		if (TD.PriorityIndex != SD.PriorityIndex)
		{
			if (TD.PriorityIndex > SD.PriorityIndex) { OutPri = TD.PriorityIndex; OutDist = TD.DistanceFromOrigin; }
			else { OutPri = SD.PriorityIndex; OutDist = SD.DistanceFromOrigin; }
			return;
		}

		// same priority, compare distance
		if (TD.DistanceFromOrigin >= SD.DistanceFromOrigin)
		{
			OutPri = TD.PriorityIndex;
			OutDist = TD.DistanceFromOrigin;
		}
		else
		{
			OutPri = SD.PriorityIndex;
			OutDist = SD.DistanceFromOrigin;
		}
	};

	int32 GroupPri = 0;
	int32 GroupDist = 0;

	const bool bHasTile = PendingUndoTileDeltas.Num() > 0;
	const bool bHasSpawn = PendingUndoSpawnDeltas.Num() > 0;

	PeekKey(
		GroupPri, GroupDist,
		bHasTile, bHasTile ? PendingUndoTileDeltas[0] : FML_TileUndoDelta{},
		bHasSpawn, bHasSpawn ? PendingUndoSpawnDeltas[0] : FML_SpawnUndoDelta{}
	);

	ApplyUndoWaveGroup(GroupPri, GroupDist);

	// Decide delay: if we still have work, use IntraWaveDelay for same "priority sweep",
	// and InterWaveDelay when moving to next priority group.
	if (PendingUndoTileDeltas.Num() == 0 && PendingUndoSpawnDeltas.Num() == 0)
	{
		FinishUndoAnimation();
		return;
	}

	// Compute next key to decide if priority changes
	int32 NextPri = 0, NextDist = 0;
	const bool bHasTile2 = PendingUndoTileDeltas.Num() > 0;
	const bool bHasSpawn2 = PendingUndoSpawnDeltas.Num() > 0;

	PeekKey(
		NextPri, NextDist,
		bHasTile2, bHasTile2 ? PendingUndoTileDeltas[0] : FML_TileUndoDelta{},
		bHasSpawn2, bHasSpawn2 ? PendingUndoSpawnDeltas[0] : FML_SpawnUndoDelta{}
	);

	const float Delay = (NextPri != GroupPri) ? DevSettings->InterWaveDelay : DevSettings->IntraWaveDelay;
	ScheduleNextUndoWave(Delay);
}

void UML_WavePropagationSubsystem::ApplyUndoWaveGroup(int32 PriorityIndex, int32 DistanceFromOrigin)
{
	// Destroy collectibles first (so tiles revert after visuals vanish)
	{
		int32 i = 0;
		while (i < PendingUndoSpawnDeltas.Num())
		{
			const FML_SpawnUndoDelta& SD = PendingUndoSpawnDeltas[i];
			if (SD.PriorityIndex == PriorityIndex && SD.DistanceFromOrigin == DistanceFromOrigin)
			{
				if (AActor* A = SD.SpawnedActor.Get())
				{
					if (IsValid(A))
						A->Destroy();
				}
				PendingUndoSpawnDeltas.RemoveAt(i);
				continue;
			}
			i++;
		}
	}

	// Revert tiles (silent)
	bUndoInProgress = true;
	{
		int32 i = 0;
		while (i < PendingUndoTileDeltas.Num())
		{
			const FML_TileUndoDelta& TD = PendingUndoTileDeltas[i];
			if (TD.PriorityIndex == PriorityIndex && TD.DistanceFromOrigin == DistanceFromOrigin)
			{
				AML_Tile* Tile = TD.Tile.Get();
				if (IsValid(Tile))
				{
					const UML_BiomeTileSet* TileSet = Tile->GetBoardSpawnerFromTile()->GetBiomeTileSet();
					if (TileSet)
					{
						Tile->UpdateClassAtRuntime_Silent(TD.OldType, TileSet->GetClassFromTileType(TD.OldType));
						Tile->SetHasCollectible(TD.bOldHasCollectible);
						Tile->bConsumedGrass = TD.bOldConsumedGrass;
					}
				}
				PendingUndoTileDeltas.RemoveAt(i);
				continue;
			}
			i++;
		}
	}
	bUndoInProgress = false;
}

void UML_WavePropagationSubsystem::ScheduleNextUndoWave(float Delay)
{
	if (!GetWorld()) return;

	GetWorld()->GetTimerManager().SetTimer(
		IntraWaveTimerHandle, // reuse handle (we already clear timers at start)
		this,
		&UML_WavePropagationSubsystem::RunUndoWave,
		Delay,
		false
	);
}

void UML_WavePropagationSubsystem::FinishUndoAnimation()
{
	// Restore player position at the end (feels like "rewind aftermath")
	if (PlayerController)
	{
		PlayerController->MovePlayerToAxial(
			ActiveUndoRecord.PlayerAxialBefore,
			/*bUsePath=*/true,
			/*bFallbackTeleport=*/true,
			ActiveUndoRecord.PlayerWorldBefore
		);

		PlayerController->EnableInput(PlayerController);
	}

	// Clear undo state
	bIsUndoAnimating = false;
	bUndoInProgress = false;
	PendingUndoTileDeltas.Reset();
	PendingUndoSpawnDeltas.Reset();
	ActiveUndoRecord = FML_TurnUndoRecord{};
}