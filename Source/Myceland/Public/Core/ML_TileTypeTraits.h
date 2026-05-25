// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Core/ML_CoreData.h"
#include "ML_TileTypeTraits.generated.h"

/**
 * Centralized utility class for tile type properties and traits.
 * All type checking logic goes through here to maintain a single source of truth.
 */
UCLASS()
class MYCELAND_API UML_TileTypeTraits : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Returns true if the player can walk on this tile type. */
	UFUNCTION(BlueprintPure, Category = "Myceland|Tile")
	static bool IsWalkable(EML_TileType Type);

	/** Returns true if this tile type blocks movement/collision. */
	UFUNCTION(BlueprintPure, Category = "Myceland|Tile")
	static bool IsBlocking(EML_TileType Type);

	/** Returns true if Parasite wave can propagate to this tile type. */
	UFUNCTION(BlueprintPure, Category = "Myceland|Tile")
	static bool CanParasitePropagateTo(EML_TileType Type);

	/** Returns true if Grass wave can propagate through this tile type. */
	UFUNCTION(BlueprintPure, Category = "Myceland|Tile")
	static bool CanGrassPropagateTo(EML_TileType Type);

	/** Returns true if this tile type will actually convert to Grass (not just propagate through). */
	UFUNCTION(BlueprintPure, Category = "Myceland|Tile")
	static bool IsGrassConvertible(EML_TileType Type);

	/** Returns true if Water wave can propagate to this tile type. */
	UFUNCTION(BlueprintPure, Category = "Myceland|Tile")
	static bool CanWaterPropagateTo(EML_TileType Type);

	/** Returns true if a collectible can spawn on this tile type. */
	UFUNCTION(BlueprintPure, Category = "Myceland|Tile")
	static bool CanSpawnCollectible(EML_TileType Type);

	/** Returns true if the player can plant on this tile type. */
	UFUNCTION(BlueprintPure, Category = "Myceland|Tile")
	static bool CanPlayerPlant(EML_TileType Type);

	/** Returns true if win propagation will convert this tile type. */
	UFUNCTION(BlueprintPure, Category = "Myceland|Tile")
	static bool IsWinPropagationConvertible(EML_TileType Type);

	/** Returns true if stepping on this tile kills the player. */
	UFUNCTION(BlueprintPure, Category = "Myceland|Tile")
	static bool IsLethalTile(EML_TileType Type);

	/** Returns true if this tile type is valid for win path detection (BFS from entry to exit). */
	UFUNCTION(BlueprintPure, Category = "Myceland|Tile")
	static bool IsWinPathTile(EML_TileType Type);

	/** Returns true if this is a Water tile (original water, not converted). */
	UFUNCTION(BlueprintPure, Category = "Myceland|Tile")
	static bool IsWaterType(EML_TileType Type);

	/** Returns true if this is a Parasite tile. */
	UFUNCTION(BlueprintPure, Category = "Myceland|Tile")
	static bool IsParasiteType(EML_TileType Type);

	/** Returns true if this is a Tree tile (goal). */
	UFUNCTION(BlueprintPure, Category = "Myceland|Tile")
	static bool IsTreeType(EML_TileType Type);
};
