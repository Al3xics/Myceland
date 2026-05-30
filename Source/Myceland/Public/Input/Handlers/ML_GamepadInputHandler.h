// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/ML_InputHandlerBase.h"
#include "ML_GamepadInputHandler.generated.h"

class AML_Tile;
class AML_BoardSpawner;

/**
 * Handles gamepad input (analog sticks and buttons).
 *
 * Free movement : left stick continuously moves the character in the stick direction.
 *                 Board entry is automatic when the character physically walks onto a tile.
 *
 * Inside board  : one full neutral-to-pushed transition selects the neighbor tile closest
 *                 to the stick direction and navigates to it.
 *                 Pushing toward the board edge (no walkable neighbor) starts the exit hold.
 *                 Stick returning to neutral (IA_GamepadMove Completed) resets the gate and
 *                 cancels any in-progress exit hold.
 *
 * Move          : moves to FocusedTile — the last tile selected by stick navigation.
 *
 * Move & plant  : plants on FocusedTile — the last tile selected by stick navigation,
 *                 or the character's current tile if the stick has not moved yet.
 */
UCLASS(ClassGroup=(Myceland), meta=(BlueprintSpawnableComponent))
class MYCELAND_API UML_GamepadInputHandler : public UML_InputHandlerBase
{
	GENERATED_BODY()

private:
	/** True if the stick has returned to the neutral zone since the last directional input. */
	bool bStickWasNeutral = true;

	/**
	 * The last tile explicitly selected via stick navigation.
	 * Used as the target for OnMoveActionStarted and OnMoveAndPlantAction.
	 * Falls back to CurrentTileOn when no navigation has occurred yet.
	 */
	UPROPERTY()
	AML_Tile* FocusedTile = nullptr;

	void HandleFreeMovementStick(FVector2D StickValue, float DeltaTime);
	void HandleInsideBoardStick(FVector2D StickValue);

	/**
	 * Converts a 2D stick value to a world-space direction using the controller's
	 * yaw rotation. Stick X maps to world right, stick Y maps to world forward.
	 */
	FVector StickToWorldDirection(FVector2D StickValue) const;

	/**
	 * Finds the neighbor tile most aligned with the stick direction (walkable or not).
	 * Only truly missing board slots (null) are ignored.
	 * Returns nullptr if no neighbor clears AlignmentThreshold (i.e. the stick points
	 * into empty space — no board tile exists in that direction).
	 * Default threshold is cos 60° (0.5); pass cos 30° (0.866) for stricter selection.
	 */
	AML_Tile* FindNeighborInStickDirection(FVector2D StickValue, float AlignmentThreshold = 0.5f) const;

	/**
	 * Returns true if the origin tile (FocusedTile when valid, otherwise CurrentTileOn) is
	 * walkable and a border tile. Mirrors the mouse condition: any walkable border tile is a
	 * valid exit point, no need to be near a specific WaterPath tile.
	 */
	bool IsOriginTileWalkableBorderTile(const AML_BoardSpawner* Board) const;

public:
	virtual void OnActivated() override;
	virtual void OnDeactivated() override;
	virtual void OnMoveActionStarted() override;
	virtual void OnStickAxis(FVector2D StickValue, float DeltaTime) override;
	virtual void OnStickReleased() override;
	virtual void OnMoveAndPlantAction() override;

	/** Movement speed scale forwarded to AddMovementInput during free movement. */
	UPROPERTY(EditAnywhere, Category = "Myceland")
	float FreeMovementScale = 1.f;
};
