// Copyright Myceland Team, All Rights Reserved.


#include "Waves/ChildWaves/ML_WaveParasite.h"

#include "Core/ML_TileTypeTraits.h"
#include "Tiles/ML_BoardSpawner.h"
#include "Core/ML_CoreData.h"
#include "Tiles/ML_Tile.h"

void UML_WaveParasite::ComputeWave(AML_Tile* OriginTile, TArray<FML_WaveChange>& OutChanges)
{
	if (!OriginTile) return;

	AML_BoardSpawner* Board = OriginTile->GetBoardSpawnerFromTile();
	ensureMsgf(Board, TEXT("Board is not set!"));
	if (!Board) return;

	TQueue<TPair<AML_Tile*, int32>> Queue;
	TSet<AML_Tile*> Visited;

	// Get all the tiles in the board spawner
	const TArray<AML_Tile*>& AllTiles = Board->GetGridTiles();
	for (AML_Tile* Tile : AllTiles)
	{
		if (!Tile) continue;

		if (UML_TileTypeTraits::IsParasiteType(Tile->GetCurrentType()))
		{
			Queue.Enqueue({ Tile, 0 });
			Visited.Add(Tile);
		}
	}

	// If there is no parasite, then do nothing
	if (Queue.IsEmpty())
		return;

	// Chain propagation
	FML_TileNeighbors Neighbors;
	while (!Queue.IsEmpty())
	{
		TPair<AML_Tile*, int32> Current;
		Queue.Dequeue(Current);

		AML_Tile* CurrentTile = Current.Key;
		int32 Distance = Current.Value;

		Board->GetNeighbors(CurrentTile, Neighbors);

		for (int32 NeighborIndex = 0; NeighborIndex < Neighbors.Num(); ++NeighborIndex)
		{
			AML_Tile* Neighbor = Neighbors[NeighborIndex];

			if (!Neighbor || Visited.Contains(Neighbor))
				continue;

			Visited.Add(Neighbor);

			if (UML_TileTypeTraits::CanParasitePropagateTo(Neighbor->GetCurrentType()))
			{
				FML_WaveChange Change(
					Neighbor,
					EML_TileType::Parasite,
					Distance + 1);

				Change.SourcePropagationTile = CurrentTile;
				Change.PropagationNeighborIndex = NeighborIndex;

				OutChanges.Add(Change);

				Queue.Enqueue({ Neighbor, Distance + 1 });
			}
		}
	}
}
