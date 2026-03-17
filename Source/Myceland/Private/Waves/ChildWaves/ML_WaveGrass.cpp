// Copyright Myceland Team, All Rights Reserved.


#include "Waves/ChildWaves/ML_WaveGrass.h"

#include "Tiles/ML_BoardSpawner.h"
#include "Core/ML_CoreData.h"
#include "Tiles/ML_Tile.h"

void UML_WaveGrass::ExpandWaterNetwork(AML_BoardSpawner* Board, AML_Tile* FromTile, TSet<AML_Tile*>& WaterConnected)
{
    TQueue<AML_Tile*> ExpansionQueue;

    for (AML_Tile* Neighbor : Board->GetNeighbors(FromTile))
    {
        if (Neighbor && Neighbor->GetCurrentType() == EML_TileType::Water && !WaterConnected.Contains(Neighbor))
        {
            WaterConnected.Add(Neighbor);
            ExpansionQueue.Enqueue(Neighbor);
        }
    }

    while (!ExpansionQueue.IsEmpty())
    {
        AML_Tile* CurrentWater;
        ExpansionQueue.Dequeue(CurrentWater);

        for (AML_Tile* Neighbor : Board->GetNeighbors(CurrentWater))
        {
            if (Neighbor && Neighbor->GetCurrentType() == EML_TileType::Water && !WaterConnected.Contains(Neighbor))
            {
                WaterConnected.Add(Neighbor);
                ExpansionQueue.Enqueue(Neighbor);
            }
        }
    }
}

bool UML_WaveGrass::IsDirtLike(const AML_Tile* Tile)
{
    return Tile->GetCurrentType() == EML_TileType::Dirt || Tile->GetCurrentType() == EML_TileType::Obstacle;
}

void UML_WaveGrass::ComputeWave(AML_Tile* OriginTile, TArray<FML_WaveChange>& OutChanges)
{
    GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Green, TEXT("Grass Wave"));
    
     if (!OriginTile) return;

    AML_BoardSpawner* Board = OriginTile->GetBoardSpawnerFromTile();
    if (!Board) return;

    TSet<AML_Tile*> Scheduled;
    TSet<AML_Tile*> WaterConnected;
    TArray<AML_Tile*> GrassSources;

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

        // We take all existing Grasses
        for (AML_Tile* Tile : Board->GetGridTiles())
        {
            if (!Tile) continue;

            if (Tile->GetCurrentType() == EML_TileType::Grass)
            {
                GrassSources.Add(Tile);
                ExpandWaterNetwork(Board, Tile, WaterConnected);
                Scheduled.Add(Tile);
            }
        }

        if (GrassSources.Num() == 0)
            return;
    }

    // -------------------------------------------------
    // COMPLETE BFS PROPAGATION (STEP-BY-STEP via Distance)
    // -------------------------------------------------

    TQueue<TPair<AML_Tile*, int32>> PropagationQueue;

    // Initialization
    for (AML_Tile* Source : GrassSources)
    {
        for (AML_Tile* Neighbor : Board->GetNeighbors(Source))
        {
            if (!Neighbor || Scheduled.Contains(Neighbor))
                continue;

            if (!IsDirtLike(Neighbor))
                continue;

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
                PropagationQueue.Enqueue({ Neighbor, 1 });
                Scheduled.Add(Neighbor);
            }
        }
    }

    while (!PropagationQueue.IsEmpty())
    {
        TPair<AML_Tile*, int32> Current;
        PropagationQueue.Dequeue(Current);

        AML_Tile* CurrentTile = Current.Key;
        int32 StepDistance = Current.Value;

        if (CurrentTile->GetCurrentType() == EML_TileType::Dirt)
        {
            OutChanges.Add(FML_WaveChange(CurrentTile, EML_TileType::Grass, StepDistance));
            ExpandWaterNetwork(Board, CurrentTile, WaterConnected);
        }

        for (AML_Tile* Neighbor : Board->GetNeighbors(CurrentTile))
        {
            if (!Neighbor || Scheduled.Contains(Neighbor))
                continue;

            if (!IsDirtLike(Neighbor))
                continue;

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

    // We sort by distance to ensure the correct order
    OutChanges.Sort([](const FML_WaveChange& A, const FML_WaveChange& B)
    {
        return A.DistanceFromOrigin < B.DistanceFromOrigin;
    });
}
