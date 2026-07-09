// Copyright Myceland Team, All Rights Reserved.


#include "Waves/ChildWaves/ML_WaveGrass.h"

#include "Core/ML_TileTypeTraits.h"
#include "Tiles/ML_BoardSpawner.h"
#include "Core/ML_CoreData.h"
#include "Tiles/ML_Tile.h"

void UML_WaveGrass::ExpandWaterNetwork(AML_BoardSpawner* Board, AML_Tile* FromTile, TSet<AML_Tile*>& WaterConnected)
{
    TQueue<AML_Tile*> ExpansionQueue;
    FML_TileNeighbors Neighbors;

    Board->GetNeighbors(FromTile, Neighbors);
    for (AML_Tile* Neighbor : Neighbors)
    {
        if (Neighbor && UML_TileTypeTraits::IsWaterType(Neighbor->GetCurrentType()) && !WaterConnected.Contains(Neighbor))
        {
            WaterConnected.Add(Neighbor);
            ExpansionQueue.Enqueue(Neighbor);
        }
    }

    while (!ExpansionQueue.IsEmpty())
    {
        AML_Tile* CurrentWater;
        ExpansionQueue.Dequeue(CurrentWater);

        Board->GetNeighbors(CurrentWater, Neighbors);
        for (AML_Tile* Neighbor : Neighbors)
        {
            if (Neighbor && UML_TileTypeTraits::IsWaterType(Neighbor->GetCurrentType()) && !WaterConnected.Contains(Neighbor))
            {
                WaterConnected.Add(Neighbor);
                ExpansionQueue.Enqueue(Neighbor);
            }
        }
    }
}

void UML_WaveGrass::ComputeWave(AML_Tile* OriginTile, TArray<FML_WaveChange>& OutChanges)
{
    if (!OriginTile) return;

    AML_BoardSpawner* Board = OriginTile->GetBoardSpawnerFromTile();
    if (!Board) return;

    const TArray<AML_Tile*>& GridTiles = Board->GetGridTiles();
    TSet<AML_Tile*> Scheduled;
    TSet<AML_Tile*> WaterConnected;
    TArray<AML_Tile*> GrassSources;

    // Pre-allocation to improve performance
    Scheduled.Reserve(GridTiles.Num() / 4); // Estimate
    WaterConnected.Reserve(GridTiles.Num() / 8); // Estimate
    OutChanges.Reserve(GridTiles.Num() / 4); // Estimate

    // -------------------------------------------------
    // CASE 1: FIRST WAVE (Origin = Dirt)
    // -------------------------------------------------
    if (OriginTile->GetCurrentType() == EML_TileType::Dirt)
    {
        OutChanges.Add(FML_WaveChange(OriginTile, EML_TileType::Grass, 0));
        Scheduled.Add(OriginTile);
        GrassSources.Add(OriginTile);

        ExpandWaterNetwork(Board, OriginTile, WaterConnected);
    }
    else
    {
        // -------------------------------------------------
        // CASE 2: RETURN AFTER WATER / PARASITE
        // -------------------------------------------------
        
        // Collect all existing grass tiles
        for (AML_Tile* Tile : GridTiles)
        {
            if (Tile && Tile->GetCurrentType() == EML_TileType::Grass)
            {
                GrassSources.Add(Tile);
                Scheduled.Add(Tile);
            }
        }

        if (GrassSources.Num() == 0) return;

        // Expansion of the water network for all grass sources
        for (AML_Tile* Source : GrassSources)
        {
            ExpandWaterNetwork(Board, Source, WaterConnected);
        }
    }

    // -------------------------------------------------
    // COMPUTE DISTANCES FROM ORIGIN TO ALL TILES
    // -------------------------------------------------
    
    // Multi-source BFS: calculate the shortest distance from any grass source to every tile.
    // In CASE 1 (first wave), there's only one source (OriginTile).
    // In CASE 2 (recovery after water/parasite), all existing grass tiles act as sources.
    // This ensures we get the minimum distance to the nearest grass tile.
    
    TMap<AML_Tile*, int32> DistanceToGrass;
    DistanceToGrass.Reserve(GridTiles.Num());

    TQueue<TPair<AML_Tile*, int32>> DistanceQueue;
    FML_TileNeighbors Neighbors;

    for (AML_Tile* Source : GrassSources)
    {
        DistanceToGrass.Add(Source, 0);
        DistanceQueue.Enqueue({ Source, 0 });
    }

    while (!DistanceQueue.IsEmpty())
    {
        TPair<AML_Tile*, int32> Current;
        DistanceQueue.Dequeue(Current);

        Board->GetNeighbors(Current.Key, Neighbors);
        const int32 NextDist = Current.Value + 1;

        for (AML_Tile* Neighbor : Neighbors)
        {
            if (Neighbor && !DistanceToGrass.Contains(Neighbor))
            {
                DistanceToGrass.Add(Neighbor, NextDist);
                DistanceQueue.Enqueue({ Neighbor, NextDist });
            }
        }
    }

    // -------------------------------------------------
    // COMPLETE BFS PROPAGATION
    // -------------------------------------------------

    TQueue<TPair<AML_Tile*, int32>> PropagationQueue;

    // Checking neighbors' access to all water tiles in the network
    for (AML_Tile* WaterTile : WaterConnected)
    {
        Board->GetNeighbors(WaterTile, Neighbors);

        for (AML_Tile* Neighbor : Neighbors)
        {
            if (!Neighbor || Scheduled.Contains(Neighbor) || !UML_TileTypeTraits::CanGrassPropagateTo(Neighbor->GetCurrentType()))
                continue;

            // Retrieving the actual distance from the origin
            const int32* FoundDistance = DistanceToGrass.Find(Neighbor);
            const int32 Distance = FoundDistance ? *FoundDistance : 1;
            
            PropagationQueue.Enqueue({ Neighbor, Distance });
            Scheduled.Add(Neighbor);
        }
    }

    while (!PropagationQueue.IsEmpty())
    {
        TPair<AML_Tile*, int32> Current;
        PropagationQueue.Dequeue(Current);

        AML_Tile* CurrentTile = Current.Key;
        const int32 StepDistance = Current.Value;

        if (CurrentTile->GetCurrentType() == EML_TileType::Dirt)
        {
            OutChanges.Add(FML_WaveChange(CurrentTile, EML_TileType::Grass, StepDistance));
            ExpandWaterNetwork(Board, CurrentTile, WaterConnected);
        }

        Board->GetNeighbors(CurrentTile, Neighbors);

        for (AML_Tile* Neighbor : Neighbors)
        {
            if (!Neighbor || Scheduled.Contains(Neighbor) || !UML_TileTypeTraits::CanGrassPropagateTo(Neighbor->GetCurrentType()))
                continue;

            // Checking if the neighbor is touching the water
            bool bTouchesWater = false;
            FML_TileNeighbors AroundNeighbors;
            Board->GetNeighbors(Neighbor, AroundNeighbors);

            for (AML_Tile* Around : AroundNeighbors)
            {
                if (Around && WaterConnected.Contains(Around))
                {
                    bTouchesWater = true;
                    break;
                }
            }

            if (bTouchesWater)
            {
                // Retrieving the actual distance from the origin
                const int32* FoundDistance = DistanceToGrass.Find(Neighbor);
                const int32 Distance = FoundDistance ? *FoundDistance : StepDistance + 1;
                
                PropagationQueue.Enqueue({ Neighbor, Distance });
                Scheduled.Add(Neighbor);
            }
        }
    }

    // Sort by distance (stable: keeps insertion order inside a ring, deterministic for undo)
    OutChanges.StableSort([](const FML_WaveChange& A, const FML_WaveChange& B)
    {
        return A.DistanceFromOrigin < B.DistanceFromOrigin;
    });
}