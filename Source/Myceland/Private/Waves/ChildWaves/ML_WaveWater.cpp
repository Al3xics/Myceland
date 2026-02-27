// Copyright Myceland Team, All Rights Reserved.


#include "Waves/ChildWaves/ML_WaveWater.h"

#include "Tiles/ML_BoardSpawner.h"
#include "Core/ML_CoreData.h"
#include "Tiles/ML_Tile.h"

void UML_WaveWater::ComputeWave(AML_Tile* OriginTile, TArray<FML_WaveChange>& OutChanges)
{
	GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Blue, TEXT("Water Wave"));
	
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

		if (Tile->GetCurrentType() == EML_TileType::Water)
		{
			Queue.Enqueue({ Tile, 0 });
			Visited.Add(Tile);
		}
	}

	// If there is no water, then do nothing
	if (Queue.IsEmpty())
		return;

	// Chain propagation
	while (!Queue.IsEmpty())
	{
		TPair<AML_Tile*, int32> Current;
		Queue.Dequeue(Current);

		AML_Tile* CurrentTile = Current.Key;
		int32 Distance = Current.Value;

		TArray<AML_Tile*> Neighbors = Board->GetNeighbors(CurrentTile);
		for (AML_Tile* Neighbor : Neighbors)
		{
			if (!Neighbor || Visited.Contains(Neighbor))
				continue;

			Visited.Add(Neighbor);

			// Water eats only a parasite
			if (Neighbor->GetCurrentType() == EML_TileType::Parasite)
			{
				OutChanges.Add(FML_WaveChange(Neighbor, EML_TileType::Water, Distance + 1));

				// The transformed parasite becomes water
				// therefore, it can continue to spread
				Queue.Enqueue({ Neighbor, Distance + 1 });
			}
		}
	}
}
