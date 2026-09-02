// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ML_WaterNavPath.generated.h"

/**
 * Pre-placed water-bridge actor associated with a puzzle board via
 * AML_BoardSpawner::AssociatedWaterPaths. It starts hidden with collision off;
 * winning the puzzle calls Spawn() to reveal the mesh and enable collision.
 *
 * The Blueprint 'WaterNavPath' should be reparented to this class so that the
 * save system can replay Spawn() on load when the puzzle is already solved.
 */
UCLASS()
class MYCELAND_API AML_WaterNavPath : public AActor
{
	GENERATED_BODY()

public:
	// Reveals the bridge (visibility + collision). Implemented in Blueprint by the
	// existing 'Spawn' event. Called on win, and replayed on load for solved puzzles.
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Water Nav Path")
	void Spawn();

	// Inverse of Spawn: hides every mesh and disables its collision. Implemented in
	// C++ (does not rely on a Blueprint graph). Called when the puzzle is replayed/
	// reset back to its initial state.
	UFUNCTION(BlueprintCallable, Category = "Water Nav Path")
	void Despawn();
};