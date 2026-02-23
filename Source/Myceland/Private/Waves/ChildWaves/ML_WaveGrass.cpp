// Copyright Myceland Team, All Rights Reserved.


#include "Waves/ChildWaves/ML_WaveGrass.h"

#include "Tiles/ML_BoardSpawner.h"
#include "Core/ML_CoreData.h"
#include "Tiles/ML_Tile.h"

void UML_WaveGrass::ComputeWave(AML_Tile* OriginTile, TArray<FML_WaveChange>& OutChanges)
{
        if (!OriginTile) return;
    
    AML_BoardSpawner* Board = OriginTile->GetBoardSpawnerFromTile();
    ensureMsgf(Board, TEXT("Board is not set!"));
    if (!Board) return;
    
    TSet<AML_Tile*> Scheduled;
    
    // If the origin is no longer Dirt, it means the grass was eaten
    // or transformed during a previous cycle. Stop here.
    if (OriginTile->GetCurrentType() != EML_TileType::Dirt)
        return;
    
    // Step 0: Transforms the origin if it's Dirt
    OutChanges.Add(FML_WaveChange(OriginTile, EML_TileType::Grass, 0));
    Scheduled.Add(OriginTile);
    
    // Step 1.1: Build the connected water network from the source
    TSet<AML_Tile*> WaterConnected;
    TQueue<AML_Tile*> WaterQueue;
    
    for (AML_Tile* Neighbor : Board->GetNeighbors(OriginTile))
    {
        if (Neighbor && Neighbor->GetCurrentType() == EML_TileType::Water)
        {
            WaterConnected.Add(Neighbor);
            WaterQueue.Enqueue(Neighbor);
        }
    }
    
    // Step 1.2: Continue building the water network
    while (!WaterQueue.IsEmpty())
    {
        AML_Tile* CurrentWater;
        WaterQueue.Dequeue(CurrentWater);
    
        for (AML_Tile* Neighbor : Board->GetNeighbors(CurrentWater))
        {
            if (Neighbor && Neighbor->GetCurrentType() == EML_TileType::Water && !WaterConnected.Contains(Neighbor))
            {
                WaterConnected.Add(Neighbor);
                WaterQueue.Enqueue(Neighbor);
            }
        }
    }

    // Step 2: Sequential propagation by continuity
    TQueue<TPair<AML_Tile*, int32>> PropagationQueue;

    // Helper lambda to check if a tile is Dirt or Obstacle (treated as passable Dirt)
    auto IsDirtLike = [](AML_Tile* Tile) -> bool
    {
        return Tile->GetCurrentType() == EML_TileType::Dirt || Tile->GetCurrentType() == EML_TileType::Obstacle;
    };
    
    // Initial: all the Dirt neighboring the origin tile touching the water
    for (AML_Tile* Neighbor : Board->GetNeighbors(OriginTile))
    {
        // IsDirtLike instead of == Dirt
        if (Neighbor && IsDirtLike(Neighbor) && !Scheduled.Contains(Neighbor))
        {
            for (AML_Tile* Around : Board->GetNeighbors(Neighbor))
            {
                if (Around && WaterConnected.Contains(Around))
                {
                    PropagationQueue.Enqueue({ Neighbor, 1 });
                    Scheduled.Add(Neighbor);
                    break;
                }
            }
        }
    }
    
    while (!PropagationQueue.IsEmpty())
    {
        TPair<AML_Tile*, int32> Current;
        PropagationQueue.Dequeue(Current);
    
        AML_Tile* CurrentTile = Current.Key;
        int32 StepDistance = Current.Value;
    
        // Only add to OutChanges if it's real Dirt, not an Obstacle
        if (CurrentTile->GetCurrentType() == EML_TileType::Dirt)
        {
            OutChanges.Add(FML_WaveChange(CurrentTile, EML_TileType::Grass, StepDistance));
        }
    
        // Dirt neighbors must be neighbors of the current Grass tile
        for (AML_Tile* Neighbor : Board->GetNeighbors(CurrentTile))
        {
            // IsDirtLike instead of == Dirt
            if (!Neighbor || Scheduled.Contains(Neighbor) || !IsDirtLike(Neighbor))
                continue;
    
            // must touch the water network
            bool bTouchesWater = false;
            for (AML_Tile* Around : Board->GetNeighbors(Neighbor))
            {
                if (Around && WaterConnected.Contains(Around))
                {
                    bTouchesWater = true;
                    break;
                }
            }
    
            if (bTouchesWater)
            {
                PropagationQueue.Enqueue({ Neighbor, StepDistance + 1 });
                Scheduled.Add(Neighbor);
            }
        }
    }
}
