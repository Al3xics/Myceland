// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ML_CheatTeleportPoint.generated.h"

class UArrowComponent;

/**
 * Demo-only marker: a destination the cheat mode can teleport the player to.
 * Place one per interesting spot of a level, on the ground (the player is spawned one capsule
 * height above the actor's origin), and give it the Slot number of the key that should reach it.
 *
 * These are also the fallback destinations of the "unstuck" cheat, which teleports the player to
 * the closest point. Points are therefore meant to sit OUTSIDE any board footprint: dropping the
 * player on board tiles makes them enter that board, which is rarely what an unstuck is for.
 */
UCLASS(Blueprintable)
class MYCELAND_API AML_CheatTeleportPoint : public AActor
{
	GENERATED_BODY()

public:
	AML_CheatTeleportPoint();

	// Key number that teleports here in cheat mode (1 = the "1" key, ... 9 = the "9" key).
	// Two points sharing a slot: the first one found wins and a warning is logged.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "01 Cheat Teleport Point", meta = (ClampMin = "1", ClampMax = "9"))
	int32 Slot = 1;

	// Name shown next to the key in the cheat overlay. Falls back to the actor label when empty.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "01 Cheat Teleport Point")
	FText DisplayName;

	// Label the overlay shows for this point (DisplayName, or the actor name as a fallback).
	FText GetDisplayLabel() const;

private:
	// Editor-only visual so the point can be seen and oriented in the viewport.
	UPROPERTY(VisibleAnywhere, Category = "Components")
	UArrowComponent* ArrowComponent;
};
