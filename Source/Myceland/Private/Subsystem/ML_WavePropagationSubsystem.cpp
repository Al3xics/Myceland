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

			RecordTileBeforeChange(Tile, Change.DistanceFromOrigin);

			const UML_BiomeTileSet* TileSet = Tile->GetBoardSpawnerFromTile()->GetBiomeTileSet();
			if (!TileSet) return;

			if (!bUndoInProgress)
				Tile->UpdateClassAtRuntime(Change.TargetType, TileSet->GetClassFromTileType(Change.TargetType));
			else
				Tile->UpdateClassAtRuntime_Silent(Change.TargetType, TileSet->GetClassFromTileType(Change.TargetType));

			// parasite bookkeeping
			if (Tile->GetCurrentType() == EML_TileType::Parasite && Tile->bConsumedGrass)
			{
				ParasitesThatAteGrass.Add(Tile);
				Tile->bConsumedGrass = false;
			}

			bCycleHasChanges = true;
		}
		else if (Change.CollectibleClass)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			AActor* Spawned = GetWorld()->SpawnActor<AActor>(
				Change.CollectibleClass,
				Change.SpawnLocation,
				FRotator::ZeroRotator,
				Params
			);

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

		PlayerController->CurrentEnergy = ActiveUndoRecord.EnergyBefore;

		PendingUndoTileDeltas = ActiveUndoRecord.TileDeltas;
		PendingUndoSpawnDeltas = ActiveUndoRecord.SpawnDeltas;

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

	// fallback
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

		// both exist
		if (TD.PriorityIndex != SD.PriorityIndex)
		{
			if (TD.PriorityIndex > SD.PriorityIndex) { OutPri = TD.PriorityIndex; OutDist = TD.DistanceFromOrigin; }
			else { OutPri = SD.PriorityIndex; OutDist = SD.DistanceFromOrigin; }
			return;
		}

		// same pri -> larger dist first
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
	// Destroy spawned actors in this group
	for (int32 i = 0; i < PendingUndoSpawnDeltas.Num(); )
	{
		const FML_SpawnUndoDelta& SD = PendingUndoSpawnDeltas[i];
		if (SD.PriorityIndex == PriorityIndex && SD.DistanceFromOrigin == DistanceFromOrigin)
		{
			if (AActor* A = SD.SpawnedActor.Get())
				if (IsValid(A)) A->Destroy();

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

					const bool bShouldHave = TD.bOldHasCollectible;
					const bool bHasNow = Tile->HasCollectible();

					if (!bShouldHave && bHasNow)
					{
						// Il ne devait PAS y avoir de collectible avant la propagation,
						// donc on enlève tout collectible présent maintenant, même s'il vient d'un Undo Move.
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

    // Source of truth: board = owner de la tile courante du joueur
    AML_BoardSpawner* Board = Cast<AML_BoardSpawner>(PC->CurrentTileOn->GetOwner());
    if (!IsValid(Board)) return false;

    UML_BiomeTileSet* TileSet = Board->GetBiomeTileSet();
    if (!IsValid(TileSet)) return false;

    // Source of truth: classe collectible vient du biome
    TSubclassOf<AML_Collectible> CollectibleClass = TileSet->GetCollectibleClass();
    if (!*CollectibleClass) return false;

    // Trouver la tuile cible via l'axial
    const TMap<FIntPoint, AML_Tile*> GridMap = Board->GetGridMap();
    AML_Tile* const* TilePtr = GridMap.Find(Axial);
    if (!TilePtr || !IsValid(*TilePtr)) return false;

    AML_Tile* Tile = *TilePtr;

    // Anti doublon : si la tile pense déjà en avoir un, on ne respawn pas
    if (Tile->HasCollectible())
    {
        // UE_LOG(LogTemp, Warning, TEXT("[UNDO] Skip respawn: HasCollectible already true at %d,%d"), Axial.X, Axial.Y);
        return false;
    }

    // Source of truth: spawn location = position monde de la tuile
    const FVector SpawnLocation = Tile->GetActorLocation();

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    // Spawn (comme RunWave)
    AActor* SpawnedActor = GetWorld()->SpawnActor<AActor>(
        CollectibleClass,
        SpawnLocation,
        FRotator::ZeroRotator,
        Params
    );

    AML_Collectible* SpawnedCollectible = Cast<AML_Collectible>(SpawnedActor);
    if (!IsValid(SpawnedCollectible))
    {
        if (IsValid(SpawnedActor))
        {
            // mauvais type -> nettoyage
            SpawnedActor->Destroy();
        }
        return false;
    }

    // IMPORTANT : owning axial
    SpawnedCollectible->InitOwningAxial(Axial);

    // IMPORTANT : monde dit maintenant "il y a un collectible"
    Tile->SetHasCollectible(true);

    // IMPORTANT : on rend l’énergie au monde
    PlayerController->CurrentEnergy = FMath::Max(0, PlayerController->CurrentEnergy - 1);

    // (Option anti re-pick instant si collision énorme)
    if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(SpawnedCollectible->GetRootComponent()))
    {
        Prim->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        FTimerHandle Tmp;
        GetWorld()->GetTimerManager().SetTimer(Tmp, [Prim]()
        {
            if (IsValid(Prim))
                Prim->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        }, 0.05f, false);
    }

    // UE_LOG(LogTemp, Log, TEXT("[UNDO] Respawn collectible at %d,%d"), Axial.X, Axial.Y);
    return true;
}

void UML_WavePropagationSubsystem::DestroyCollectibleActorOnTile(AML_Tile* Tile)
{
	if (!GetWorld() || !IsValid(Tile)) return;

	// Méthode simple: retrouver un collectible à la position de la tuile
	// (vu que tu spawns à Tile->GetActorLocation() sans offset)
	const FVector TileLoc = Tile->GetActorLocation();

	// Scan léger: itère collectibles existants (généralement peu nombreux)
	for (TActorIterator<AML_Collectible> It(GetWorld()); It; ++It)
	{
		AML_Collectible* C = *It;
		if (!IsValid(C)) continue;

		// 2D suffit souvent, garde tolérance
		const float Dist2D = FVector::DistSquared2D(C->GetActorLocation(), TileLoc);
		if (Dist2D <= FMath::Square(5.f)) // tolérance
		{
			C->Destroy();
			return;
		}

		// Encore plus robuste si OwningAxial est set partout:
		// if (C->GetOwningAxial() == Tile->GetAxialCoord()) { C->Destroy(); return; }
	}
}