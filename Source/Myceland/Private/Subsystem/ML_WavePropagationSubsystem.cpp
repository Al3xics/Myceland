// Copyright Myceland Team, All Rights Reserved.

#include "Subsystem/ML_WavePropagationSubsystem.h"

#include "Audio/ML_FMODEvents.h"
#include "Core/ML_TileTypeTraits.h"
#include "Developer Settings/ML_MycelandDeveloperSettings.h"
#include "Player/ML_PlayerController.h"
#include "Collectible/ML_Collectible.h"
#include "Subsystem/ML_RollBackSubsystem.h"
#include "Subsystem/ML_SoundSubsystem.h"
#include "Subsystem/ML_WinLoseSubsystem.h"
#include "Tiles/ML_BoardSpawner.h"
#include "Tiles/ML_Tile.h"
#include "Data Asset/ML_BiomeTileSet.h"
#include "Waves/ML_PropagationWaves.h"
#include "Waves/ChildWaves/ML_WaveCollectible.h"

void UML_WavePropagationSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Shared budget for both slicers: one deadline per frame.
	const double Deadline = MakeSliceDeadline();

	if (bTouchRingInProgress)
		ProcessTouchSlice(Deadline);

	if (bRingInProgress)
		ProcessRingSlice(Deadline);
}

bool UML_WavePropagationSubsystem::IsTickable() const
{
	return bRingInProgress || bTouchRingInProgress;
}

TStatId UML_WavePropagationSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UML_WavePropagationSubsystem, STATGROUP_Tickables);
}

double UML_WavePropagationSubsystem::MakeSliceDeadline() const
{
	const float BudgetMs = DevSettings ? DevSettings->WavePropagationFrameBudgetMs : 2.f;
	return FPlatformTime::Seconds() + static_cast<double>(BudgetMs) / 1000.0;
}

void UML_WavePropagationSubsystem::EnsureInitialized()
{
	if (!GetWorld()) return;

	WinLoseSubsystem = GetWorld()->GetSubsystem<UML_WinLoseSubsystem>();
	PlayerController = Cast<AML_PlayerController>(GetWorld()->GetFirstPlayerController());
	DevSettings = UML_MycelandDeveloperSettings::GetMycelandDeveloperSettings();
	RollBackSubsystem = GetWorld()->GetSubsystem<UML_RollBackSubsystem>();
	
	ensure(PlayerController && WinLoseSubsystem && RollBackSubsystem);

	if (RollBackSubsystem && !bRollbackDelegatesBound)
	{
		RollBackSubsystem->OnUndoAnimating.AddDynamic(this, &UML_WavePropagationSubsystem::HandleRollbackUndoAnimating);
		RollBackSubsystem->OnResetAnimating.AddDynamic(this, &UML_WavePropagationSubsystem::HandleRollbackResetAnimating);
		bRollbackDelegatesBound = true;
	}
}

void UML_WavePropagationSubsystem::CancelAllWaveTimers()
{
	if (!GetWorld()) return;

	FTimerManager& TM = GetWorld()->GetTimerManager();
	TM.ClearTimer(IntraWaveTimerHandle);
	TM.ClearTimer(InterWaveTimerHandle);
	TM.ClearTimer(TouchTimerHandle);
}

void UML_WavePropagationSubsystem::EndTileResolved()
{
	// A wave can resolve on a board the player is not standing on (e.g. hub tile
	// changes after a win). Win/lose and goal-path logic only concern the player's
	// board, so skip both when the wave demonstrably ran elsewhere. If either board
	// is unknown (first wave before CheckWinLose resolved it), keep the old behavior.
	AML_BoardSpawner* WaveBoard = IsValid(CurrentOriginTile) ? CurrentOriginTile->GetBoardSpawnerFromTile() : nullptr;
	const bool bWaveOnOtherBoard = IsValid(WaveBoard)
		&& IsValid(WinLoseSubsystem->CurrentBoardSpawner)
		&& WaveBoard != WinLoseSubsystem->CurrentBoardSpawner;

	if (!bWaveOnOtherBoard)
	{
		WinLoseSubsystem->CheckWinLose();

		// If the whole action changed nothing on the board, goal connectivity can't
		// have changed either: skip the (BFS-heavy) goal-path recompute.
		if (bAnyChangeThisAction)
			WinLoseSubsystem->TriggerFindConnectedGoalCheck();
	}

	if (bPlayAvatarSurpriseVocalThisAction && TotalReactionTileCount > 0 && !WinLoseSubsystem->bIsPlayerDead)
	{
		if (UML_SoundSubsystem* SoundSubsystem = UML_SoundSubsystem::Get(this))
		{
			if (const AML_Tile* PlayerTile = WinLoseSubsystem->GetPlayerCurrentTile())
			{
				const TCHAR* SurpriseEvent = MLFMODEvents::AvatarSurpriseLow;
				if (TotalReactionTileCount >= 6)
				{
					SurpriseEvent = MLFMODEvents::AvatarSurpriseHigh;
				}
				else if (TotalReactionTileCount >= 3)
				{
					SurpriseEvent = MLFMODEvents::AvatarSurpriseMedium;
				}

				const FTransform PlayerTransform(FRotator::ZeroRotator, PlayerTile->GetActorLocation());
				SoundSubsystem->StartSoundAtLocationByPath(SurpriseEvent, PlayerTransform);
			}
		}
	}
	TotalReactionTileCount = 0;
	bPlayAvatarSurpriseVocalThisAction = true;

	bIsResolvingTiles = false;

	if (RollBackSubsystem)
		RollBackSubsystem->CommitTurnRecord();

	if (PlayerController)
	{
		if (PlayerController->TransitionComponent)
			PlayerController->TransitionComponent->OnBoardActivityStateChanged.Broadcast(false);
		PlayerController->EnableInput(PlayerController);
	}
}

void UML_WavePropagationSubsystem::BeginTileResolved(AML_Tile* HitTile)
{
	BeginTileResolvedInternal(HitTile, true);
}

void UML_WavePropagationSubsystem::BeginTileResolvedWithoutAvatarSurprise(AML_Tile* HitTile)
{
	BeginTileResolvedInternal(HitTile, false);
}

void UML_WavePropagationSubsystem::BeginTileResolvedInternal(AML_Tile* HitTile, bool bPlayAvatarSurpriseVocal)
{
	if (!HitTile || bIsResolvingTiles) return;
	if (!PlayerController || !RollBackSubsystem) EnsureInitialized();
	if (!PlayerController || !DevSettings || !RollBackSubsystem) return;

	bIsResolvingTiles = true;
	PlayerController->DisableInput(PlayerController);
	if (PlayerController->TransitionComponent)
		PlayerController->TransitionComponent->OnBoardActivityStateChanged.Broadcast(true);

	CurrentOriginTile = HitTile;
	CurrentWaveIndex = 0;
	bCycleHasChanges = false;
	bAnyChangeThisAction = false;
	CurrentNatureReactionCount = 0;
	CurrentParasiteReactionCount = 0;
	CurrentWaterReactionCount = 0;
	TotalReactionTileCount = 0;
	bPlayAvatarSurpriseVocalThisAction = bPlayAvatarSurpriseVocal;

	ParasitesThatAteGrass.Empty();
	PendingChanges.Empty();
	PendingChangesIndex = 0;
	bRingInProgress = false;
	bTouchRingInProgress = false;

	if (RollBackSubsystem)
		RollBackSubsystem->BeginTurnRecord(HitTile);

	BuildTouchQueue(HitTile);
	FireNextTouchRing();

	ProcessNextWave();
}

// -------------------- Forward waves --------------------

void UML_WavePropagationSubsystem::RunWave()
{
	if (PendingChangesIndex >= PendingChanges.Num())
	{
		EndTileResolved();
		return;
	}

	// Start the next "distance group" (wave step). The changes are applied under a
	// per-frame CPU budget: whatever doesn't fit continues in Tick on the next frames.
	CurrentRingDistance = PendingChanges[PendingChangesIndex].DistanceFromOrigin;
	bRingInProgress = true;
	ProcessRingSlice(MakeSliceDeadline());
}

void UML_WavePropagationSubsystem::ProcessRingSlice(const double Deadline)
{
	while (PendingChangesIndex < PendingChanges.Num()
		&& PendingChanges[PendingChangesIndex].DistanceFromOrigin == CurrentRingDistance)
	{
		ApplyChange(PendingChanges[PendingChangesIndex]);
		PendingChangesIndex++;

		// A change can abort the whole propagation (e.g. player death): stop cleanly.
		if (!bIsResolvingTiles)
		{
			bRingInProgress = false;
			return;
		}

		const bool bRingHasMore =
			PendingChangesIndex < PendingChanges.Num()
			&& PendingChanges[PendingChangesIndex].DistanceFromOrigin == CurrentRingDistance;

		// Budget exhausted: resume this ring next frame (Tick).
		if (bRingHasMore && FPlatformTime::Seconds() >= Deadline)
			return;
	}

	bRingInProgress = false;
	FinishRing();
}

void UML_WavePropagationSubsystem::ApplyChange(const FML_WaveChange& Change)
{
	// Tile update
	if (Change.Tile)
	{
		AML_Tile* Tile = Change.Tile;
		if (!IsValid(Tile)) return;
		const EML_TileType OldType = Tile->GetCurrentType();

		if (RollBackSubsystem)
			RollBackSubsystem->RecordTileForUndo(Tile, Change.DistanceFromOrigin, CurrentPriorityIndexForRecording);

		const UML_BiomeTileSet* TileSet = Tile->GetBoardSpawnerFromTile()->GetBiomeTileSet();
		if (!TileSet) return;

		if (!RollBackSubsystem || !RollBackSubsystem->IsUndoInProgress())
			Tile->UpdateClassAtRuntime(Change.TargetType, TileSet->GetClassFromTileType(Change.TargetType));
		else
			Tile->UpdateClassAtRuntime_Silent(Change.TargetType, TileSet->GetClassFromTileType(Change.TargetType));

		const bool bIsUndo = RollBackSubsystem && RollBackSubsystem->IsUndoInProgress();
		const bool bTileChanged = OldType != Change.TargetType;
		if (bTileChanged && !bIsUndo)
		{
			if (Change.TargetType == EML_TileType::Grass && Change.DistanceFromOrigin > 0)
			{
				++CurrentNatureReactionCount;
				++TotalReactionTileCount;
			}
			else if (Change.TargetType == EML_TileType::Parasite)
			{
				++CurrentParasiteReactionCount;
				++TotalReactionTileCount;
			}
			else if (Change.TargetType == EML_TileType::Water)
			{
				++CurrentWaterReactionCount;
				++TotalReactionTileCount;
			}

			if (UML_SoundSubsystem* SoundSubsystem = UML_SoundSubsystem::Get(this))
			{
				const FTransform SoundTransform(FRotator::ZeroRotator, Tile->GetActorLocation());

				if (OldType == EML_TileType::Grass && Change.TargetType == EML_TileType::Parasite)
				{
					SoundSubsystem->StartSoundAtLocationByPath(MLFMODEvents::TileParasiteSpread, SoundTransform);

					if (Tile == WinLoseSubsystem->GetPlayerCurrentTile())
					{
						SoundSubsystem->StartSoundAtLocationByPath(MLFMODEvents::TileParasiteEngulf, SoundTransform);
						SoundSubsystem->StartSoundAtLocationByPath(MLFMODEvents::AvatarEngulfedVocal, SoundTransform);
					}
				}

				if (OldType == EML_TileType::Parasite && Change.TargetType == EML_TileType::Water)
				{
					SoundSubsystem->StartSoundAtLocationByPath(MLFMODEvents::TileParasiteDieWater, SoundTransform);
					SoundSubsystem->StartSoundAtLocationByPath(MLFMODEvents::TileEarthDig, SoundTransform);
					SoundSubsystem->StartSoundAtLocationByPath(MLFMODEvents::TileWaterFill, SoundTransform);
				}
			}
		}

		// Destroy collectible if the tile changed to something other than dirt or grass
		// (because on dirt or grass it can stay)
		if (!UML_TileTypeTraits::CanSpawnCollectible(Change.TargetType))
		{
			if (Tile->HasCollectible())
			{
				if (AML_Collectible* Collectible = Tile->CollectibleActor.Get())
					if (IsValid(Collectible)) Collectible->DestroyCollectible();

				Tile->SetHasCollectible(false);
			}
		}

		// Parasite bookkeeping
		if (UML_TileTypeTraits::IsParasiteType(Tile->GetCurrentType()) && Tile->bConsumedGrass)
		{
			ParasitesThatAteGrass.Add(Tile);
			Tile->bConsumedGrass = false;
		}

		if (Change.Tile == WinLoseSubsystem->GetPlayerCurrentTile())
		{
			WinLoseSubsystem->CheckPlayerKilled(Change.Tile);
		}

		if (OldType != Change.TargetType)
		{
			bCycleHasChanges = true;
			bAnyChangeThisAction = true;
		}
	}
	// Collectible spawn
	else if (Change.CollectibleClass)
	{
		// Collectible wave - Deferred Spawn
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		// Create the actor WITHOUT the spawn (deferred)
		AML_Collectible* Collectible = GetWorld()->SpawnActorDeferred<AML_Collectible>(
			Change.CollectibleClass,
			FTransform(FRotator::ZeroRotator, Change.SpawnLocation),
			nullptr,
			nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn
		);

		if (Collectible)
		{
			// Configure BEFORE the spawn
			Collectible->SetOwningTile(Change.Neighbor);
			Collectible->SetSourceParasite(Change.SourceParasite);
			Change.Neighbor->CollectibleActor = Collectible;

			// Finish spawning
			Collectible->FinishSpawning(FTransform(FRotator::ZeroRotator, Change.SpawnLocation));

			if (RollBackSubsystem)
				RollBackSubsystem->RecordSpawnedActor(Collectible, Change.DistanceFromOrigin, CurrentPriorityIndexForRecording);

			if (UML_SoundSubsystem* SoundSubsystem = UML_SoundSubsystem::Get(this))
			{
				SoundSubsystem->StartSound2DByPath(MLFMODEvents::EnergySpawn);
			}

			bCycleHasChanges = true;
			bAnyChangeThisAction = true;
		}
	}
}

void UML_WavePropagationSubsystem::FinishRing()
{
	// Schedule next distance step (intra-wave) or next priority (inter-wave)
	if (PendingChangesIndex < PendingChanges.Num())
	{
		GetWorld()->GetTimerManager().SetTimer(IntraWaveTimerHandle, this, &UML_WavePropagationSubsystem::RunWave, DevSettings->IntraWaveDelay, false);
	}
	else
	{
		if (UML_SoundSubsystem* SoundSubsystem = UML_SoundSubsystem::Get(this))
		{
			if (CurrentNatureReactionCount > 0)
			{
				SoundSubsystem->StartSound2DByPath(MLFMODEvents::ReactionChainNature);
			}
			else if (CurrentParasiteReactionCount > 0)
			{
				SoundSubsystem->StartSound2DByPath(MLFMODEvents::ReactionChainParasite);
			}
			else if (CurrentWaterReactionCount > 0)
			{
				SoundSubsystem->StartSound2DByPath(MLFMODEvents::ReactionChainWater);
			}
		}

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

	const FML_WavePriorityEntry& WaveEntry = DevSettings->WavesPriority[CurrentWaveIndex];
	UML_PropagationWaves* WaveLogic = WaveEntry.WaveClass ? WaveEntry.WaveClass->GetDefaultObject<UML_PropagationWaves>() : nullptr;

	if (!WaveLogic)
	{
		CurrentWaveIndex++;
		ProcessNextWave();
		return;
	}

	CurrentPriorityIndexForRecording = CurrentWaveIndex;
	const bool bCanStopIfNoChanges = WaveEntry.bCanStopHereIfNoChanges;
	CurrentWaveIndex++;

	PendingChanges.Empty();
	PendingChangesIndex = 0;
	CurrentNatureReactionCount = 0;
	CurrentParasiteReactionCount = 0;
	CurrentWaterReactionCount = 0;

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
		// No changes in this wave
		if (bCanStopIfNoChanges)
		{
			// Stop immediately if this wave allows stopping
			EndTileResolved();
			return;
		}
		else
		{
			// Continue to the next wave
			ScheduleNextPriority();
			return;
		}
	}

	// StableSort keeps the order produced by the wave computation for tiles at the same
	// distance, so replay order (and the undo record) stays deterministic.
	PendingChanges.StableSort([](const FML_WaveChange& A, const FML_WaveChange& B)
	{
		return A.DistanceFromOrigin < B.DistanceFromOrigin;
	});

	RunWave();
}

void UML_WavePropagationSubsystem::AbortPropagationRuntime()
{
	bIsResolvingTiles = false;
	CancelAllWaveTimers();
	ParasitesThatAteGrass.Empty();
	PendingChanges.Empty();
	PendingChangesIndex = 0;
	bRingInProgress = false;
	PendingTouched.Empty();
	TouchIndex = 0;
	bTouchRingInProgress = false;
	CurrentOriginTile = nullptr;
	CurrentWaveIndex = 0;
	bCycleHasChanges = false;
	bAnyChangeThisAction = false;
	CurrentNatureReactionCount = 0;
	CurrentParasiteReactionCount = 0;
	CurrentWaterReactionCount = 0;
	TotalReactionTileCount = 0;
}

void UML_WavePropagationSubsystem::BuildTouchQueue(AML_Tile* OriginTile)
{
	if (GetWorld())
		GetWorld()->GetTimerManager().ClearTimer(TouchTimerHandle);

	PendingTouched.Empty();
	TouchIndex = 0;
	bTouchRingInProgress = false;

	AML_BoardSpawner* Board = OriginTile->GetBoardSpawnerFromTile();
	if (!Board) return;

	TSet<AML_Tile*> Visited;
	TQueue<TPair<AML_Tile*, int32>> Queue;
	FML_TileNeighbors Neighbors;

	PendingTouched.Add(FML_WaveChange(OriginTile, OriginTile->GetCurrentType(), 0));
	Visited.Add(OriginTile);
	Queue.Enqueue({ OriginTile, 0 });

	while (!Queue.IsEmpty())
	{
		TPair<AML_Tile*, int32> Current;
		Queue.Dequeue(Current);

		Board->GetNeighbors(Current.Key, Neighbors);
		for (AML_Tile* Neighbor : Neighbors)
		{
			if (!IsValid(Neighbor) || Visited.Contains(Neighbor)) continue;
			Visited.Add(Neighbor);
			const int32 Dist = Current.Value + 1;
			PendingTouched.Add(FML_WaveChange(Neighbor, Neighbor->GetCurrentType(), Dist));
			Queue.Enqueue({ Neighbor, Dist });
		}
	}

	// Already sorted by construction (BFS produces non-decreasing distances),
	// but sort explicitly to guarantee ordering. StableSort keeps the BFS order
	// inside each ring.
	PendingTouched.StableSort([](const FML_WaveChange& A, const FML_WaveChange& B)
	{
		return A.DistanceFromOrigin < B.DistanceFromOrigin;
	});
}

void UML_WavePropagationSubsystem::FireNextTouchRing()
{
	if (TouchIndex >= PendingTouched.Num() || !GetWorld() || !DevSettings) return;

	// Start the next touch ring; like the forward wave, it is applied under a per-frame
	// budget and continues in Tick when the ring is too big for one frame.
	CurrentTouchDistance = PendingTouched[TouchIndex].DistanceFromOrigin;
	bTouchRingInProgress = true;
	ProcessTouchSlice(MakeSliceDeadline());
}

void UML_WavePropagationSubsystem::ProcessTouchSlice(const double Deadline)
{
	while (TouchIndex < PendingTouched.Num()
		&& PendingTouched[TouchIndex].DistanceFromOrigin == CurrentTouchDistance)
	{
		if (IsValid(PendingTouched[TouchIndex].Tile))
			PendingTouched[TouchIndex].Tile->OnWaveTouched();
		TouchIndex++;

		const bool bRingHasMore =
			TouchIndex < PendingTouched.Num()
			&& PendingTouched[TouchIndex].DistanceFromOrigin == CurrentTouchDistance;

		// Budget exhausted: resume this ring next frame (Tick).
		if (bRingHasMore && FPlatformTime::Seconds() >= Deadline)
			return;
	}

	bTouchRingInProgress = false;

	if (TouchIndex < PendingTouched.Num())
	{
		GetWorld()->GetTimerManager().SetTimer(
			TouchTimerHandle, this, &UML_WavePropagationSubsystem::FireNextTouchRing,
			DevSettings->IntraWaveDelay, false
		);
	}
}

void UML_WavePropagationSubsystem::RecordTileForUndo(AML_Tile* Tile, int32 DistanceFromOrigin)
{
	if (!RollBackSubsystem) EnsureInitialized();
	if (RollBackSubsystem)
	{
		RollBackSubsystem->RecordTileForUndo(Tile, DistanceFromOrigin, CurrentPriorityIndexForRecording);
	}
}

bool UML_WavePropagationSubsystem::CanUndo() const
{
	if (RollBackSubsystem) return RollBackSubsystem->CanUndo();
	if (const UWorld* World = GetWorld())
		if (const UML_RollBackSubsystem* Subsystem = World->GetSubsystem<UML_RollBackSubsystem>())
			return Subsystem->CanUndo();
	return false;
}

bool UML_WavePropagationSubsystem::UndoLastAction_Animated()
{
	if (!RollBackSubsystem) EnsureInitialized();
	return RollBackSubsystem ? RollBackSubsystem->UndoLastAction_Animated() : false;
}

void UML_WavePropagationSubsystem::FinishUndoAnimation()
{
	if (!RollBackSubsystem) EnsureInitialized();
	if (RollBackSubsystem)
	{
		RollBackSubsystem->FinishUndoAnimation();
	}
}

bool UML_WavePropagationSubsystem::RestoreCollectibleDuringUndoMove(const FIntPoint& Axial)
{
	if (!RollBackSubsystem) EnsureInitialized();
	return RollBackSubsystem ? RollBackSubsystem->RestoreCollectibleDuringUndoMove(Axial) : false;
}

void UML_WavePropagationSubsystem::NotifyMoveCompleted(
	const FIntPoint& StartAxial,
	const FIntPoint& EndAxial,
	const TArray<FIntPoint>& AxialPath,
	const FVector& StartWorld,
	const FVector& EndWorld,
	const TArray<FIntPoint>& PickedCollectibleAxials)
{
	if (!RollBackSubsystem) EnsureInitialized();
	if (RollBackSubsystem)
	{
		RollBackSubsystem->NotifyMoveCompleted(StartAxial, EndAxial, AxialPath, StartWorld, EndWorld, PickedCollectibleAxials);
	}
}

bool UML_WavePropagationSubsystem::ResetAllActions_Animated()
{
	if (!RollBackSubsystem) EnsureInitialized();
	return RollBackSubsystem ? RollBackSubsystem->ResetAllActions_Animated() : false;
}

bool UML_WavePropagationSubsystem::ResetAllActions_ExcludingMoves_Animated()
{
	if (!RollBackSubsystem) EnsureInitialized();
	return RollBackSubsystem ? RollBackSubsystem->ResetAllActions_ExcludingMoves_Animated() : false;
}

void UML_WavePropagationSubsystem::ResetAllActions_ExcludingMoves_Instant(AML_BoardSpawner* Board)
{
	if (!RollBackSubsystem) EnsureInitialized();
	if (RollBackSubsystem)
	{
		RollBackSubsystem->ResetAllActions_ExcludingMoves_Instant(Board);
	}
}

void UML_WavePropagationSubsystem::HandleRollbackUndoAnimating(bool bIsAnimating)
{
	OnUndoAnimating.Broadcast(bIsAnimating);
}

void UML_WavePropagationSubsystem::HandleRollbackResetAnimating(bool bIsAnimating)
{
	OnResetAnimating.Broadcast(bIsAnimating);
}
