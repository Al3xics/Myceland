// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "Tiles/ML_Tile.h"
#include "ML_UndoTypes.generated.h"

USTRUCT()
struct FML_TileUndoDelta
{
	GENERATED_BODY()

	UPROPERTY() TWeakObjectPtr<class AML_Tile> Tile;
	UPROPERTY() EML_TileType OldType = EML_TileType::Dirt;

	UPROPERTY() bool bOldHasCollectible = false;

	// State you already touch in your wave code
	UPROPERTY() bool bOldConsumedGrass = false;

	// NEW ordering
	UPROPERTY() int32 PriorityIndex = 0;        // which wave logic in DevSettings->WavesPriority
	UPROPERTY() int32 DistanceFromOrigin = 0;   // same as WaveChange.DistanceFromOrigin
	UPROPERTY() int32 Sequence = 0;             // stable ordering
};

USTRUCT()
struct FML_SpawnUndoDelta
{
	GENERATED_BODY()

	UPROPERTY() TWeakObjectPtr<AActor> SpawnedActor;

	UPROPERTY() bool bOldHasCollectible = false;

	// NEW ordering
	UPROPERTY() int32 PriorityIndex = 0;
	UPROPERTY() int32 DistanceFromOrigin = 0;
	UPROPERTY() int32 Sequence = 0;
};

USTRUCT()
struct FML_TurnUndoRecord
{
	GENERATED_BODY()

	// Energy snapshot (Undo restores this)
	UPROPERTY() int32 EnergyBefore = 0;

	// Player snapshot (Undo restores player position by path or teleport)
	UPROPERTY() FIntPoint PlayerAxialBefore = FIntPoint::ZeroValue;
	UPROPERTY() FVector PlayerWorldBefore = FVector::ZeroVector;

	// What tile started the propagation (optional but useful)
	UPROPERTY() TWeakObjectPtr<class AML_Tile> OriginTile;

	// Deltas
	UPROPERTY() TArray<FML_TileUndoDelta> TileDeltas;
	UPROPERTY() TArray<FML_SpawnUndoDelta> SpawnDeltas;
};