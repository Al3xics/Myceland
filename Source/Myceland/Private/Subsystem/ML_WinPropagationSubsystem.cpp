// Copyright Myceland Team, All Rights Reserved.

#include "Subsystem/ML_WinPropagationSubsystem.h"

#include "Core/ML_TileTypeTraits.h"
#include "Data Asset/ML_BiomeTileSet.h"
#include "Developer Settings/ML_MycelandDeveloperSettings.h"
#include "Subsystem/ML_WinLoseSubsystem.h"
#include "Tiles/ML_BoardSpawner.h"
#include "Tiles/ML_Tile.h"

void UML_WinPropagationSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	WinLoseSubsystem = InWorld.GetSubsystem<UML_WinLoseSubsystem>();
	DevSettings = UML_MycelandDeveloperSettings::GetMycelandDeveloperSettings();

	if (WinLoseSubsystem && !bDelegateBound)
	{
		WinLoseSubsystem->OnWin.AddDynamic(this, &UML_WinPropagationSubsystem::HandleBoardPropagationOnWin);
		bDelegateBound = true;
	}
}

void UML_WinPropagationSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bWinRingInProgress)
		ProcessWinRingSlice(MakeWinSliceDeadline());
}

bool UML_WinPropagationSubsystem::IsTickable() const
{
	return bWinRingInProgress;
}

TStatId UML_WinPropagationSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UML_WinPropagationSubsystem, STATGROUP_Tickables);
}

double UML_WinPropagationSubsystem::MakeWinSliceDeadline() const
{
	const float BudgetMs = DevSettings ? DevSettings->WinFrameBudgetMs : 2.f;
	return FPlatformTime::Seconds() + static_cast<double>(BudgetMs) / 1000.0;
}

void UML_WinPropagationSubsystem::HandleBoardPropagationOnWin()
{
	if (!GetWorld()) return;

	FTimerHandle TimerHandle;

	GetWorld()->GetTimerManager().SetTimer(
		TimerHandle,
		this,
		&UML_WinPropagationSubsystem::StartWinPropagation,
		1.5f,
		false
	);
}
void UML_WinPropagationSubsystem::StartWinPropagation()
{
	if (!WinLoseSubsystem || !WinLoseSubsystem->CurrentBoardSpawner) return;

	AML_BoardSpawner* Board = WinLoseSubsystem->CurrentBoardSpawner;
	const TArray<AML_Tile*> TreeTiles = Board->GetTreeTiles();

	WinWaveEntries.Empty();
	WinWaveIndex = 0;
	bWinRingInProgress = false;

	// Multi-source BFS.
	// Normal win: starts from tree tiles.
	// No-tree forced win: starts from all convertible tiles.
	TSet<AML_Tile*> Visited;
	TQueue<TPair<AML_Tile*, int32>> Queue;

	if (TreeTiles.Num() > 0)
	{
		for (AML_Tile* Tree : TreeTiles)
		{
			if (!IsValid(Tree)) continue;

			Visited.Add(Tree);
			Queue.Enqueue({ Tree, 0 });
		}
	}
	else
	{
		for (AML_Tile* Tile : Board->GetGridTiles())
		{
			if (!IsValid(Tile)) continue;

			if (!UML_TileTypeTraits::IsWinPropagationConvertible(Tile->GetCurrentType()))
				continue;

			Visited.Add(Tile);

			// Unlike tree sources, these source tiles themselves need to become Grass.
			WinWaveEntries.Add(
			   FML_WaveChange(
				  Tile,
				  EML_TileType::Grass,
				  0
			   )
			);

			Queue.Enqueue({ Tile, 0 });
		}
	}

	FML_TileNeighbors Neighbors;

	while (!Queue.IsEmpty())
	{
		TPair<AML_Tile*, int32> Current;
		Queue.Dequeue(Current);

		const int32 NextDistance = Current.Value + 1;

		Board->GetNeighbors(Current.Key, Neighbors);

		for (AML_Tile* Neighbor : Neighbors)
		{
			if (!IsValid(Neighbor) || Visited.Contains(Neighbor))
				continue;

			Visited.Add(Neighbor);

			const EML_TileType Type = Neighbor->GetCurrentType();

			// Dirt and Parasite are converted to Grass;
			// all other types stay unchanged.
			const EML_TileType TargetType =
				UML_TileTypeTraits::IsWinPropagationConvertible(Type)
					? EML_TileType::Grass
					: Type;

			WinWaveEntries.Add(
				FML_WaveChange(
					Neighbor,
					TargetType,
					NextDistance
				)
			);

			Queue.Enqueue({ Neighbor, NextDistance });
		}
	}

	WinWaveEntries.StableSort(
		[](const FML_WaveChange& A, const FML_WaveChange& B)
		{
			return A.DistanceFromOrigin < B.DistanceFromOrigin;
		}
	);

	RunWinWave();
}
void UML_WinPropagationSubsystem::RunWinWave()
{
	if (WinWaveIndex >= WinWaveEntries.Num() || !GetWorld() || !DevSettings) return;

	// Start the next ring. Like the forward wave propagation, the ring is applied
	// under a per-frame CPU budget: whatever doesn't fit continues in Tick.
	CurrentWinRingDistance = WinWaveEntries[WinWaveIndex].DistanceFromOrigin;
	bWinRingInProgress = true;
	ProcessWinRingSlice(MakeWinSliceDeadline());
}

void UML_WinPropagationSubsystem::ProcessWinRingSlice(const double Deadline)
{
	while (WinWaveIndex < WinWaveEntries.Num()
		&& WinWaveEntries[WinWaveIndex].DistanceFromOrigin == CurrentWinRingDistance)
	{
		AML_Tile* Tile = WinWaveEntries[WinWaveIndex].Tile;

		if (IsValid(Tile))
		{
			Tile->OnWaveTouched();

			// Use the tile's live type, not the BFS snapshot — ClearWinPath may have
			// already converted Water → Grass before this ring runs.
			const EML_TileType LiveType = Tile->GetCurrentType();
			if (UML_TileTypeTraits::IsWinPropagationConvertible(LiveType))
			{
				const AML_BoardSpawner* Board = Tile->GetBoardSpawnerFromTile();
				if (const UML_BiomeTileSet* TileSet = Board ? Board->GetBiomeTileSet() : nullptr)
				{
					Tile->UpdateClassAtRuntime(EML_TileType::Grass, TileSet->GetClassFromTileType(EML_TileType::Grass));
				}
			}
		}

		WinWaveIndex++;

		const bool bRingHasMore =
			WinWaveIndex < WinWaveEntries.Num()
			&& WinWaveEntries[WinWaveIndex].DistanceFromOrigin == CurrentWinRingDistance;

		// Budget exhausted: resume this ring next frame (Tick).
		if (bRingHasMore && FPlatformTime::Seconds() >= Deadline)
			return;
	}

	bWinRingInProgress = false;

	if (WinWaveIndex < WinWaveEntries.Num())
	{
		GetWorld()->GetTimerManager().SetTimer(
			WinWaveTimerHandle,
			this,
			&UML_WinPropagationSubsystem::RunWinWave,
			DevSettings->IntraWaveDelay,
			false
		);
	}
	else
	{
		WinWaveEntries.Empty();
		WinWaveIndex = 0;
	}
}
