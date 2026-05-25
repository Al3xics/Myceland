// Copyright Myceland Team, All Rights Reserved.

#include "Input/Handlers/ML_MouseKeyboardInputHandler.h"

#include "Player/ML_PlayerController.h"
#include "Player/ML_PlayerCharacter.h"
#include "Tiles/ML_Tile.h"
#include "Tiles/ML_BoardSpawner.h"

void UML_MouseKeyboardInputHandler::OnMoveActionStarted()
{
	FollowTime = 0.f;

	const EML_PlayerMovementMode Mode = Controller->GetMovementMode();
	if (Mode == EML_PlayerMovementMode::InsideBoard)
		HandleInsideBoardClick();
	else if (Mode == EML_PlayerMovementMode::FreeMovement)
		HandleFreeMovementClick();
}

void UML_MouseKeyboardInputHandler::OnMoveActionTriggered(float DeltaTime)
{
	FollowTime += DeltaTime;

	if (Controller->GetMovementMode() != EML_PlayerMovementMode::FreeMovement)
		return;

	AML_PlayerCharacter* Character = Controller->GetMycelandCharacter();
	if (!IsValid(Character))
		return;

	FHitResult Hit;
	if (!Controller->GetHitResultUnderCursorByChannel(UEngineTypes::ConvertToTraceType(ECC_Visibility), true, Hit))
		return;
	if (!Controller->IsClickableGround(Hit))
		return;

	// Only follow on open ground — clicking on a board still triggers re-entry on release.
	if (!Cast<AML_Tile>(Hit.GetActor()))
		HoldMoveCachedDestination = Hit.Location;

	const FVector Direction = (HoldMoveCachedDestination - Character->GetActorLocation()).GetSafeNormal();
	Character->AddMovementInput(Direction, Controller->GetMoveSpeedScale());
}

void UML_MouseKeyboardInputHandler::OnMoveActionReleased()
{
	Controller->CancelExitHold();

	if (Controller->GetMovementMode() == EML_PlayerMovementMode::FreeMovement)
	{
		if (FollowTime <= Controller->GetShortPressThreshold())
			Controller->StartNavMeshMovement(HoldMoveCachedDestination);
	}

	FollowTime = 0.f;
}

void UML_MouseKeyboardInputHandler::OnMoveAndPlantAction()
{
	AML_Tile* TargetTile = Controller->GetTileUnderCursor();
	Controller->Plant(TargetTile);
}

void UML_MouseKeyboardInputHandler::HandleInsideBoardClick()
{
	AML_PlayerCharacter* Character = Controller->GetMycelandCharacter();
	if (!IsValid(Character) || !IsValid(Character->CurrentTileOn)) return;

	AML_BoardSpawner* Board = Character->CurrentTileOn->GetBoardSpawnerFromTile();
	if (!IsValid(Board)) return;

	if (Controller->GetBoardActionState() == EML_PlayerBoardActionState::TurningToPlant) return;

	AML_Tile* TargetTile = Controller->GetTileUnderCursor();

	// Click on a tile in the board (move in board)
	if (IsValid(TargetTile) && TargetTile->GetOwner() == Board)
	{
		Controller->Move(TargetTile);
		return;
	}

	// If arrives here, then the click is outside the board (wants to leave the board)
	FHitResult Hit;
	if (!Controller->GetHitResultUnderCursorByChannel(UEngineTypes::ConvertToTraceType(ECC_Visibility), true, Hit)) return;
	if (!Controller->IsClickableGround(Hit)) return;

	AML_Tile* ExitGate = Controller->FindReachableExitBorderTile(Board, Hit.Location);
	if (!IsValid(ExitGate)) return;

	Controller->RequestExitHold(ExitGate, Hit.Location);
}

void UML_MouseKeyboardInputHandler::HandleFreeMovementClick()
{
	Controller->StopNavMeshMovement();

	AML_Tile* TargetTile = Controller->GetTileUnderCursor();
	
	// Click on a tile in the board (wants to enter the board)
	if (IsValid(TargetTile))
	{
		AML_BoardSpawner* Board = TargetTile->GetBoardSpawnerFromTile();
		if (!IsValid(Board)) return;

		Controller->RequestBoardEntry(TargetTile);
		Controller->StartNavMeshMovement(TargetTile->GetActorLocation());
		return;
	}

	// Didn't click in the board, continue to move outside (free movement)
	FHitResult Hit;
	if (!Controller->GetHitResultUnderCursorByChannel(UEngineTypes::ConvertToTraceType(ECC_Visibility), true, Hit)) return;
	if (!Controller->IsClickableGround(Hit)) return;
	HoldMoveCachedDestination = Hit.Location;
}
