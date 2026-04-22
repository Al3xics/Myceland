// Copyright Myceland Team, All Rights Reserved.

#include "Subsystem/ML_WavePropagationSubsystem.h"

#include "Developer Settings/ML_MycelandDeveloperSettings.h"
#include "Player/ML_PlayerController.h"
#include "Collectible/ML_Collectible.h"
#include "Subsystem/ML_RollBackSubsystem.h"
#include "Subsystem/ML_WinLoseSubsystem.h"
#include "Tiles/ML_BoardSpawner.h"
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
}

void UML_WavePropagationSubsystem::EndTileResolved()
{
	WinLoseSubsystem->CheckWinLose();
	WinLoseSubsystem->OnCheckPaths.Broadcast();
	WinLoseSubsystem->TriggerFindConnectedGoalCheck();

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

	ParasitesThatAteGrass.Empty();
	PendingChanges.Empty();

	if (RollBackSubsystem)
		RollBackSubsystem->BeginTurnRecord(HitTile);

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
			const EML_TileType OldType = Change.Tile->GetCurrentType();
			AML_Tile* Tile = Change.Tile;
			if (!IsValid(Tile)) continue;

			Tile->OnWaveTouched();

			if (RollBackSubsystem)
				RollBackSubsystem->RecordTileForUndo(Tile, Change.DistanceFromOrigin, CurrentPriorityIndexForRecording);

			const UML_BiomeTileSet* TileSet = Tile->GetBoardSpawnerFromTile()->GetBiomeTileSet();
			if (!TileSet) return;

			if (!RollBackSubsystem || !RollBackSubsystem->IsUndoInProgress())
				Tile->UpdateClassAtRuntime(Change.TargetType, TileSet->GetClassFromTileType(Change.TargetType));
			else
				Tile->UpdateClassAtRuntime_Silent(Change.TargetType, TileSet->GetClassFromTileType(Change.TargetType));

			// Destroy collectible if the tile changed to something other than dirt or grass
			// (because on dirt or grass it can stay)
			if (Change.TargetType != EML_TileType::Dirt &&
				Change.TargetType != EML_TileType::Grass)
			{
				if (Tile->HasCollectible())
				{
					if (AML_Collectible* Collectible = Tile->CollectibleActor.Get())
						if (IsValid(Collectible)) Collectible->DestroyCollectible();
					
					Tile->SetHasCollectible(false);
				}
			}

			// Parasite bookkeeping
			if (Tile->GetCurrentType() == EML_TileType::Parasite && Tile->bConsumedGrass)
			{
				ParasitesThatAteGrass.Add(Tile);
				Tile->bConsumedGrass = false;
			}

			if (Change.Tile == WinLoseSubsystem->GetPlayerCurrentTile())
			{
				WinLoseSubsystem->CheckPlayerKilled(Change.Tile);
			}

			if (OldType != Change.TargetType) bCycleHasChanges = true;
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
				Change.Neighbor->CollectibleActor = Collectible;

				// Finish spawning
				Collectible->FinishSpawning(FTransform(FRotator::ZeroRotator, Change.SpawnLocation));

				if (RollBackSubsystem)
					RollBackSubsystem->RecordSpawnedActor(Collectible, Change.DistanceFromOrigin, CurrentPriorityIndexForRecording);

				bCycleHasChanges = true;
			}
		}
	}

	// Schedule next distance step (intra-wave) or next priority (inter-wave)
	if (PendingChanges.Num() > 0)
	{
		GetWorld()->GetTimerManager().SetTimer(IntraWaveTimerHandle, this, &UML_WavePropagationSubsystem::RunWave, DevSettings->IntraWaveDelay, false);
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

	PendingChanges.Sort([](const FML_WaveChange& A, const FML_WaveChange& B)
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
	CurrentOriginTile = nullptr;
	CurrentWaveIndex = 0;
	bCycleHasChanges = false;
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
