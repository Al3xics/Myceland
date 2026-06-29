// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/ML_InputHandlerBase.h"
#include "ML_MouseKeyboardInputHandler.generated.h"

class AML_Tile;

/**
 * Handles all mouse and keyboard input.
 * Contains the cursor-based movement logic that was previously in ML_PlayerController.
 */
UCLASS(ClassGroup=(Myceland), meta=(BlueprintSpawnableComponent))
class MYCELAND_API UML_MouseKeyboardInputHandler : public UML_InputHandlerBase
{
	GENERATED_BODY()

private:
	/** Accumulated hold time — drives short-tap vs hold detection in free movement. */
	float FollowTime = 0.f;

	/** World position under cursor, cached every frame during free movement hold. */
	FVector HoldMoveCachedDestination = FVector::ZeroVector;

	/** Set when Started detects a non-walkable tile; suppresses Triggered and Released for this click cycle. */
	bool bIgnoreCurrentClick = false;

	void HandleInsideBoardClick();
	void HandleFreeMovementClick();

public:
	virtual void OnMoveActionStarted() override;
	virtual void OnMoveActionTriggered(float DeltaTime) override;
	virtual void OnMoveActionReleased() override;
	virtual void OnMoveAndPlantAction() override;
};
