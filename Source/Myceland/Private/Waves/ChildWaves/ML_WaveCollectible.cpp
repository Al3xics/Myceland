// Copyright Myceland Team, All Rights Reserved.


#include "Waves/ChildWaves/ML_WaveCollectible.h"

#include "Core/ML_CoreData.h"
#include "Data Asset/ML_BiomeTileSet.h"
#include "Developer Settings/ML_MycelandDeveloperSettings.h"
#include "Subsystem/ML_WavePropagationSubsystem.h"
#include "Tiles/ML_Tile.h"

void UML_WaveCollectible::ComputeWaveForCollectibles(AML_Tile* OriginTile, const TArray<AML_Tile*>& ParasitesThatAteGrass, TArray<FML_WaveChange>& OutChanges)
{
    GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Yellow, TEXT("Collectible Wave"));
    
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

        TArray<AML_Tile*> Neighbors = Board->GetNeighbors(Tile);
        for (AML_Tile* Neighbor : Neighbors)
        {
            if (!Neighbor || Visited.Contains(Neighbor))
                continue;

            Visited.Add(Neighbor);

            // Continue propagation everywhere (like other waves)
            Queue.Enqueue({ Neighbor, Distance + 1 });

            // Spawn condition
            if (!Neighbor->HasCollectible() &&
                (Neighbor->GetCurrentType() == EML_TileType::Dirt ||
                 Neighbor->GetCurrentType() == EML_TileType::Grass))
            {
                // Check if this tile is near a parasite that has eaten
                TArray<AML_Tile*> CheckNeighbors = Board->GetNeighbors(Neighbor);
                for (AML_Tile* CheckTile : CheckNeighbors)
                {
                    if (ParasiteSet.Contains(CheckTile))
                    {
                        FML_WaveChange Change;
                        Change.Neighbor = Neighbor;
                        Change.SpawnLocation = Neighbor->GetActorLocation();
                        Change.CollectibleClass = Board->GetBiomeTileSet()->GetCollectibleClass();
                        Change.DistanceFromOrigin = Distance + 1;

                        OutChanges.Add(Change);

                        // NEW: record undo snapshot before flipping the flag
                        if (UWorld* World = OriginTile->GetWorld())
                        {
                            if (UML_WavePropagationSubsystem* S = World->GetSubsystem<UML_WavePropagationSubsystem>())
                            {
                                S->RecordTileForUndo(Neighbor, Distance + 1);
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
