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

void UML_WinPropagationSubsystem::HandleBoardPropagationOnWin()
{
	if (!WinLoseSubsystem || !WinLoseSubsystem->CurrentBoardSpawner) return;

	AML_BoardSpawner* Board = WinLoseSubsystem->CurrentBoardSpawner;
	const TArray<AML_Tile*> TreeTiles = Board->GetTreeTiles();
	if (TreeTiles.Num() == 0) return;

	WinWaveEntries.Empty();

	// Multi-source BFS from all tree tiles simultaneously.
	// Distance = hexagonal distance from the nearest tree.
	TSet<AML_Tile*> Visited;
	TQueue<TPair<AML_Tile*, int32>> Queue;

	for (AML_Tile* Tree : TreeTiles)
	{
		if (!IsValid(Tree)) continue;
		Visited.Add(Tree);
		Queue.Enqueue({ Tree, 0 });
	}

	while (!Queue.IsEmpty())
	{
		TPair<AML_Tile*, int32> Current;
		Queue.Dequeue(Current);

		const int32 NextDistance = Current.Value + 1;

		for (AML_Tile* Neighbor : Board->GetNeighbors(Current.Key))
		{
			if (!IsValid(Neighbor) || Visited.Contains(Neighbor)) continue;
			Visited.Add(Neighbor);

			const EML_TileType Type = Neighbor->GetCurrentType();

			// Dirt and Parasite are converted to Grass; all other types are passed through unchanged.
			const EML_TileType TargetType = UML_TileTypeTraits::IsWinPropagationConvertible(Type)
				? EML_TileType::Grass
				: Type;

			WinWaveEntries.Add(FML_WaveChange(Neighbor, TargetType, NextDistance));
			Queue.Enqueue({ Neighbor, NextDistance });
		}
	}

	WinWaveEntries.Sort([](const FML_WaveChange& A, const FML_WaveChange& B)
	{
		return A.DistanceFromOrigin < B.DistanceFromOrigin;
	});

	RunWinWave();
}

void UML_WinPropagationSubsystem::RunWinWave()
{
	if (WinWaveEntries.Num() == 0 || !GetWorld() || !DevSettings) return;

	const int32 CurrentDistance = WinWaveEntries[0].DistanceFromOrigin;

	int32 Count = 0;
	while (Count < WinWaveEntries.Num() && WinWaveEntries[Count].DistanceFromOrigin == CurrentDistance)
	{
		const FML_WaveChange& Entry = WinWaveEntries[Count];
		AML_Tile* Tile = Entry.Tile;

		if (IsValid(Tile))
		{
			Tile->OnWaveTouched();

			// Use the tile's live type, not the BFS snapshot — ClearWinPath may have
			// already converted Water → Grass before this timer fires.
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

		Count++;
	}

	WinWaveEntries.RemoveAt(0, Count);

	if (WinWaveEntries.Num() > 0)
	{
		GetWorld()->GetTimerManager().SetTimer(
			WinWaveTimerHandle,
			this,
			&UML_WinPropagationSubsystem::RunWinWave,
			DevSettings->IntraWaveDelay,
			false
		);
	}
}
