// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/ML_InputHandlerBase.h"
#include "ML_GamepadInputHandler.generated.h"

class UML_GameUserSettings;
class UML_MycelandDeveloperSettings;
class AML_Tile;
class AML_BoardSpawner;

/**
 * Handles gamepad input (analog sticks and buttons).
 *
 * Free movement : left stick continuously moves the character in the stick direction.
 *                 Board entry is automatic when the character physically walks onto a tile.
 *
 * Inside board  : LEFT STICK moves the player tile-by-tile — pushing the stick walks the character
 *                 to the neighbor tile most aligned with the direction; holding keeps stepping to the
 *                 next tile (GamepadHoldRepeatDelay / GamepadHoldRepeatInterval).
 *                 RIGHT STICK moves a selection cursor over the plantable tiles, stepping one tile at a
 *                 time from the currently selected tile toward the stick direction (throttled by
 *                 GamepadHoldRepeatInterval while held; a flick steps once). The cursor stays within
 *                 GamepadSelectRingDistance of the player and highlights like the mouse cursor hover. The
 *                 plant button then plants the selected tile (classic turn-to-plant + propagation).
 *                 EXIT: there is no exit button — standing on an exit border tile, pushing the LEFT STICK
 *                 toward the exit plane walks the player straight out of the board. Mirrors the mouse exit
 *                 hold: instant when ExitBoardHoldDurationGamepad is 0, otherwise the stick must stay pointed at
 *                 the exit for the whole duration (releasing / steering away cancels).
 */
UCLASS(ClassGroup=(Myceland), meta=(BlueprintSpawnableComponent))
class MYCELAND_API UML_GamepadInputHandler : public UML_InputHandlerBase
{
	GENERATED_BODY()

private:
	UPROPERTY()
	UML_GameUserSettings* Settings = nullptr;

	UPROPERTY()
	const UML_MycelandDeveloperSettings* DevSettings = nullptr;

	// ===== Left stick: tile-by-tile movement =====

	/** True while the left stick has been in the neutral zone since the last directional step. */
	bool bMoveStickWasNeutral = true;

	/** Seconds the left stick has been held away from neutral since the last step. */
	float MoveStickHeldTime = 0.f;

	/** Accumulator counting up to the repeat interval while auto-stepping is active. */
	float MoveRepeatTimer = 0.f;

	/** True once the hold delay elapsed and the player auto-steps to the next tile while held. */
	bool bMoveAutoRepeatActive = false;

	// ==================== Movement (left stick) ====================

	void HandleFreeMovementStick(FVector2D StickValue);
	void HandleInsideBoardMoveStick(FVector2D StickValue, float DeltaTime);

	/**
	 * Called each frame while the exit hold is in progress (ExitingBoard mode). Keeps the hold alive
	 * while the stick stays pointed at the exit plane; cancels it if the stick steers away from the exit.
	 */
	void HandleExitingBoardStick(FVector2D StickValue);

	/** Steps the player one tile toward the left-stick direction, or starts the board exit when the
	 *  stick points at the current tile's exit plane (see EvaluateStickExit). */
	void StepMoveInStickDirection(FVector2D StickValue);

	/** Resets the left-stick hold/repeat state so the next push starts a fresh step cycle. */
	void ResetMoveRepeatState();

	// ===== Right stick: plant selection =====

	/** True while the right stick has been in the neutral zone since the last selection update. */
	bool bSelectStickWasNeutral = true;

	/** Accumulator gating held re-selection to one update per GamepadHoldRepeatInterval. */
	float SelectRepeatTimer = 0.f;

	// ==================== Plant selection (right stick) ====================

	/**
	 * Resolves the plantable neighbor most aligned with the right-stick direction and forwards it to the
	 * HoverPreviewComponent (which owns the selection + all board visuals). Selection state lives there.
	 */
	void UpdatePlantSelection(FVector2D StickValue);

	// ==================== Helpers ====================

	/**
	 * Converts a 2D stick value to a world-space direction using the camera yaw.
	 * Stick X maps to world right, stick Y maps to world forward.
	 */
	FVector StickToWorldDirection(FVector2D StickValue) const;

	/**
	 * Finds the tile most aligned with the stick direction among those at hex ring RingDistance from the
	 * player's current tile (RingDistance 1 = the immediate neighbors).
	 * bPlantableOnly true  → only considers tiles the player can plant on (right-stick selection).
	 * bPlantableOnly false → only considers walkable tiles (left-stick movement).
	 * Returns nullptr if no candidate clears AlignmentThreshold. When OutBestDot is provided it receives
	 * the alignment dot of the returned tile (or -1 when none clears the threshold), so callers can
	 * compare it against the exit direction.
	 */
	AML_Tile* FindNeighborInStickDirection(FVector2D StickValue, float AlignmentThreshold, bool bPlantableOnly, int32 RingDistance = 1, float* OutBestDot = nullptr) const;

	/** Hex (axial) distance between two axial coordinates. */
	static int32 HexAxialDistance(const FIntPoint& A, const FIntPoint& B);

	/**
	 * If the player stands on an exit border tile, computes how well the stick points at that exit's plane.
	 * OutExitDot receives the alignment dot [-1,1], OutExitTarget the plane's world location.
	 * Returns false (leaving the outputs untouched) when the player is not on a valid exit tile.
	 */
	bool EvaluateStickExit(FVector2D StickValue, float& OutExitDot, FVector& OutExitTarget) const;

public:
	virtual void OnActivated() override;
	virtual void OnDeactivated() override;
	virtual void OnStickAxis(FVector2D StickValue, float DeltaTime) override;
	virtual void OnStickReleased() override;
	virtual void OnPlantSelectAxis(FVector2D StickValue, float DeltaTime) override;
	virtual void OnPlantSelectReleased() override;
	virtual void OnMoveAndPlantAction() override;

	/** Movement speed scale forwarded to AddMovementInput during free movement. */
	UPROPERTY(EditAnywhere, Category = "Myceland")
	float FreeMovementScale = 1.f;
};
