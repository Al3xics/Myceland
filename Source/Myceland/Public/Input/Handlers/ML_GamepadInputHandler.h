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
 *                 RIGHT STICK selects a plantable tile around the player (only among the highlighted
 *                 plantable neighbors), highlighting it like the mouse cursor hover. The plant button
 *                 then plants the selected tile (classic turn-to-plant + propagation).
 *                 EXIT: walk onto an exit tile — the board broadcasts the exit plane(s) to highlight;
 *                 pressing the exit input starts the exit hold (instant when ExitBoardHoldDuration is 0).
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

	/** Steps the player one tile toward the left-stick direction (walks to the aligned neighbor). */
	void StepMoveInStickDirection(FVector2D StickValue);

	/** Resets the left-stick hold/repeat state so the next push starts a fresh step cycle. */
	void ResetMoveRepeatState();

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
	 * Finds the neighbor tile of the player's current tile most aligned with the stick direction.
	 * bPlantableOnly true  → only considers tiles the player can plant on (right-stick selection).
	 * bPlantableOnly false → only considers walkable tiles (left-stick movement).
	 * Returns nullptr if no candidate clears AlignmentThreshold.
	 */
	AML_Tile* FindNeighborInStickDirection(FVector2D StickValue, float AlignmentThreshold, bool bPlantableOnly) const;

public:
	virtual void OnActivated() override;
	virtual void OnDeactivated() override;
	virtual void OnStickAxis(FVector2D StickValue, float DeltaTime) override;
	virtual void OnStickReleased() override;
	virtual void OnPlantSelectAxis(FVector2D StickValue, float DeltaTime) override;
	virtual void OnPlantSelectReleased() override;
	virtual void OnMoveAndPlantAction() override;
	virtual void OnExitAction() override;
	virtual void OnExitReleased() override;

	/** Movement speed scale forwarded to AddMovementInput during free movement. */
	UPROPERTY(EditAnywhere, Category = "Myceland")
	float FreeMovementScale = 1.f;
};
