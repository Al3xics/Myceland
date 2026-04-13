// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ML_HexPathfinder.generated.h"

class AML_Tile;

/**
 * Stateless hex-grid pathfinding utilities.
 * All methods are static — no instance needed.
 */
UCLASS()
class MYCELAND_API UML_HexPathfinder : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/** Returns true if the tile can be walked on (Dirt or Grass and not blocked). */
	static bool IsTileWalkable(const AML_Tile* Tile);

	/**
	 * BFS on a hex axial grid. Fills OutAxialPath with the shortest walkable path
	 * from StartAxial to GoalAxial. Returns true if a path was found.
	 */
	static bool BuildPath_AxialBFS(const FIntPoint& StartAxial, const FIntPoint& GoalAxial,
	                               const TMap<FIntPoint, AML_Tile*>& GridMap,
	                               TArray<FIntPoint>& OutAxialPath);

	/**
	 * Returns the nearest walkable tile in GridMap to WorldLocation (2D distance),
	 * or nullptr if none exists.
	 */
	static AML_Tile* FindNearestWalkableTile(const FVector& WorldLocation,
	                                          const TMap<FIntPoint, AML_Tile*>& GridMap);
};
