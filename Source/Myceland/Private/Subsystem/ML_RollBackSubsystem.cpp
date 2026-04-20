// Copyright Myceland Team, All Rights Reserved.

#include "Subsystem/ML_RollBackSubsystem.h"

#include "Algo/Reverse.h"
#include "Collectible/ML_Collectible.h"
#include "Components/PrimitiveComponent.h"
#include "Data Asset/ML_BiomeTileSet.h"
#include "Developer Settings/ML_MycelandDeveloperSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Player/ML_PlayerCharacter.h"
#include "Player/ML_PlayerController.h"
#include "Subsystem/ML_WavePropagationSubsystem.h"
#include "Tiles/ML_BoardSpawner.h"
#include "Tiles/ML_Tile.h"

void UML_RollBackSubsystem::EnsureInitialized()
{
	if (!GetWorld()) return;

	PlayerController = Cast<AML_PlayerController>(GetWorld()->GetFirstPlayerController());
	DevSettings = UML_MycelandDeveloperSettings::GetMycelandDeveloperSettings();

	ensure(PlayerController);
}

TArray<FML_ActionUndoRecord>* UML_RollBackSubsystem::GetCurrentBoardStack()
{
	if (!PlayerController) return nullptr;

	AML_PlayerCharacter* PC = Cast<AML_PlayerCharacter>(PlayerController->GetPawn());
	if (!PC || !PC->CurrentTileOn) return nullptr;

	AML_BoardSpawner* Board = PC->CurrentTileOn->GetBoardSpawnerFromTile();
	if (!IsValid(Board)) return nullptr;

	return &BoardUndoStacks.FindOrAdd(Board);
}

void UML_RollBackSubsystem::BeginTurnRecord(AML_Tile* OriginTile)
{
	if (!PlayerController) return;

	AML_PlayerCharacter* PC = Cast<AML_PlayerCharacter>(PlayerController->GetPawn());
	if (!PC || !PC->CurrentTileOn) return;

	bHasActiveTurnRecord = true;
	CurrentTurnRecord = FML_TurnUndoRecord{};
	CurrentTurnRecord.EnergyBefore = PlayerController->EnergyComponent->GetCurrentEnergy() + 1;
	CurrentTurnRecord.PlayerAxialBefore = PC->CurrentTileOn->GetAxialCoord();
	CurrentTurnRecord.PlayerWorldBefore = PC->GetActorLocation();
	CurrentTurnRecord.OriginTile = OriginTile;

	UndoSequenceCounter = 0;
}

void UML_RollBackSubsystem::CommitTurnRecord()
{
	if (!bHasActiveTurnRecord) return;

	FML_ActionUndoRecord Action;
	Action.Type = EML_UndoActionType::PlantWaves;
	Action.Turn = CurrentTurnRecord;

	if (TArray<FML_ActionUndoRecord>* Stack = GetCurrentBoardStack())
	{
		Stack->Add(Action);
	}

	bHasActiveTurnRecord = false;
	CurrentTurnRecord = FML_TurnUndoRecord{};
}

void UML_RollBackSubsystem::DiscardTurnRecord()
{
	bHasActiveTurnRecord = false;
	CurrentTurnRecord = FML_TurnUndoRecord{};
}

void UML_RollBackSubsystem::RecordTileForUndo(AML_Tile* Tile, int32 DistanceFromOrigin, int32 PriorityIndex)
{
	if (!bHasActiveTurnRecord || !IsValid(Tile)) return;

	FML_TileUndoDelta Delta;
	Delta.Tile = Tile;
	Delta.OldType = Tile->GetCurrentType();
	Delta.bOldConsumedGrass = Tile->bConsumedGrass;
	Delta.bOldHasCollectible = Tile->HasCollectible();
	Delta.PriorityIndex = PriorityIndex;
	Delta.DistanceFromOrigin = DistanceFromOrigin;
	Delta.Sequence = UndoSequenceCounter++;

	CurrentTurnRecord.TileDeltas.Add(Delta);
}

void UML_RollBackSubsystem::RecordSpawnedActor(AActor* Spawned, int32 DistanceFromOrigin, int32 PriorityIndex)
{
	if (!bHasActiveTurnRecord || !IsValid(Spawned)) return;

	FML_SpawnUndoDelta Delta;
	Delta.SpawnedActor = Spawned;
	Delta.PriorityIndex = PriorityIndex;
	Delta.DistanceFromOrigin = DistanceFromOrigin;
	Delta.Sequence = UndoSequenceCounter++;

	CurrentTurnRecord.SpawnDeltas.Add(Delta);
}

bool UML_RollBackSubsystem::CanUndo() const
{
	if (bIsUndoAnimating) return false;
	if (!PlayerController) return false;

	const UML_WavePropagationSubsystem* WaveSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UML_WavePropagationSubsystem>() : nullptr;
	if (WaveSubsystem && WaveSubsystem->IsResolvingTiles()) return false;

	AML_PlayerCharacter* PC = Cast<AML_PlayerCharacter>(PlayerController->GetPawn());
	if (!PC || !PC->CurrentTileOn) return false;

	AML_BoardSpawner* Board = PC->CurrentTileOn->GetBoardSpawnerFromTile();
	if (!IsValid(Board)) return false;

	const TArray<FML_ActionUndoRecord>* Stack = BoardUndoStacks.Find(Board);
	return Stack && Stack->Num() > 0;
}

void UML_RollBackSubsystem::RunUndoWave()
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

		if (TD.PriorityIndex != SD.PriorityIndex)
		{
			if (TD.PriorityIndex > SD.PriorityIndex) { OutPri = TD.PriorityIndex; OutDist = TD.DistanceFromOrigin; }
			else { OutPri = SD.PriorityIndex; OutDist = SD.DistanceFromOrigin; }
			return;
		}

		if (TD.DistanceFromOrigin >= SD.DistanceFromOrigin) { OutPri = TD.PriorityIndex; OutDist = TD.DistanceFromOrigin; }
		else { OutPri = SD.PriorityIndex; OutDist = SD.DistanceFromOrigin; }
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

	if (PendingUndoTileDeltas.Num() == 0 && PendingUndoSpawnDeltas.Num() == 0)
	{
		FinishUndoAnimation();
		return;
	}

	int32 NextPri = 0;
	int32 NextDist = 0;

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

void UML_RollBackSubsystem::ApplyUndoWaveGroup(int32 PriorityIndex, int32 DistanceFromOrigin)
{
	for (int32 i = 0; i < PendingUndoSpawnDeltas.Num(); )
	{
		const FML_SpawnUndoDelta& SpawnDelta = PendingUndoSpawnDeltas[i];
		if (SpawnDelta.PriorityIndex == PriorityIndex && SpawnDelta.DistanceFromOrigin == DistanceFromOrigin)
		{
			if (AActor* SpawnedActor = SpawnDelta.SpawnedActor.Get())
			{
				if (IsValid(SpawnedActor))
				{
					SpawnedActor->Destroy();
				}
			}

			PendingUndoSpawnDeltas.RemoveAt(i);
			continue;
		}

		++i;
	}

	bUndoInProgress = true;

	for (int32 i = 0; i < PendingUndoTileDeltas.Num(); )
	{
		const FML_TileUndoDelta& TileDelta = PendingUndoTileDeltas[i];
		if (TileDelta.PriorityIndex == PriorityIndex && TileDelta.DistanceFromOrigin == DistanceFromOrigin)
		{
			AML_Tile* Tile = TileDelta.Tile.Get();
			if (IsValid(Tile))
			{
				const UML_BiomeTileSet* TileSet = Tile->GetBoardSpawnerFromTile()->GetBiomeTileSet();
				if (TileSet)
				{
					const EML_TileType PreviousType = Tile->GetCurrentType();
					const EML_TileType NewType = TileDelta.OldType;

					Tile->UpdateClassAtRuntime_Silent(NewType, TileSet->GetClassFromTileType(NewType));

					if (PreviousType != NewType)
					{
						Tile->OnTileTypeChanged(PreviousType, NewType);
					}

					const bool bShouldHaveCollectible = TileDelta.bOldHasCollectible;
					const bool bHasCollectibleNow = Tile->HasCollectible();

					if (!bShouldHaveCollectible && bHasCollectibleNow)
					{
						DestroyCollectibleActorOnTile(Tile);
						Tile->SetHasCollectible(false);
					}
					else
					{
						Tile->SetHasCollectible(bShouldHaveCollectible);
					}

					Tile->bConsumedGrass = TileDelta.bOldConsumedGrass;
				}
			}

			PendingUndoTileDeltas.RemoveAt(i);
			continue;
		}

		++i;
	}

	bUndoInProgress = false;
}

void UML_RollBackSubsystem::ScheduleNextUndoWave(float Delay)
{
	if (!GetWorld()) return;

	GetWorld()->GetTimerManager().SetTimer(
		UndoWaveTimerHandle,
		this,
		&UML_RollBackSubsystem::RunUndoWave,
		Delay,
		false
	);
}

void UML_RollBackSubsystem::FinishUndoAnimation()
{
	bIsUndoAnimating = false;
	bUndoInProgress = false;

	PendingUndoTileDeltas.Reset();
	PendingUndoSpawnDeltas.Reset();
	ActiveUndoRecord = FML_TurnUndoRecord{};

	if (!bIsResetAllAnimating)
	{
		ClearTimeDilation();
		OnUndoAnimating.Broadcast(bIsUndoAnimating);
	}

	ContinueResetAllIfNeeded();
}

bool UML_RollBackSubsystem::RestoreCollectibleDuringUndoMove(const FIntPoint& Axial)
{
	EnsureInitialized();
	if (!GetWorld() || !PlayerController) return false;

	AML_PlayerCharacter* PC = Cast<AML_PlayerCharacter>(PlayerController->GetPawn());
	if (!IsValid(PC) || !IsValid(PC->CurrentTileOn)) return false;

	AML_BoardSpawner* Board = Cast<AML_BoardSpawner>(PC->CurrentTileOn->GetOwner());
	if (!IsValid(Board)) return false;

	UML_BiomeTileSet* TileSet = Board->GetBiomeTileSet();
	if (!IsValid(TileSet)) return false;

	TSubclassOf<AML_Collectible> CollectibleClass = TileSet->GetCollectibleClass();
	if (!*CollectibleClass) return false;

	const TMap<FIntPoint, AML_Tile*> GridMap = Board->GetGridMap();
	AML_Tile* const* TilePtr = GridMap.Find(Axial);
	if (!TilePtr || !IsValid(*TilePtr)) return false;

	AML_Tile* Tile = *TilePtr;
	if (Tile->HasCollectible()) return false;

	const FVector SpawnLocation = Tile->GetActorLocation();

	AML_Collectible* SpawnedCollectible = GetWorld()->SpawnActorDeferred<AML_Collectible>(
		CollectibleClass,
		FTransform(FRotator::ZeroRotator, SpawnLocation),
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn
	);

	if (SpawnedCollectible)
	{
		SpawnedCollectible->SetOwningTile(*TilePtr);
		SpawnedCollectible->FinishSpawning(FTransform(FRotator::ZeroRotator, SpawnLocation));
	}

	if (!IsValid(SpawnedCollectible)) return false;

	SpawnedCollectible->InitOwningAxial(Axial);
	Tile->CollectibleActor = SpawnedCollectible;
	Tile->SetHasCollectible(true);

	PlayerController->EnergyComponent->AddEnergy(-1);

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

void UML_RollBackSubsystem::DestroyCollectibleActorOnTile(AML_Tile* Tile)
{
	if (!IsValid(Tile)) return;

	if (AML_Collectible* Collectible = Tile->CollectibleActor.Get())
	{
		if (IsValid(Collectible))
		{
			Collectible->DestroyCollectible();
		}
	}
}

void UML_RollBackSubsystem::NotifyMoveCompleted(
	const FIntPoint& StartAxial,
	const FIntPoint& EndAxial,
	const TArray<FIntPoint>& AxialPath,
	const FVector& StartWorld,
	const FVector& EndWorld,
	const TArray<FIntPoint>& PickedCollectibleAxials)
{
	EnsureInitialized();

	if (!PlayerController) return;
	if (bIsUndoAnimating || bIsResetAllAnimating) return;

	const UML_WavePropagationSubsystem* WaveSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UML_WavePropagationSubsystem>() : nullptr;
	if (WaveSubsystem && WaveSubsystem->IsResolvingTiles()) return;

	if (AxialPath.Num() < 2) return;
	if (StartAxial == EndAxial) return;

	TArray<FML_ActionUndoRecord>* Stack = GetCurrentBoardStack();
	if (!Stack) return;

	FML_ActionUndoRecord Action;
	Action.Type = EML_UndoActionType::Move;
	Action.Move.StartAxial = StartAxial;
	Action.Move.EndAxial = EndAxial;
	Action.Move.AxialPath = AxialPath;
	Action.Move.StartWorld = StartWorld;
	Action.Move.EndWorld = EndWorld;
	Action.Move.PickedCollectibleAxials = PickedCollectibleAxials;

	Stack->Add(Action);
}

void UML_RollBackSubsystem::ApplyUndoTimeDilation()
{
	if (!GetWorld() || bUndoTimeDilationApplied) return;

	UGameplayStatics::SetGlobalTimeDilation(GetWorld(), UML_MycelandDeveloperSettings::GetMycelandDeveloperSettings()->UndoSpeed);
	bUndoTimeDilationApplied = true;
}

void UML_RollBackSubsystem::ApplyResetTimeDilation()
{
	if (!GetWorld() || bUndoTimeDilationApplied) return;

	UGameplayStatics::SetGlobalTimeDilation(GetWorld(), UML_MycelandDeveloperSettings::GetMycelandDeveloperSettings()->ResetSpeed);
	bUndoTimeDilationApplied = true;
}

void UML_RollBackSubsystem::ClearTimeDilation()
{
	if (!GetWorld() || !bUndoTimeDilationApplied) return;

	UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 1.0f);
	bUndoTimeDilationApplied = false;
}

bool UML_RollBackSubsystem::ResetAllActions_Animated()
{
	EnsureInitialized();
	if (!PlayerController || !DevSettings) return false;
	if (bIsUndoAnimating || bIsResetAllAnimating) return false;

	UML_WavePropagationSubsystem* WaveSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UML_WavePropagationSubsystem>() : nullptr;
	if (!WaveSubsystem || WaveSubsystem->IsResolvingTiles()) return false;

	AML_PlayerCharacter* PC = Cast<AML_PlayerCharacter>(PlayerController->GetPawn());
	if (!PC || !PC->CurrentTileOn) return false;

	TArray<FML_ActionUndoRecord>* Stack = GetCurrentBoardStack();
	if (!Stack || Stack->Num() == 0) return false;

	WaveSubsystem->CancelAllWaveTimers();
	PlayerController->DisableInput(PlayerController);
	ApplyResetTimeDilation();

	bIsResetAllAnimating = true;
	OnResetAnimating.Broadcast(bIsResetAllAnimating);

	const bool bStarted = UndoLastAction_Animated();
	if (!bStarted)
	{
		bIsResetAllAnimating = false;
		ClearTimeDilation();
		OnResetAnimating.Broadcast(bIsResetAllAnimating);
		PlayerController->EnableInput(PlayerController);
		return false;
	}

	return true;
}

void UML_RollBackSubsystem::StartNextResetUndoStep()
{
	if (!bIsResetAllAnimating || bIsUndoAnimating) return;

	UML_WavePropagationSubsystem* WaveSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UML_WavePropagationSubsystem>() : nullptr;
	if (WaveSubsystem && WaveSubsystem->IsResolvingTiles()) return;

	TArray<FML_ActionUndoRecord>* Stack = GetCurrentBoardStack();
	if (!Stack || Stack->Num() == 0)
	{
		bIsResetAllAnimating = false;
		ClearTimeDilation();
		OnResetAnimating.Broadcast(bIsResetAllAnimating);

		if (PlayerController)
		{
			PlayerController->EnableInput(PlayerController);
		}
		return;
	}

	const bool bStarted = UndoLastAction_Animated();
	if (!bStarted)
	{
		bIsResetAllAnimating = false;
		ClearTimeDilation();
		OnResetAnimating.Broadcast(bIsResetAllAnimating);

		if (PlayerController)
		{
			PlayerController->EnableInput(PlayerController);
		}
	}
}

void UML_RollBackSubsystem::ContinueResetAllIfNeeded()
{
	if (!bIsResetAllAnimating)
	{
		if (PlayerController)
		{
			PlayerController->EnableInput(PlayerController);
		}
		return;
	}

	TArray<FML_ActionUndoRecord>* Stack = GetCurrentBoardStack();
	if (!Stack || Stack->Num() == 0 || !GetWorld())
	{
		bIsResetAllAnimating = false;
		ClearTimeDilation();
		OnResetAnimating.Broadcast(bIsResetAllAnimating);

		if (PlayerController)
		{
			PlayerController->EnableInput(PlayerController);
		}
		return;
	}

	GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UML_RollBackSubsystem::StartNextResetUndoStep);
}

bool UML_RollBackSubsystem::UndoLastAction_Animated()
{
	EnsureInitialized();
	if (!PlayerController || !DevSettings) return false;
	if (bIsUndoAnimating) return false;

	UML_WavePropagationSubsystem* WaveSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UML_WavePropagationSubsystem>() : nullptr;
	if (!WaveSubsystem || WaveSubsystem->IsResolvingTiles()) return false;

	AML_PlayerCharacter* PC = Cast<AML_PlayerCharacter>(PlayerController->GetPawn());
	if (!PC || !PC->CurrentTileOn) return false;

	TArray<FML_ActionUndoRecord>* Stack = GetCurrentBoardStack();
	if (!Stack || Stack->Num() == 0) return false;

	WaveSubsystem->CancelAllWaveTimers();
	PlayerController->DisableInput(PlayerController);
	ApplyUndoTimeDilation();

	const FML_ActionUndoRecord Action = Stack->Pop();

	if (Action.Type == EML_UndoActionType::Move)
	{
		bIsUndoAnimating = true;
		if (!bIsResetAllAnimating)
		{
			OnUndoAnimating.Broadcast(bIsUndoAnimating);
		}

		TArray<FIntPoint> ReversePath = Action.Move.AxialPath;
		Algo::Reverse(ReversePath);

		PlayerController->StartMoveAlongAxialPathForUndo(ReversePath, Action.Move.PickedCollectibleAxials);
		return true;
	}

	if (Action.Type == EML_UndoActionType::PlantWaves)
	{
		bIsUndoAnimating = true;
		if (!bIsResetAllAnimating)
		{
			OnUndoAnimating.Broadcast(bIsUndoAnimating);
		}

		ActiveUndoRecord = Action.Turn;
		PlayerController->EnergyComponent->SetCurrentEnergy(ActiveUndoRecord.EnergyBefore);

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

	if (bIsResetAllAnimating)
	{
		ContinueResetAllIfNeeded();
	}
	else
	{
		ClearTimeDilation();
		if (PlayerController)
		{
			PlayerController->EnableInput(PlayerController);
		}
	}

	return false;
}

bool UML_RollBackSubsystem::ResetAllActions_ExcludingMoves_Animated()
{
	EnsureInitialized();
	if (!PlayerController || !DevSettings) return false;
	if (bIsUndoAnimating || bIsResetAllAnimating) return false;

	UML_WavePropagationSubsystem* WaveSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UML_WavePropagationSubsystem>() : nullptr;
	if (!WaveSubsystem || WaveSubsystem->IsResolvingTiles()) return false;

	TArray<FML_ActionUndoRecord>* Stack = GetCurrentBoardStack();
	if (!Stack) return false;

	Stack->RemoveAll([](const FML_ActionUndoRecord& Action)
	{
		return Action.Type == EML_UndoActionType::Move;
	});

	if (Stack->Num() == 0) return false;

	WaveSubsystem->CancelAllWaveTimers();
	PlayerController->DisableInput(PlayerController);
	ApplyResetTimeDilation();

	bIsResetAllAnimating = true;
	OnResetAnimating.Broadcast(bIsResetAllAnimating);

	const bool bStarted = UndoLastAction_Animated();
	if (!bStarted)
	{
		bIsResetAllAnimating = false;
		ClearTimeDilation();
		OnResetAnimating.Broadcast(bIsResetAllAnimating);
		PlayerController->EnableInput(PlayerController);
		return false;
	}

	return true;
}

void UML_RollBackSubsystem::ResetRuntimeState()
{
	bIsResetAllAnimating = false;
	bIsUndoAnimating = false;
	bUndoInProgress = false;

	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(UndoWaveTimerHandle);
	}

	PendingUndoTileDeltas.Reset();
	PendingUndoSpawnDeltas.Reset();
	ActiveUndoRecord = FML_TurnUndoRecord{};
	ClearTimeDilation();
}

void UML_RollBackSubsystem::ResetAllActions_ExcludingMoves_Instant(AML_BoardSpawner* Board)
{
	EnsureInitialized();
	if (!PlayerController || !IsValid(Board)) return;

	UML_WavePropagationSubsystem* WaveSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UML_WavePropagationSubsystem>() : nullptr;
	if (!WaveSubsystem) return;

	if (WaveSubsystem->IsResolvingTiles() && bHasActiveTurnRecord)
	{
		CommitTurnRecord();
	}
	else
	{
		DiscardTurnRecord();
	}

	TArray<FML_ActionUndoRecord>* Stack = BoardUndoStacks.Find(Board);
	if (!Stack)
	{
		WaveSubsystem->AbortPropagationRuntime();
		ResetRuntimeState();
		if (PlayerController)
		{
			PlayerController->EnableInput(PlayerController);
		}
		return;
	}

	Stack->RemoveAll([](const FML_ActionUndoRecord& Action)
	{
		return Action.Type == EML_UndoActionType::Move;
	});

	for (int32 i = 0; i < Stack->Num(); ++i)
	{
		if ((*Stack)[i].Type == EML_UndoActionType::PlantWaves)
		{
			PlayerController->EnergyComponent->SetCurrentEnergy((*Stack)[i].Turn.EnergyBefore);
			break;
		}
	}

	for (int32 i = Stack->Num() - 1; i >= 0; --i)
	{
		const FML_ActionUndoRecord& Action = (*Stack)[i];
		if (Action.Type != EML_UndoActionType::PlantWaves) continue;

		const FML_TurnUndoRecord& Turn = Action.Turn;

		for (const FML_SpawnUndoDelta& SpawnDelta : Turn.SpawnDeltas)
		{
			if (AActor* SpawnedActor = SpawnDelta.SpawnedActor.Get())
			{
				if (IsValid(SpawnedActor))
				{
					SpawnedActor->Destroy();
				}
			}
		}

		TSet<AML_Tile*> AlreadyReverted;
		for (const FML_TileUndoDelta& TileDelta : Turn.TileDeltas)
		{
			AML_Tile* Tile = TileDelta.Tile.Get();
			if (!IsValid(Tile) || AlreadyReverted.Contains(Tile)) continue;

			AlreadyReverted.Add(Tile);

			const UML_BiomeTileSet* TileSet = Tile->GetBoardSpawnerFromTile()->GetBiomeTileSet();
			if (!TileSet) continue;

			const EML_TileType PreviousType = Tile->GetCurrentType();
			const EML_TileType NewType = TileDelta.OldType;

			Tile->UpdateClassAtRuntime_Silent(NewType, TileSet->GetClassFromTileType(NewType));

			if (PreviousType != NewType)
			{
				Tile->OnTileTypeChanged(PreviousType, NewType);
			}

			const bool bShouldHaveCollectible = TileDelta.bOldHasCollectible;
			const bool bHasCollectibleNow = Tile->HasCollectible();

			if (!bShouldHaveCollectible && bHasCollectibleNow)
			{
				DestroyCollectibleActorOnTile(Tile);
				Tile->SetHasCollectible(false);
			}
			else
			{
				Tile->SetHasCollectible(bShouldHaveCollectible);
			}

			Tile->bConsumedGrass = TileDelta.bOldConsumedGrass;
		}
	}

	BoardUndoStacks.Remove(Board);
	WaveSubsystem->AbortPropagationRuntime();
	ResetRuntimeState();

	if (PlayerController)
	{
		PlayerController->EnableInput(PlayerController);
	}
}
