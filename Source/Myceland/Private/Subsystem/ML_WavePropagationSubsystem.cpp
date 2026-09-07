// Copyright Myceland Team, All Rights Reserved.

#include "Subsystem/ML_WavePropagationSubsystem.h"

#include "Audio/ML_FMODEvents.h"
#include "Core/ML_TileTypeTraits.h"
#include "Developer Settings/ML_MycelandDeveloperSettings.h"
#include "Player/ML_PlayerController.h"
#include "Collectible/ML_Collectible.h"
#include "Subsystem/ML_BoardActionSubsystem.h"
#include "Subsystem/ML_CinematicSubsystem.h"
#include "Subsystem/ML_RollBackSubsystem.h"
#include "Subsystem/ML_SoundSubsystem.h"
#include "Subsystem/ML_WinLoseSubsystem.h"
#include "Tiles/ML_BoardSpawner.h"
#include "Tiles/ML_Tile.h"
#include "Data Asset/ML_BiomeTileSet.h"
#include "Waves/ML_PropagationWaves.h"
#include "Tiles/TileBase/ML_TileParasite.h"
#include "Tiles/TileBase/ML_TileGrass.h"
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
	CinematicSubsystem = GetWorld()->GetSubsystem<UML_CinematicSubsystem>();
	
	ensure(WinLoseSubsystem && PlayerController && DevSettings && RollBackSubsystem && CinematicSubsystem);

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

	// Includes the visual settle timeout, and drops the gate flag with it: clearing the timer alone would
	// leave a wave waiting on a report that nothing is scheduled to force anymore.
	ClearPendingVisuals();
}

void UML_WavePropagationSubsystem::EndTileResolved()
{
	// A wave can resolve on a board the player is not standing on (e.g. hub tile
	// changes after a win). Win/lose and goal-path logic only concern the player's
	// board, so skip both when the wave demonstrably ran elsewhere. If either board
	// is unknown (first wave before CheckWinLose resolved it), keep the old behavior.
	AML_BoardSpawner* WaveBoard = IsValid(CurrentOriginTile) ? CurrentOriginTile->GetBoardSpawnerFromTile() : nullptr;
	const bool bWaveOnOtherBoard = IsValid(WaveBoard) && IsValid(WinLoseSubsystem->CurrentBoardSpawner) && WaveBoard != WinLoseSubsystem->CurrentBoardSpawner;

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

	// Animations may well outlive the propagation (a collectible is still landing); no wave is left to wait
	// on them, so drop the bindings rather than carrying them into the next turn.
	ClearPendingVisuals();

	bIsResolvingTiles = false;

	if (RollBackSubsystem)
		RollBackSubsystem->CommitTurnRecord();

	if (PlayerController && PlayerController->TransitionComponent)
		PlayerController->TransitionComponent->OnBoardActivityStateChanged.Broadcast(false);

	// The propagation is over, but the animations it spawned (collectible flight, parasite crash) can
	// still be running and hold tokens of their own. Releasing ours only gives the input back if we were
	// the last one, which is what closes the gap the old BP delays used to paper over. The win-sequence
	// and cinematic lock is honoured inside the subsystem.
	if (UML_BoardActionSubsystem* BoardAction = UML_BoardActionSubsystem::Get(this))
		BoardAction->EndBoardAction(this);

	// The board has reached its final state: let board-dependent UI (e.g. the gamepad plantable
	// highlight) refresh once, rather than on every tile change during the propagation.
	OnWavePropagationFinished.Broadcast();
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

	// One token for the whole turn, held until EndTileResolved. Anything that starts animating during the
	// propagation takes its own token before this one is released, so the lock never briefly opens.
	if (UML_BoardActionSubsystem* BoardAction = UML_BoardActionSubsystem::Get(this))
		BoardAction->BeginBoardAction(this, EML_BoardActionReason::Wave, 30.f);
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
	ClearPendingVisuals();

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
		const bool bIsUndo =
			RollBackSubsystem && RollBackSubsystem->IsUndoInProgress();
		if (!bIsUndo)
	{
	    // ---------------------------------------------------------
	    // GRASS -> PARASITE
	    // Start the Grass transition immediately,
	    // wait, then actually replace Grass with Parasite.
	    // ---------------------------------------------------------
	    if (OldType == EML_TileType::Grass &&
	        Change.TargetType == EML_TileType::Parasite)
	    {
	        // Tell the existing Grass Blueprint to start its transition.
	        if (UChildActorComponent* ChildComponent = Tile->GetTileChildActor())
	        {
	            if (AML_TileGrass* GrassActor =
	                Cast<AML_TileGrass>(ChildComponent->GetChildActor()))
	            {
	                GrassActor->StartTransition();
	            }
	        }
	    	// The parasite that caused this Grass transition
	    	// starts its propagation effect immediately.
	    	if (IsValid(Change.SourcePropagationTile) &&
				Change.PropagationNeighborIndex != INDEX_NONE)
	    	{
	    		if (AML_TileParasite* SourceParasite =
					Cast<AML_TileParasite>(
						Change.SourcePropagationTile
							->GetTileChildActor()
							->GetChildActor()))
	    		{
	    			SourceParasite->Propagate(
						Change.PropagationNeighborIndex,
						Tile
					);
	    		}
	    	}
	        // IMPORTANT:
	        // Register this immediately so the collectible wave knows
	        // that this Parasite consumed Grass, even though the visual
	        // switch happens later.
	        if (!ParasitesThatAteGrass.Contains(Tile))
	        {
	            ParasitesThatAteGrass.Add(Tile);
	        }
			Tile->bConsumedGrass = true;

			// The transformation starts now and ends several seconds later (GrassToParasiteDelay, then the
			// parasite growth animation). A wave flagged bWaitForPendingVisuals waits on it.
			TrackPendingParasiteVisual(Tile);
	        TWeakObjectPtr<AML_Tile> WeakTile = Tile;
	   

	 

	        const TSubclassOf<AML_TileBase> ParasiteClass =
	            TileSet->GetClassFromTileType(EML_TileType::Parasite);

	        FTimerHandle ParasiteDelayHandle;

	    	GetWorld()->GetTimerManager().SetTimer(
			 ParasiteDelayHandle,
			 [WeakTile, ParasiteClass]()
			 {
				 if (!WeakTile.IsValid())
		 			return;

				 if (WeakTile->GetCurrentType() != EML_TileType::Grass)
		 			return;

				 WeakTile->UpdateClassAtRuntime(
					 EML_TileType::Parasite,
					 ParasiteClass
				 );

				
			 },
			 DevSettings->GrassToParasiteDelay,
			 false
		 );
	    }

    // ---------------------------------------------------------
    // ANYTHING -> GRASS
    // ---------------------------------------------------------
	    else if (Change.TargetType == EML_TileType::Grass)
	    {
	    	TWeakObjectPtr<AML_Tile> WeakTile = Tile;

	    	const TSubclassOf<AML_TileBase> GrassClass =
				TileSet->GetClassFromTileType(EML_TileType::Grass);

	    	FTimerHandle GrassDelayHandle;

	    	GetWorld()->GetTimerManager().SetTimer(
				GrassDelayHandle,
				[WeakTile, GrassClass]()
				{
					if (WeakTile.IsValid())
					{
						WeakTile->UpdateClassAtRuntime(
							EML_TileType::Grass,
							GrassClass
						);

						if (UML_SoundSubsystem* SoundSubsystem =
							UML_SoundSubsystem::Get(WeakTile.Get()))
						{
							SoundSubsystem->StartSoundAtLocationByPath(
								MLFMODEvents::TilePlant,
								FTransform(WeakTile->GetActorLocation())
							);
						}
					}
				},
				DevSettings->GrassSpawnDelay,
				false
			);
	    }

    // ---------------------------------------------------------
    // EVERYTHING ELSE
    // ---------------------------------------------------------
    else
    {
        Tile->UpdateClassAtRuntime(
            Change.TargetType,
            TileSet->GetClassFromTileType(Change.TargetType)
        );
    }
}
else
{
    // Undo remains immediate.
    Tile->UpdateClassAtRuntime_Silent(
        Change.TargetType,
        TileSet->GetClassFromTileType(Change.TargetType)
    );
}

		

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
			WinLoseSubsystem->CheckPlayerKilledByType(
				Change.Tile,
				Change.TargetType
			);
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

			// Before FinishSpawning, because BeginPlay is where the pickup overlap fires: a collectible
			// landing on the tile the player stands on was collected on its first frame, unseen.
			Collectible->PrepareForSpawnSequence();

			// Finish spawning
			Collectible->FinishSpawning(FTransform(FRotator::ZeroRotator, Change.SpawnLocation));

			if (RollBackSubsystem)
				RollBackSubsystem->RecordSpawnedActor(Collectible, Change.DistanceFromOrigin, CurrentPriorityIndexForRecording);

			// The wave resolves faster than the grass -> parasite transformation that triggered it, so the
			// collectible stays hidden until its own source parasite is done. Per collectible rather than
			// per wave: the energies then cascade in the order the parasites finish. The spawn sound moved
			// with the visual, inside BeginSpawnSequence.
			if (IsValid(Collectible))
			{
				// Tracked before the wait is armed: WaitForSourceParasite can start the flight synchronously
				// when the source parasite is already grown.
				TrackPendingCollectibleVisual(Collectible);
				Collectible->WaitForSourceParasite(DevSettings->CollectibleSourceReadyTimeout);
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
			if (CurrentNatureReactionCount > 1)
			{
				SoundSubsystem->StartSound2DByPath(MLFMODEvents::ReactionChainNature);
			}
			else if (CurrentParasiteReactionCount > 1)
			{
				SoundSubsystem->StartSound2DByPath(MLFMODEvents::ReactionChainParasite);
			}
			else if (CurrentWaterReactionCount > 1)
			{
				SoundSubsystem->StartSound2DByPath(MLFMODEvents::ReactionChainWater);
			}
		}

		ScheduleNextPriority();
	}
}

void UML_WavePropagationSubsystem::ScheduleNextPriority()
{
	// CurrentWaveIndex was already advanced in ProcessNextWave, so it points at the wave about to run.
	// A wave whose pacing already comes from the animations it waits on (the collectibles wait on their
	// source parasite) sets DelayBeforeWave to 0 so the two delays do not stack into a visible pause.
	float Delay = DevSettings->InterWaveDelay;
	bool bWaitForVisuals = false;

	if (DevSettings->WavesPriority.IsValidIndex(CurrentWaveIndex))
	{
		const FML_WavePriorityEntry& NextWave = DevSettings->WavesPriority[CurrentWaveIndex];

		const float Override = NextWave.DelayBeforeWave;
		if (Override >= 0.f)
			Delay = Override;

		bWaitForVisuals = NextWave.bWaitForPendingVisuals;
	}

	// The next wave needs a settled board (water, which would otherwise reach a parasite still growing out
	// of the grass). Hold here until every animation the previous waves started has reported, then apply
	// the delay on top, so the wait and the delay never overlap.
	if (bWaitForVisuals && HasPendingVisuals())
	{
		bWaitingForVisualSettle = true;
		PendingVisualSettleDelay = Delay;

		if (DevSettings->WaveVisualSettleTimeout > 0.f)
		{
			GetWorld()->GetTimerManager().SetTimer(
				VisualSettleTimeoutHandle,
				this,
				&UML_WavePropagationSubsystem::ForceReleaseVisualGate,
				DevSettings->WaveVisualSettleTimeout,
				false
			);
		}

		return;
	}

	StartNextWaveTimer(Delay);
}

void UML_WavePropagationSubsystem::StartNextWaveTimer(const float Delay)
{
	// A rate of 0 clears a timer instead of firing it: keep it schedulable so CancelAllWaveTimers still
	// owns the teardown.
	GetWorld()->GetTimerManager().SetTimer(
		InterWaveTimerHandle,
		this,
		&UML_WavePropagationSubsystem::ProcessNextWave,
		FMath::Max(Delay, 0.001f),
		false
	);
}

void UML_WavePropagationSubsystem::TrackPendingParasiteVisual(AML_Tile* Tile)
{
	// Called while the tile is still Grass: it becomes Parasite only after GrassToParasiteDelay, and the
	// parasite Blueprint reports through NotifyParasiteReady at the end of its growth animation.
	if (!IsValid(Tile)) return;

	Tile->OnParasiteReady.AddUniqueDynamic(this, &UML_WavePropagationSubsystem::HandlePendingParasiteReady);
	PendingVisuals.Add(Tile);
}

void UML_WavePropagationSubsystem::TrackPendingCollectibleVisual(AML_Collectible* Collectible)
{
	// A collectible collected on its spawn frame has already reported through EndPlay: nothing to wait for.
	if (!IsValid(Collectible) || Collectible->HasSpawnAnimationFinished()) return;

	Collectible->OnSpawnAnimationFinished.AddUniqueDynamic(this, &UML_WavePropagationSubsystem::HandlePendingCollectibleFinished);
	PendingVisuals.Add(Collectible);
}

bool UML_WavePropagationSubsystem::HasPendingVisuals()
{
	for (auto It = PendingVisuals.CreateIterator(); It; ++It)
	{
		UObject* Pending = It->Get();

		// Destroyed mid-animation: it will never report.
		if (!Pending)
		{
			It.RemoveCurrent();
			continue;
		}

		// A pending grass -> parasite transition that was overridden (the tile turned into something else
		// before its timer fired) has no parasite Blueprint left to report either.
		if (AML_Tile* Tile = Cast<AML_Tile>(Pending))
		{
			const EML_TileType Type = Tile->GetCurrentType();
			if (Type != EML_TileType::Grass && !UML_TileTypeTraits::IsParasiteType(Type))
			{
				Tile->OnParasiteReady.RemoveDynamic(this, &UML_WavePropagationSubsystem::HandlePendingParasiteReady);
				It.RemoveCurrent();
			}
		}
	}

	return PendingVisuals.Num() > 0;
}

void UML_WavePropagationSubsystem::HandlePendingParasiteReady(AML_Tile* Tile)
{
	if (IsValid(Tile))
		Tile->OnParasiteReady.RemoveDynamic(this, &UML_WavePropagationSubsystem::HandlePendingParasiteReady);

	PendingVisuals.Remove(TWeakObjectPtr<UObject>(Tile));
	TryReleaseVisualGate();
}

void UML_WavePropagationSubsystem::HandlePendingCollectibleFinished(AML_Collectible* Collectible)
{
	if (IsValid(Collectible))
		Collectible->OnSpawnAnimationFinished.RemoveDynamic(this, &UML_WavePropagationSubsystem::HandlePendingCollectibleFinished);

	PendingVisuals.Remove(TWeakObjectPtr<UObject>(Collectible));
	TryReleaseVisualGate();
}

void UML_WavePropagationSubsystem::TryReleaseVisualGate()
{
	if (!bWaitingForVisualSettle || HasPendingVisuals()) return;

	bWaitingForVisualSettle = false;

	if (GetWorld())
		GetWorld()->GetTimerManager().ClearTimer(VisualSettleTimeoutHandle);

	StartNextWaveTimer(PendingVisualSettleDelay);
}

void UML_WavePropagationSubsystem::ForceReleaseVisualGate()
{
	// A Blueprint never reported the end of its animation. Start the wave anyway rather than leaving the
	// board locked: the water wave still catches the tiles stuck mid grass -> parasite transition (see
	// UML_WaveWater), so this path degrades to the pre-gate behaviour instead of losing the reaction.
	if (!bWaitingForVisualSettle) return;

	bWaitingForVisualSettle = false;
	StartNextWaveTimer(PendingVisualSettleDelay);
}

void UML_WavePropagationSubsystem::ClearPendingVisuals()
{
	for (const TWeakObjectPtr<UObject>& Pending : PendingVisuals)
	{
		if (AML_Tile* Tile = Cast<AML_Tile>(Pending.Get()))
			Tile->OnParasiteReady.RemoveDynamic(this, &UML_WavePropagationSubsystem::HandlePendingParasiteReady);
		else if (AML_Collectible* Collectible = Cast<AML_Collectible>(Pending.Get()))
			Collectible->OnSpawnAnimationFinished.RemoveDynamic(this, &UML_WavePropagationSubsystem::HandlePendingCollectibleFinished);
	}

	PendingVisuals.Empty();
	bWaitingForVisualSettle = false;
	PendingVisualSettleDelay = 0.f;

	if (GetWorld())
		GetWorld()->GetTimerManager().ClearTimer(VisualSettleTimeoutHandle);
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

	// This path tears the propagation down without ever reaching EndTileResolved, so the turn token has
	// to be released here too -- otherwise the board stays locked until the timeout fires.
	if (UML_BoardActionSubsystem* BoardAction = UML_BoardActionSubsystem::Get(this))
		BoardAction->EndBoardAction(this);

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
	// The UMG button binds this: returning false while the board resolves greys it out instead of
	// letting the player queue an undo on top of a running turn.
	if (const UML_BoardActionSubsystem* BoardAction = UML_BoardActionSubsystem::Get(this))
	{
		if (BoardAction->IsBoardBusy()) return false;
	}

	if (RollBackSubsystem) return RollBackSubsystem->CanUndo();
	if (const UWorld* World = GetWorld())
		if (const UML_RollBackSubsystem* Subsystem = World->GetSubsystem<UML_RollBackSubsystem>())
			return Subsystem->CanUndo();
	return false;
}

bool UML_WavePropagationSubsystem::UndoLastAction_Animated()
{
	if (!RollBackSubsystem) EnsureInitialized();
	if (const UML_BoardActionSubsystem* BoardAction = UML_BoardActionSubsystem::Get(this))
	{
		if (BoardAction->IsBoardBusy()) return false;
	}
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
	if (const UML_BoardActionSubsystem* BoardAction = UML_BoardActionSubsystem::Get(this))
	{
		if (BoardAction->IsBoardBusy()) return false;
	}
	return RollBackSubsystem ? RollBackSubsystem->ResetAllActions_Animated() : false;
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
	// Undo and reset are mutually exclusive, so they can share the rollback subsystem as token owner.
	HoldBoardForRollback(bIsAnimating, EML_BoardActionReason::Undo);
	OnUndoAnimating.Broadcast(bIsAnimating);
}

void UML_WavePropagationSubsystem::HandleRollbackResetAnimating(bool bIsAnimating)
{
	HoldBoardForRollback(bIsAnimating, EML_BoardActionReason::Reset);
	OnResetAnimating.Broadcast(bIsAnimating);
}

void UML_WavePropagationSubsystem::HoldBoardForRollback(const bool bIsAnimating, const EML_BoardActionReason Reason)
{
	UML_BoardActionSubsystem* BoardAction = UML_BoardActionSubsystem::Get(this);
	if (!BoardAction || !RollBackSubsystem) return;

	if (bIsAnimating)
		BoardAction->BeginBoardAction(RollBackSubsystem, Reason, 30.f);
	else
		BoardAction->EndBoardAction(RollBackSubsystem);
}
