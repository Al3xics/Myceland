// Copyright Myceland Team, All Rights Reserved.

#include "Subsystem/ML_WavePropagationSubsystem.h"

#include "Developer Settings/ML_MycelandDeveloperSettings.h"
#include "Player/ML_PlayerController.h"
#include "Player/ML_PlayerCharacter.h"
#include "Subsystem/ML_WinLoseSubsystem.h"
#include "Tiles/ML_Tile.h"
#include "Tiles/ML_BoardSpawner.h"
#include "Data Asset/ML_BiomeTileSet.h"
#include "Waves/ML_PropagationWaves.h"
#include "Waves/ChildWaves/ML_WaveCollectible.h"
#include "Collectible/ML_Collectible.h"

void UML_WavePropagationSubsystem::EnsureInitialized()
{
	if (!GetWorld()) return;

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

	UndoSequenceCounter = 0;
}

void UML_WavePropagationSubsystem::CommitTurnRecord_Internal()
{
	if (!bHasActiveTurnRecord) return;

	FML_ActionUndoRecord A;
	A.Type = EML_UndoActionType::PlantWaves;
	A.Turn = CurrentTurnRecord;
	ActionUndoStack.Add(A);

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

	FML_TileUndoDelta Delta;
	Delta.Tile = Tile;
	Delta.OldType = Tile->GetCurrentType();
	Delta.bOldConsumedGrass = Tile->bConsumedGrass;
	Delta.bOldHasCollectible = Tile->HasCollectible();

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
	WinLoseSubsystem->CheckWinLose(); 
	
	bIsResolvingTiles = false;

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

	BeginTurnRecord_Internal(HitTile);

	ProcessNextWave();
}

// -------------------- Forward waves --------------------

void UML_WavePropagationSubsystem::RunWave()
{
	if (PendingChanges.Num() == 0)
	{
		EndTileResolved();
		return;
	}

	const int32 CurrentDistance = PendingChanges[0].DistanceFromOrigin;

	// Build the current "distance group" (wave step)
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
		// Tile update
		if (Change.Tile)
		{
			AML_Tile* Tile = Change.Tile;
			if (!IsValid(Tile)) continue;

			RecordTileBeforeChange(Tile, Change.DistanceFromOrigin);

			const UML_BiomeTileSet* TileSet = Tile->GetBoardSpawnerFromTile()->GetBiomeTileSet();
			if (!TileSet) return;

			if (!bUndoInProgress)
				Tile->UpdateClassAtRuntime(Change.TargetType, TileSet->GetClassFromTileType(Change.TargetType));
			else
				Tile->UpdateClassAtRuntime_Silent(Change.TargetType, TileSet->GetClassFromTileType(Change.TargetType));

			// Parasite bookkeeping
			if (Tile->GetCurrentType() == EML_TileType::Parasite && Tile->bConsumedGrass)
			{
				ParasitesThatAteGrass.Add(Tile);
				Tile->bConsumedGrass = false;
			}

			bCycleHasChanges = true;
		}
		// Collectible spawn
		else if (Change.CollectibleClass)
		{
			// Collectible wave - Deferred Spawn
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			
			// Create the actor WITHOUT the spawn (deferred)
			AML_Collectible* Collectible = GetWorld()->SpawnActorDeferred<AML_Collectible>(Change.CollectibleClass, FTransform(FRotator::ZeroRotator, Change.SpawnLocation), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (Collectible)
			{
				// Configure BEFORE the spawn
				Collectible->SetOwningTile(Change.Neighbor);
        
				// Finish spawning
				Collectible->FinishSpawning(FTransform(FRotator::ZeroRotator, Change.SpawnLocation));

				RecordSpawnedActor(Collectible, Change.DistanceFromOrigin);
        
				bCycleHasChanges = true;
			}
		}
	}

	// Schedule next distance step (intra-wave) or next priority (inter-wave)
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

	// End of all priorities
	if (CurrentWaveIndex >= DevSettings->WavesPriority.Num())
	{
		// Restart cycle if changes occurred (propagation chain reaction)
		if (bCycleHasChanges)
		{
			CurrentWaveIndex = 0;
			bCycleHasChanges = false;
			ProcessNextWave();
			return;
		}

		EndTileResolved();
		return;
	}

	UML_PropagationWaves* WaveLogic = DevSettings->WavesPriority[CurrentWaveIndex]->GetDefaultObject<UML_PropagationWaves>();
	if (!WaveLogic)
	{
		CurrentWaveIndex++;
		ProcessNextWave();
		return;
	}

	CurrentPriorityIndexForRecording = CurrentWaveIndex;
	CurrentWaveIndex++;

	PendingChanges.Empty();

	// Collectible wave uses a dedicated entry point
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
		// No changes in this wave → STOP immediately
		EndTileResolved();
		return;
	}

	PendingChanges.Sort([](const FML_WaveChange& A, const FML_WaveChange& B)
	{
		return A.DistanceFromOrigin < B.DistanceFromOrigin;
	});

	RunWave();
}

void UML_WavePropagationSubsystem::RecordTileForUndo(AML_Tile* Tile, int32 DistanceFromOrigin)
{
	RecordTileBeforeChange(Tile, DistanceFromOrigin);
}

// -------------------- Action recording (Move) --------------------

void UML_WavePropagationSubsystem::NotifyMoveCompleted(
	const FIntPoint& StartAxial,
	const FIntPoint& EndAxial,
	const TArray<FIntPoint>& AxialPath,
	const FVector& StartWorld,
	const FVector& EndWorld,
	const TArray<FIntPoint>& PickedCollectibleAxials)
{
	EnsureInitialized();

	UE_LOG(LogTemp, Warning, TEXT("[MOVE RECORD] Picked=%d Start=(%d,%d) End=(%d,%d)"), PickedCollectibleAxials.Num(), StartAxial.X, StartAxial.Y, EndAxial.X, EndAxial.Y);
	
	if (!PlayerController) return;
	if (bIsResolvingTiles || bIsUndoAnimating) return;

	if (AxialPath.Num() < 2) return;
	if (StartAxial == EndAxial) return;

	FML_ActionUndoRecord A;
	A.Type = EML_UndoActionType::Move;
	A.Move.StartAxial = StartAxial;
	A.Move.EndAxial = EndAxial;
	A.Move.AxialPath = AxialPath;
	A.Move.StartWorld = StartWorld;
	A.Move.EndWorld = EndWorld;
	A.Move.PickedCollectibleAxials = PickedCollectibleAxials;

	ActionUndoStack.Add(A);
}

// -------------------- Undo animated (Move + Plant/Waves) --------------------

bool UML_WavePropagationSubsystem::UndoLastAction_Animated()
{
	EnsureInitialized();
	if (!PlayerController || !DevSettings) return false;
	if (bIsResolvingTiles || bIsUndoAnimating) return false;
	if (ActionUndoStack.Num() == 0) return false;

	CancelAllWaveTimers();
	PlayerController->DisableInput(PlayerController);

	const FML_ActionUndoRecord Action = ActionUndoStack.Pop();

	// MOVE: play reversed path
	if (Action.Type == EML_UndoActionType::Move)
	{
		bIsUndoAnimating = true;

		TArray<FIntPoint> ReversePath = Action.Move.AxialPath;
		Algo::Reverse(ReversePath);

		PlayerController->StartMoveAlongAxialPathForUndo(ReversePath, Action.Move.PickedCollectibleAxials);
		return true;
	}

	// PLANT/WAVES: play undo-wave groups
	if (Action.Type == EML_UndoActionType::PlantWaves)
	{
		bIsUndoAnimating = true;
		ActiveUndoRecord = Action.Turn;

		// Restore energy at turn start
		PlayerController->CurrentEnergy = ActiveUndoRecord.EnergyBefore;

		PendingUndoTileDeltas = ActiveUndoRecord.TileDeltas;
		PendingUndoSpawnDeltas = ActiveUndoRecord.SpawnDeltas;

		// Sort for deterministic reverse playback (priority desc, distance desc, sequence desc)
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

		RunUndoWave();
		return true;
	}

	PlayerController->EnableInput(PlayerController);
	return false;
}

void UML_WavePropagationSubsystem::RunUndoWave()
{
	if (PendingUndoTileDeltas.Num() == 0 && PendingUndoSpawnDeltas.Num() == 0)
	{
		FinishUndoAnimation();
		return;
	}

	auto PeekKey = [](int32& OutPri, int32& OutDist,
		const bool bHasTile, const FML_TileUndoDelta& TD,
		const bool bHasSpawn, const FML_SpawnUndoDelta& SD)
	{
		if (bHasTile && !bHasSpawn) { OutPri = TD.PriorityIndex; OutDist = TD.DistanceFromOrigin; return; }
		if (!bHasTile && bHasSpawn) { OutPri = SD.PriorityIndex; OutDist = SD.DistanceFromOrigin; return; }

		// Both exist: pick the "largest" group key in our order
		if (TD.PriorityIndex != SD.PriorityIndex)
		{
			if (TD.PriorityIndex > SD.PriorityIndex) { OutPri = TD.PriorityIndex; OutDist = TD.DistanceFromOrigin; }
			else { OutPri = SD.PriorityIndex; OutDist = SD.DistanceFromOrigin; }
			return;
		}

		// Same priority -> larger distance first
		if (TD.DistanceFromOrigin >= SD.DistanceFromOrigin) { OutPri = TD.PriorityIndex; OutDist = TD.DistanceFromOrigin; }
		else { OutPri = SD.PriorityIndex; OutDist = SD.DistanceFromOrigin; }
	};

	int32 GroupPri = 0, GroupDist = 0;

	const bool bHasTile = PendingUndoTileDeltas.Num() > 0;
	const bool bHasSpawn = PendingUndoSpawnDeltas.Num() > 0;

	PeekKey(
		GroupPri, GroupDist,
		bHasTile, bHasTile ? PendingUndoTileDeltas[0] : FML_TileUndoDelta{},
		bHasSpawn, bHasSpawn ? PendingUndoSpawnDeltas[0] : FML_SpawnUndoDelta{}
	);

	ApplyUndoWaveGroup(GroupPri, GroupDist);

	if (PendingUndoTileDeltas.Num() == 0 && PendingUndoSpawnDeltas.Num() == 0)
	{
		FinishUndoAnimation();
		return;
	}

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
	// Destroy actors spawned during this group
	for (int32 i = 0; i < PendingUndoSpawnDeltas.Num(); )
	{
		const FML_SpawnUndoDelta& SD = PendingUndoSpawnDeltas[i];
		if (SD.PriorityIndex == PriorityIndex && SD.DistanceFromOrigin == DistanceFromOrigin)
		{
			if (AActor* A = SD.SpawnedActor.Get())
			{
				if (IsValid(A)) A->Destroy();
			}

			PendingUndoSpawnDeltas.RemoveAt(i);
			continue;
		}
		i++;
	}

	// Revert tiles in this group
	bUndoInProgress = true;
	for (int32 i = 0; i < PendingUndoTileDeltas.Num(); )
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

					// Important: if the tile should NOT have a collectible (pre-wave state),
					// we must also remove any collectible that may exist now (including those restored by undo-move).
					const bool bShouldHave = TD.bOldHasCollectible;
					const bool bHasNow = Tile->HasCollectible();

					if (!bShouldHave && bHasNow)
					{
						DestroyCollectibleActorOnTile(Tile);
						Tile->SetHasCollectible(false);
					}
					else
					{
						Tile->SetHasCollectible(bShouldHave);
					}

					Tile->bConsumedGrass = TD.bOldConsumedGrass;
				}
			}

			PendingUndoTileDeltas.RemoveAt(i);
			continue;
		}
		i++;
	}
	bUndoInProgress = false;
}

void UML_WavePropagationSubsystem::ScheduleNextUndoWave(float Delay)
{
	if (!GetWorld()) return;

	GetWorld()->GetTimerManager().SetTimer(
		IntraWaveTimerHandle,
		this,
		&UML_WavePropagationSubsystem::RunUndoWave,
		Delay,
		false
	);
}

void UML_WavePropagationSubsystem::FinishUndoAnimation()
{
	bIsUndoAnimating = false;
	bUndoInProgress = false;

	PendingUndoTileDeltas.Reset();
	PendingUndoSpawnDeltas.Reset();
	ActiveUndoRecord = FML_TurnUndoRecord{};

	if (PlayerController)
		PlayerController->EnableInput(PlayerController);

	PlayerController->CurrentEnergy ++;
}

void UML_WavePropagationSubsystem::NotifyUndoMoveFinished()
{
	// End of undo MOVE playback
	bIsUndoAnimating = false;

	if (PlayerController)
		PlayerController->EnableInput(PlayerController);
}

bool UML_WavePropagationSubsystem::RestoreCollectibleDuringUndoMove(const FIntPoint& Axial)
{
	EnsureInitialized();
	if (!GetWorld() || !PlayerController) return false;

	AML_PlayerCharacter* PC = Cast<AML_PlayerCharacter>(PlayerController->GetPawn());
	if (!IsValid(PC) || !IsValid(PC->CurrentTileOn)) return false;

	// Board is the owner of the player's current tile (single-board assumption).
	AML_BoardSpawner* Board = Cast<AML_BoardSpawner>(PC->CurrentTileOn->GetOwner());
	if (!IsValid(Board)) return false;

	UML_BiomeTileSet* TileSet = Board->GetBiomeTileSet();
	if (!IsValid(TileSet)) return false;

	// Collectible class comes from the current biome.
	TSubclassOf<AML_Collectible> CollectibleClass = TileSet->GetCollectibleClass();
	if (!*CollectibleClass) return false;

	// Resolve target tile from axial coordinate.
	const TMap<FIntPoint, AML_Tile*> GridMap = Board->GetGridMap();
	AML_Tile* const* TilePtr = GridMap.Find(Axial);
	if (!TilePtr || !IsValid(*TilePtr)) return false;

	AML_Tile* Tile = *TilePtr;

	// Prevent duplicates.
	if (Tile->HasCollectible())
	{
		return false;
	}

	// Spawn at tile world position (same rule as waves).
	const FVector SpawnLocation = Tile->GetActorLocation();

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AML_Collectible* SpawnedCollectible = GetWorld()->SpawnActorDeferred<AML_Collectible>(CollectibleClass, FTransform(FRotator::ZeroRotator, SpawnLocation), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (SpawnedCollectible)
	{
		// Configure BEFORE the spawn
		SpawnedCollectible->SetOwningTile(*TilePtr);
        
		// Finish spawning
		SpawnedCollectible->FinishSpawning(FTransform(FRotator::ZeroRotator, SpawnLocation));

		//RecordSpawnedActor(SpawnedCollectible, DistanceFromOrigin);
        
	}

	if (!IsValid(SpawnedCollectible))
	{
		return false;
	}

	// Wire tile <-> collectible.
	SpawnedCollectible->InitOwningAxial(Axial);
	Tile->CollectibleActor = SpawnedCollectible;
	Tile->SetHasCollectible(true);

	// Give energy back to the world (player loses 1).
	PlayerController->CurrentEnergy = FMath::Max(0, PlayerController->CurrentEnergy - 1);

	// Optional safety: avoid instant overlap in the same frame.
	if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(SpawnedCollectible->GetRootComponent()))
	{
		Prim->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		FTimerHandle Tmp;
		GetWorld()->GetTimerManager().SetTimer(Tmp, [Prim]()
		{
			if (IsValid(Prim))
			{
				Prim->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			}
		}, 0.05f, false);
	}

	return true;
}

void UML_WavePropagationSubsystem::DestroyCollectibleActorOnTile(AML_Tile* Tile)
{
	if (!IsValid(Tile)) return;

	// Prefer the tile reference (O(1), deterministic).
	if (AML_Collectible* C = Tile->CollectibleActor.Get())
	{
		if (IsValid(C))
		{
			C->Destroy();
		}
	}

	Tile->CollectibleActor = nullptr;
}