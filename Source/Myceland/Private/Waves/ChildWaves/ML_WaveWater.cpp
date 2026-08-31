// Copyright Myceland Team, All Rights Reserved.


#include "Waves/ChildWaves/ML_WaveWater.h"

#include "Core/ML_TileTypeTraits.h"
#include "Tiles/ML_BoardSpawner.h"
#include "Core/ML_CoreData.h"
#include "Tiles/ML_Tile.h"

void UML_WaveWater::ComputeWave(AML_Tile* OriginTile, TArray<FML_WaveChange>& OutChanges)
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

		if (UML_TileTypeTraits::IsWaterType(Tile->GetCurrentType()))
		{
			Queue.Enqueue({ Tile, 0 });
			Visited.Add(Tile);
		}
	}

	// If there is no water, then do nothing
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
		for (AML_Tile* Neighbor : Neighbors)
		{
			if (!Neighbor || Visited.Contains(Neighbor))
				continue;

			Visited.Add(Neighbor);

			// Water eats only a parasite
			// Water eats:
            // 1. A real parasite
            // 2. Grass that is currently in the delayed Grass -> Parasite transition
            const bool bIsParasite =
                UML_TileTypeTraits::CanWaterPropagateTo(Neighbor->GetCurrentType());
            
            const bool bIsPendingParasite =
                Neighbor->GetCurrentType() == EML_TileType::Grass &&
                Neighbor->bConsumedGrass;
            
            if (bIsParasite || bIsPendingParasite)
            {
                OutChanges.Add(
                    FML_WaveChange(
                        Neighbor,
                        EML_TileType::Water,
                        Distance + 1
                    )
                );
            
                // Once consumed, this tile becomes water and can continue
                // propagating through the parasite chain.
                Queue.Enqueue({ Neighbor, Distance + 1 });
            }
		}
	}
}
