// Copyright Myceland Team, All Rights Reserved.

#include "Input/Handlers/ML_GamepadInputHandler.h"

#include "Player/ML_PlayerController.h"
#include "Player/ML_PlayerCharacter.h"
#include "Player/ML_HexPathfinder.h"
#include "Tiles/ML_Tile.h"
#include "Tiles/ML_BoardSpawner.h"

void UML_GamepadInputHandler::OnActivated()
{
	bStickWasNeutral = true;
	FocusedTile = nullptr;
}

void UML_GamepadInputHandler::OnDeactivated()
{
	if (!Controller) return;
	if (Controller->IsHoldingExitInput())
		Controller->CancelExitHold();
	Controller->ClearForcedHoverTile();
}

void UML_GamepadInputHandler::OnStickAxis(FVector2D StickValue, float DeltaTime)
{
	if (!Controller) return;

	const EML_PlayerMovementMode Mode = Controller->GetMovementMode();
	if (Mode == EML_PlayerMovementMode::FreeMovement)
		HandleFreeMovementStick(StickValue, DeltaTime);
	else if (Mode == EML_PlayerMovementMode::InsideBoard)
		HandleInsideBoardStick(StickValue);
}

void UML_GamepadInputHandler::OnStickReleased()
{
	// IMC Completed fires when the stick returns to the dead zone.
	if (Controller && Controller->IsHoldingExitInput())
		Controller->CancelExitHold();
	bStickWasNeutral = true;
}

void UML_GamepadInputHandler::OnMoveActionStarted()
{
	if (!Controller || Controller->GetMovementMode() != EML_PlayerMovementMode::InsideBoard) return;
	if (!IsValid(FocusedTile)) return;
	Controller->ClearForcedHoverTile();
	Controller->Move(FocusedTile);
	// Reset cursor to null so the next flick sequence starts from CurrentTileOn (destination).
	FocusedTile = nullptr;
}

void UML_GamepadInputHandler::OnMoveAndPlantAction()
{
	if (!Controller || !IsValid(FocusedTile)) return;
	Controller->ClearForcedHoverTile();
	Controller->Plant(FocusedTile);
}

// ==================== Free movement ====================

void UML_GamepadInputHandler::HandleFreeMovementStick(FVector2D StickValue, float DeltaTime)
{
	if (StickValue.IsNearlyZero()) return;

	AML_PlayerCharacter* Character = Controller->GetMycelandCharacter();
	if (!IsValid(Character)) return;

	// Cancel any ongoing navmesh movement so the stick has immediate full control.
	Controller->StopNavMeshMovement();

	const FVector WorldDirection = StickToWorldDirection(StickValue);
	Character->AddMovementInput(WorldDirection, FreeMovementScale);
}

// ==================== Inside board ====================

void UML_GamepadInputHandler::HandleInsideBoardStick(FVector2D StickValue)
{
	// IMC dead zone ensures the stick is active when this is called.
	// Only act on the first frame after returning from neutral (one-shot gate).
	if (!bStickWasNeutral) return;
	bStickWasNeutral = false;

	if (Controller->GetBoardActionState() == EML_PlayerBoardActionState::TurningToPlant) return;

	AML_Tile* NeighborTile = FindNeighborInStickDirection(StickValue);
	if (IsValid(NeighborTile) && UML_HexPathfinder::IsTileWalkable(NeighborTile))
	{
		// Walkable neighbor found: advance the selection cursor.
		FocusedTile = NeighborTile;
		Controller->SetForcedHoverTile(FocusedTile);
	}
	else if (!IsValid(NeighborTile) && IsNearExitTile())
	{
		// No neighbor at all in this direction AND near a board exit point.
		AML_PlayerCharacter* Character = Controller->GetMycelandCharacter();
		if (!IsValid(Character) || !IsValid(Character->CurrentTileOn)) return;

		AML_BoardSpawner* Board = Character->CurrentTileOn->GetBoardSpawnerFromTile();
		if (!IsValid(Board)) return;

		const FVector StickWorldDir = StickToWorldDirection(StickValue);
		FVector ExitTarget = Character->GetActorLocation() + StickWorldDir * 5000.f;
		ExitTarget.Z = Character->GetActorLocation().Z;

		AML_Tile* ExitGate = Controller->FindReachableExitBorderTile(Board, ExitTarget);
		if (IsValid(ExitGate))
			Controller->RequestExitHold(ExitGate, ExitTarget);
	}
}

// ==================== Helpers ====================

FVector UML_GamepadInputHandler::StickToWorldDirection(FVector2D StickValue) const
{
	if (!Controller) return FVector::ForwardVector;

	// Use the camera's actual yaw so movement is relative to what the player sees.
	// GetControlRotation() is not updated when the view target is a camera rail.
	const float CameraYaw = Controller->PlayerCameraManager
		? Controller->PlayerCameraManager->GetCameraRotation().Yaw
		: Controller->GetControlRotation().Yaw;

	const FRotator YawRotation(0.f, CameraYaw, 0.f);
	const FVector WorldForward = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector WorldRight   = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	// Gamepad Y axis is hardware-negative when pushing up; negate to match screen-space convention.
	return (WorldForward * (-StickValue.Y) + WorldRight * StickValue.X).GetSafeNormal();
}

AML_Tile* UML_GamepadInputHandler::FindNeighborInStickDirection(FVector2D StickValue) const
{
	AML_PlayerCharacter* Character = Controller ? Controller->GetMycelandCharacter() : nullptr;
	if (!IsValid(Character) || !IsValid(Character->CurrentTileOn)) return nullptr;

	// Navigate from FocusedTile (the selection cursor), not from the player's physical tile.
	// This lets successive flicks advance the cursor through the board without requiring
	// the player to physically move between each flick.
	// Guard: discard FocusedTile if it belongs to a different board (stale from a previous visit).
	AML_BoardSpawner* CurrentBoard = Character->CurrentTileOn->GetBoardSpawnerFromTile();
	AML_Tile* OriginTile = (IsValid(FocusedTile) && FocusedTile->GetBoardSpawnerFromTile() == CurrentBoard)
		? FocusedTile
		: Character->CurrentTileOn;
	AML_BoardSpawner* Board = CurrentBoard;
	if (!IsValid(Board)) return nullptr;

	const TArray<AML_Tile*> Neighbors = Board->GetNeighbors(OriginTile);
	if (Neighbors.IsEmpty()) return nullptr;

	const FVector StickWorldDir = StickToWorldDirection(StickValue);
	const FVector OriginPos = OriginTile->GetActorLocation();

	AML_Tile* BestNeighbor = nullptr;
	float BestDot = -1.f;

	for (AML_Tile* Neighbor : Neighbors)
	{
		if (!IsValid(Neighbor)) continue;

		const FVector ToNeighbor = (Neighbor->GetActorLocation() - OriginPos).GetSafeNormal2D();
		// Dot product: 1 = same direction, 0 = perpendicular, -1 = opposite.
		// Keep the neighbor most aligned with the stick direction.
		const float Dot = FVector::DotProduct(StickWorldDir, ToNeighbor);

		if (Dot > BestDot)
		{
			BestDot = Dot;
			BestNeighbor = Neighbor;
		}
	}

	// Require at least ~60° alignment (cos 60° ≈ 0.5) to avoid ambiguous diagonals.
	return (BestDot >= 0.5f) ? BestNeighbor : nullptr;
}

bool UML_GamepadInputHandler::IsNearExitTile() const
{
	AML_PlayerCharacter* Character = Controller ? Controller->GetMycelandCharacter() : nullptr;
	if (!IsValid(Character) || !IsValid(Character->CurrentTileOn)) return false;

	AML_BoardSpawner* Board = Character->CurrentTileOn->GetBoardSpawnerFromTile();
	if (!IsValid(Board)) return false;

	// Use FocusedTile as the reference point when it belongs to this board.
	// The player may have navigated the selection cursor far from their physical tile,
	// so measuring from CurrentTileOn would incorrectly reject the exit.
	const AML_Tile* ReferenceTile = (IsValid(FocusedTile) && FocusedTile->GetBoardSpawnerFromTile() == Board)
		? FocusedTile
		: Character->CurrentTileOn;

	const FVector ReferenceLoc = ReferenceTile->GetActorLocation();
	const float RadiusSq = FMath::Square(ExitActivationRadius);

	for (const FML_WaterPath& WaterPath : Board->WaterPaths)
	{
		if (IsValid(WaterPath.EntryTile) &&
			FVector::DistSquared2D(ReferenceLoc, WaterPath.EntryTile->GetActorLocation()) <= RadiusSq)
			return true;

		if (IsValid(WaterPath.ExitTile) &&
			FVector::DistSquared2D(ReferenceLoc, WaterPath.ExitTile->GetActorLocation()) <= RadiusSq)
			return true;
	}

	return false;
}
