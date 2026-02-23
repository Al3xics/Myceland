// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ML_BridgeConstructorSubsystem.generated.h"

class AML_BoardSpawner;
class AML_Tile;

UCLASS()
class MYCELAND_API UML_BridgeConstructorSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * Returns true if there is a walkable path from startTile to exitTile.
	 * Walkable tiles are those with type Dirt or Grass (and optionally not blocked).
	 */
	UFUNCTION(BlueprintCallable, Category="Myceland|BridgeConstructor")
	bool CheckPathToExit(const FIntPoint& StartTile, const FIntPoint& ExitTile, const AML_BoardSpawner* BoardSpawner) const;

private:
	// Hex axial directions (q,r)
	static const FIntPoint AxialDirections[6];

	bool IsTileWalkable(const AML_Tile* Tile) const;
};