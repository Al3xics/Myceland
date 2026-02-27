// Copyright Myceland Team, All Rights Reserved.


#include "Subsystem/ML_WavePropagationSubsystem.h"

#include "Collectible/ML_Collectible.h"
#include "Data Asset/ML_BiomeTileSet.h"
#include "Developer Settings/ML_MycelandDeveloperSettings.h"
#include "Player/ML_PlayerController.h"
#include "Subsystem/ML_WinLoseSubsystem.h"
#include "Tiles/ML_Tile.h"
#include "Waves/ML_PropagationWaves.h"
#include "Waves/ChildWaves/ML_WaveCollectible.h"

void UML_WavePropagationSubsystem::EnsureInitialized()
{
	WinLoseSubsystem = GetWorld()->GetSubsystem<UML_WinLoseSubsystem>();
	PlayerController = Cast<AML_PlayerController>(GetWorld()->GetFirstPlayerController());
	DevSettings = UML_MycelandDeveloperSettings::GetMycelandDeveloperSettings();
	
	ensure(PlayerController && WinLoseSubsystem);
}

void UML_WavePropagationSubsystem::EndTileResolved()
{
	WinLoseSubsystem->CheckWinLose();
	WinLoseSubsystem->OnCheckPaths.Broadcast();
	WinLoseSubsystem->TriggerFindConnectedGoalCheck();
	
	bIsResolvingTiles = false;
	PlayerController->EnableInput(PlayerController);
}

void UML_WavePropagationSubsystem::RunWave()
{
	if (PendingChanges.Num() == 0)
	{
		EndTileResolved();
		return;
	}

	// We retrieve the tiles of the same distance (wave)
	const int32 CurrentDistance = PendingChanges[0].DistanceFromOrigin;

	TArray<FML_WaveChange> CurrentWave;
	int32 Index = 0;
	while (Index < PendingChanges.Num() && PendingChanges[Index].DistanceFromOrigin == CurrentDistance)
	{
		CurrentWave.Add(PendingChanges[Index]);
		Index++;
	}

	// Remove them from pending
	PendingChanges.RemoveAt(0, CurrentWave.Num());

	// Apply changes and detect real modifications
	for (const FML_WaveChange& Change : CurrentWave)
	{
		if (Change.Tile)
		{
			// Tile wave
			const EML_TileType OldType = Change.Tile->GetCurrentType();
			const UML_BiomeTileSet* TileSet = Change.Tile->GetBoardSpawnerFromTile()->GetBiomeTileSet();
			ensureMsgf(TileSet, TEXT("TileSet (Biome Data Asset) is not set for this BoardSpawner : %s"), *Change.Tile->GetBoardSpawnerFromTile()->GetName());
			if (!TileSet) return;
			Change.Tile->UpdateClassAtRuntime(Change.TargetType, TileSet->GetClassFromTileType(Change.TargetType));
			
			if (Change.Tile->GetCurrentType() == EML_TileType::Parasite &&
				Change.Tile->bConsumedGrass)
			{
				ParasitesThatAteGrass.Add(Change.Tile);
				Change.Tile->bConsumedGrass = false;
			}
			
			if (Change.Tile == WinLoseSubsystem->GetPlayerCurrentTile())
			{
				WinLoseSubsystem->CheckPlayerKilled(Change.Tile);
			}
			
			if (OldType != Change.TargetType) bCycleHasChanges = true;
		}
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
        
				bCycleHasChanges = true;
			}
		}
	}

	// Continue the same wave (next distance) with INTRA delay
	if (PendingChanges.Num() > 0)
	{
		FTimerHandle TimerHandle;
		GetWorld()->GetTimerManager().SetTimer(TimerHandle, this, &UML_WavePropagationSubsystem::RunWave, DevSettings->IntraWaveDelay, false);
	}
	else
	{
		ScheduleNextPriority();
	}
}

void UML_WavePropagationSubsystem::ScheduleNextPriority()
{
	FTimerHandle TimerHandle;
	GetWorld()->GetTimerManager().SetTimer(TimerHandle, this, &UML_WavePropagationSubsystem::ProcessNextWave, DevSettings->InterWaveDelay, false);
}

void UML_WavePropagationSubsystem::ProcessNextWave()
{
	if (!DevSettings || !CurrentOriginTile)
	{
		EndTileResolved();
		return;
	}

	// When every wave in WavesPriority is done, we do this
	if (CurrentWaveIndex >= DevSettings->WavesPriority.Num())
	{
		// End of one full priority pass
		if (bCycleHasChanges)
		{
			// Restart from the first wave
			CurrentWaveIndex = 0;
			bCycleHasChanges = false;
			
			// Restart the full cycle immediately (no delay needed here)
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

	CurrentWaveIndex++;

	PendingChanges.Empty();
	if (UML_WaveCollectible* CollectibleWave = Cast<UML_WaveCollectible>(WaveLogic))
	{
		CollectibleWave->ComputeWaveForCollectibles(CurrentOriginTile, ParasitesThatAteGrass, PendingChanges);
		ParasitesThatAteGrass.Empty();
	}
	else
		WaveLogic->ComputeWave(CurrentOriginTile, PendingChanges);

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

void UML_WavePropagationSubsystem::BeginTileResolved(AML_Tile* HitTile)
{
	if (!HitTile || bIsResolvingTiles) return;
	
	bIsResolvingTiles = true;
	PlayerController->DisableInput(PlayerController);
	
	CurrentOriginTile = HitTile;
	CurrentWaveIndex = 0;
	bCycleHasChanges = false;
	
	ProcessNextWave();
}
