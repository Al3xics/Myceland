// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tiles/ML_Tile.h"
#include "ML_UndoTypes.generated.h"

USTRUCT()
struct FML_TileUndoDelta
{
	GENERATED_BODY()

	UPROPERTY() TWeakObjectPtr<class AML_Tile> Tile;
	UPROPERTY() EML_TileType OldType = EML_TileType::Dirt;

	UPROPERTY() bool bOldHasCollectible = false;
	UPROPERTY() bool bOldConsumedGrass = false;

	// ordering
	UPROPERTY() int32 PriorityIndex = 0;
	UPROPERTY() int32 DistanceFromOrigin = 0;
	UPROPERTY() int32 Sequence = 0;
};

USTRUCT()
struct FML_SpawnUndoDelta
{
	GENERATED_BODY()

	UPROPERTY() TWeakObjectPtr<AActor> SpawnedActor;

	// ordering
	UPROPERTY() int32 PriorityIndex = 0;
	UPROPERTY() int32 DistanceFromOrigin = 0;
	UPROPERTY() int32 Sequence = 0;
};

USTRUCT()
struct FML_TurnUndoRecord
{
	GENERATED_BODY()

	UPROPERTY() int32 EnergyBefore = 0;

	UPROPERTY() FIntPoint PlayerAxialBefore = FIntPoint::ZeroValue;
	UPROPERTY() FVector  PlayerWorldBefore = FVector::ZeroVector;

	UPROPERTY() TWeakObjectPtr<class AML_Tile> OriginTile;

	UPROPERTY() TArray<FML_TileUndoDelta> TileDeltas;
	UPROPERTY() TArray<FML_SpawnUndoDelta> SpawnDeltas;
};

UENUM(BlueprintType)
enum class EML_UndoActionType : uint8
{
	Move,
	PlantWaves
};

USTRUCT()
struct FML_MoveUndoRecord
{
	GENERATED_BODY()

	UPROPERTY() FIntPoint StartAxial = FIntPoint::ZeroValue;
	UPROPERTY() FIntPoint EndAxial   = FIntPoint::ZeroValue;

	// Exact path used for deterministic replay
	UPROPERTY() TArray<FIntPoint> AxialPath;

	UPROPERTY() FVector StartWorld = FVector::ZeroVector;
	UPROPERTY() FVector EndWorld   = FVector::ZeroVector;

	// Collectibles picked during this move
	UPROPERTY() TArray<FIntPoint> PickedCollectibleAxials;
};

USTRUCT()
struct FML_ActionUndoRecord
{
	GENERATED_BODY()

	UPROPERTY() EML_UndoActionType Type = EML_UndoActionType::Move;

	UPROPERTY() FML_MoveUndoRecord Move;
	UPROPERTY() FML_TurnUndoRecord Turn;
};