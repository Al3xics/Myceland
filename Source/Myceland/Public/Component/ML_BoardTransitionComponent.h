// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/ML_CoreData.h"
#include "ML_BoardTransitionComponent.generated.h"

class AML_PlayerController;
class AML_PlayerCharacter;
class AML_BoardSpawner;
class AML_Tile;
class UML_MycelandDeveloperSettings;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnExitCursorHold, bool, bIsExiting, float, Progress);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBoardActivityStateChanged, bool, bIsMoving);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHoveredGroundChanged, AActor*, GroundActor);

/**
 * Result returned by HandlePathFinished / HandleBoardPresenceChanged so the controller
 * can react without creating a circular include dependency.
 */
USTRUCT()
struct FBoardTransitionCommand
{
	GENERATED_BODY()

	bool bStartNavMesh = false;
	FVector NavMeshTarget = FVector::ZeroVector;

	bool bStartBoardPath = false;
	TArray<FIntPoint> BoardPath;
	TMap<FIntPoint, AML_Tile*> BoardGridMap;
};

/**
 * Owns the board entry/exit state machine: movement mode, board action state,
 * exit hold timer, turn-toward-tile timer, and all associated pending state.
 * Extracted from AML_PlayerController.
 */
UCLASS(ClassGroup=(Myceland), meta=(BlueprintSpawnableComponent))
class MYCELAND_API UML_BoardTransitionComponent : public UActorComponent
{
	GENERATED_BODY()

private:

	UPROPERTY(Transient)
	AML_PlayerController* OwningController = nullptr;

	UPROPERTY(Transient)
	AML_PlayerCharacter* PlayerCharacter = nullptr;

	UPROPERTY(Transient)
	const UML_MycelandDeveloperSettings* DevSettings = nullptr;

	float RotateSpeed = 10.f;

	
	
	// ========== Movement mode ==========
	EML_PlayerMovementMode CurrentMovementMode = EML_PlayerMovementMode::InsideBoard;
	EML_PlayerBoardActionState BoardActionState = EML_PlayerBoardActionState::Idle;
	bool bWasMovingInBoard = false;

	
	
	// ========== Exit hold ==========
	float ExitHoldTimer = 0.f;
	bool bIsHoldingExitInput = false;
	bool bHasExitTargetWorld = false;
	bool bWasExitingLastFrame = false;
	float LastBroadcastProgress = -1.f;
	FTimerHandle ExitHoldTimerHandle;

	UPROPERTY(Transient)
	AML_Tile* PendingExitBorderTile = nullptr;

	FVector PendingExitTargetWorld = FVector::ZeroVector;



	// ========== Ground hover (cursor outside-board detection) ==========
	FTimerHandle GroundHoverTimerHandle;

	// Ground actor currently under the cursor. Only set while InsideBoard/ExitingBoard
	// with mouse & keyboard; always nullptr otherwise.
	UPROPERTY(Transient)
	AActor* HoveredGroundActor = nullptr;

	// False when a gamepad is active: the physical cursor position is meaningless.
	bool bCursorGroundDetectionEnabled = true;

	void TickGroundHover();
	void StartGroundHoverDetection();
	void StopGroundHoverDetection();
	void SetHoveredGroundActor(AActor* NewGround);

	
	
	// ========== Board entry ==========
	bool bPendingFreeMovementOnArrival = false;
	bool bPendingBoardEntryOnArrival = false;

	UPROPERTY(Transient)
	AML_Tile* PendingBoardEntryTargetTile = nullptr;

	
	
	// ========== Plant ==========
	UPROPERTY(Transient)
	AML_Tile* PendingPlantTargetTile = nullptr;

	
	
	// ========== Turn toward tile ==========
	FTimerHandle TurnTowardTileTimerHandle;

	
	
	// ========== Internal timer callbacks ==========
	void TickExitHold();
	void ConfirmExitBoard();
	void UpdateTurnTowardPendingTile();

	bool RotateCharacterTowardTile(const AML_Tile* Target, float DeltaTime, float TurnSpeed) const;

	// Timer intervals from the dev settings (safe fallbacks before Initialize).
	float GetCursorTickInterval() const;
	float GetExitHoldTickInterval() const;
	float GetTurnTickInterval() const;

public:

	void Initialize(AML_PlayerController* Controller, AML_PlayerCharacter* Character, const UML_MycelandDeveloperSettings* Settings, float InRotateSpeed);

	
	
	// ========== Delegates ==========
	
	// Whenever the player starts and stops moving INSIDE a board.
	// Will not be broadcasted when OUTSIDE the board.
	UPROPERTY(BlueprintAssignable, Category = "Board Transition Component|Delegates")
	FOnBoardActivityStateChanged OnBoardActivityStateChanged;
	
	// Called when the player holds cursor to exit board
	// The float is normalized between 0-1
	UPROPERTY(BlueprintAssignable, Category = "Board Transition Component|Delegates")
	FOnExitCursorHold OnExitCursorHold;

	// Called when the ground actor under the cursor changes while the player is inside a board
	// (InsideBoard/ExitingBoard, mouse & keyboard only). nullptr = the cursor is no longer over
	// a designated ground (it is over the board, decor, or nothing). Replaces the old
	// HoverPreviewComponent::OnCursorOverEmptySpaceChanged.
	UPROPERTY(BlueprintAssignable, Category = "Board Transition Component|Delegates")
	FOnHoveredGroundChanged OnHoveredGroundChanged;



	// ========== Queries ==========

	EML_PlayerMovementMode GetMovementMode() const { return CurrentMovementMode; }
	EML_PlayerBoardActionState GetBoardActionState() const { return BoardActionState; }
	bool IsOutsideBoardMovementMode() const;
	bool IsHoldingExitInput() const { return bIsHoldingExitInput; }
	bool IsPendingFreeMovementOnArrival() const { return bPendingFreeMovementOnArrival; }
	bool IsPendingBoardEntry() const { return bPendingBoardEntryOnArrival; }

	/** Ground actor under the cursor, or nullptr. See OnHoveredGroundChanged. */
	AActor* GetHoveredGroundActor() const { return HoveredGroundActor; }

	/** Stops the cursor-based ground detection while a gamepad is active. */
	void NotifyInputDeviceChanged(EML_InputDevice NewDevice);

	
	
	// ========== Mode management ==========

	/**
	 * Updates CurrentMovementMode and handles associated state cleanup.
	 * Call through AML_PlayerController::SetMovementMode so that the hover component
	 * and NavMesh are also notified.
	 */
	void SwitchToMode(EML_PlayerMovementMode NewMode);

	/** Handles bWasMovingInBoard and broadcasts OnBoardMovementStateChanged. */
	void NotifyIsMoving(bool bIsMoving);

	/**
	 * Hard-resets to FreeMovement: clears the exit-hold timer/state and any pending entry,
	 * then switches mode. Used when a board's transition is disabled while the player is inside it.
	 */
	void ForceFreeMovement();

	
	
	// ========== Exit hold ==========

	/** Called from OnSetDestinationStarted when clicking outside the board. */
	void RequestExitHold(AML_Tile* ExitBorderTile, const FVector& WorldTarget);

	/** Called from OnSetDestinationReleased. TickExitHold detects the flag and cancels. */
	void CancelExitHold();

	
	
	// ========== Board entry / action state ==========

	/** Set the board action state (and optional plant target tile). */
	void SetBoardActionState(EML_PlayerBoardActionState State, AML_Tile* PlantTarget = nullptr);

	/**
	 * Sets pending board entry state and switches mode to EnteringBoard.
	 * Controller still starts the NavMesh movement after calling this.
	 */
	void RequestBoardEntry(AML_Tile* TargetTile);

	/** Cancels any pending board entry intent (e.g. interrupted by a narrative trigger). */
	void CancelPendingBoardEntry();

	/** Starts the turn-toward-tile timer; sets BoardActionState = TurningToPlant. */
	void StartTurnTowardTile(AML_Tile* Target);

	
	
	// ========== Path completion callbacks ==========

	/**
	 * Called from AML_PlayerController::OnPathFinished.
	 * Returns a command struct describing what the controller should do next
	 * (e.g., start NavMesh or start a board path). Also updates internal state.
	 */
	FBoardTransitionCommand HandlePathFinished(AML_PlayerCharacter* Character);
};
