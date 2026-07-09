// Copyright Myceland Team, All Rights Reserved.


#include "Waves/ChildWaves/ML_WaveCollectible.h"

#include "Core/ML_CoreData.h"
#include "Core/ML_TileTypeTraits.h"
#include "Data Asset/ML_BiomeTileSet.h"
#include "Developer Settings/ML_MycelandDeveloperSettings.h"
#include "Subsystem/ML_RollBackSubsystem.h"
#include "Subsystem/ML_WavePropagationSubsystem.h"
#include "Tiles/ML_Tile.h"

void UML_WaveCollectible::ComputeWaveForCollectibles(AML_Tile* OriginTile, const TArray<AML_Tile*>& ParasitesThatAteGrass, TArray<FML_WaveChange>& OutChanges)
{
    if (!OriginTile || ParasitesThatAteGrass.Num() == 0) return;

    AML_BoardSpawner* Board = OriginTile->GetBoardSpawnerFromTile();
    ensureMsgf(Board, TEXT("Board is not set!"));
    if (!Board) return;

    TSet<AML_Tile*> ParasiteSet(ParasitesThatAteGrass);

    TQueue<TPair<AML_Tile*, int32>> Queue;
    TSet<AML_Tile*> Visited;

    Queue.Enqueue({ OriginTile, 0 });
    Visited.Add(OriginTile);

    while (!Queue.IsEmpty())
    {
        TPair<AML_Tile*, int32> Current;
        Queue.Dequeue(Current);

        AML_Tile* Tile = Current.Key;
        int32 Distance = Current.Value;

        if (!Tile) continue;

        FML_TileNeighbors Neighbors;
        Board->GetNeighbors(Tile, Neighbors);
        for (AML_Tile* Neighbor : Neighbors)
        {
            if (!Neighbor || Visited.Contains(Neighbor))
                continue;

            Visited.Add(Neighbor);

            // Continue propagation everywhere (like other waves)
            Queue.Enqueue({ Neighbor, Distance + 1 });

            // Spawn condition
            if (!Neighbor->HasCollectible() &&
                UML_TileTypeTraits::CanSpawnCollectible(Neighbor->GetCurrentType()))
            {
                // Check if this tile is near a parasite that has eaten
                FML_TileNeighbors CheckNeighbors;
                Board->GetNeighbors(Neighbor, CheckNeighbors);
                for (AML_Tile* CheckTile : CheckNeighbors)
                {
                    if (ParasiteSet.Contains(CheckTile))
                    {
                        FML_WaveChange Change;
                        Change.Neighbor = Neighbor;
                        Change.SourceParasite = CheckTile;
                        Change.SpawnLocation = Neighbor->GetActorLocation();
                        Change.CollectibleClass = Board->GetBiomeTileSet()->GetCollectibleClass();
                        Change.DistanceFromOrigin = Distance + 1;

                        OutChanges.Add(Change);

                        if (UWorld* World = OriginTile->GetWorld())
                        {
                            if (UML_RollBackSubsystem* RollBackSubsystem = World->GetSubsystem<UML_RollBackSubsystem>())
                            {
                                int32 PriorityIndex = 0;
                                if (UML_WavePropagationSubsystem* WaveSubsystem = World->GetSubsystem<UML_WavePropagationSubsystem>())
                                {
                                    PriorityIndex = WaveSubsystem->GetCurrentPriorityIndexForRecording();
                                }

                                RollBackSubsystem->RecordTileForUndo(Neighbor, Distance + 1, PriorityIndex);
                            }
                        }
                        
                        Neighbor->SetHasCollectible(true);
                        break;
                    }
                }
            }
        }
    }
}
