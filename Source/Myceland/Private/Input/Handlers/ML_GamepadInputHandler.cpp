// Copyright Myceland Team, All Rights Reserved.

#include "Input/Handlers/ML_GamepadInputHandler.h"

#include "Player/ML_PlayerController.h"
#include "Player/ML_PlayerCharacter.h"
#include "Player/ML_HexPathfinder.h"
#include "Tiles/ML_Tile.h"
#include "Tiles/ML_BoardSpawner.h"
#include "User Settings/ML_GameUserSettings.h"

void UML_GamepadInputHandler::OnActivated()
{
	Settings = UML_GameUserSettings::GetMycelandGameUserSettings();
	ResetStickRepeatState();
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
		HandleFreeMovementStick(StickValue);
	else if (Mode == EML_PlayerMovementMode::InsideBoard)
		HandleInsideBoardStick(StickValue, DeltaTime);
}

void UML_GamepadInputHandler::OnStickReleased()
{
	// IMC Completed fires when the stick returns to the dead zone.
	if (Controller && Controller->IsHoldingExitInput())
		Controller->CancelExitHold();
	ResetStickRepeatState();
}

void UML_GamepadInputHandler::OnMoveActionStarted()
{
	if (!Controller || Controller->GetMovementMode() != EML_PlayerMovementMode::InsideBoard) return;
	if (!IsValid(FocusedTile)) return;
	Controller->Move(FocusedTile);
	// ForcedHoverTile kept so TickPathHoverPreview continues updating the path glow during the walk.
	// FocusedTile cleared so the next flick starts from CurrentTileOn (the destination).
	FocusedTile = nullptr;
}

void UML_GamepadInputHandler::OnMoveAndPlantAction()
{
	if (!Controller || !IsValid(FocusedTile)) return;
	// ForcedHoverTile intentionally kept: path preview stays visible during the walk to plant.
	Controller->Plant(FocusedTile);
}

// ==================== Free movement ====================

void UML_GamepadInputHandler::HandleFreeMovementStick(FVector2D StickValue)
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

void UML_GamepadInputHandler::HandleInsideBoardStick(FVector2D StickValue, float DeltaTime)
{
	// IMC dead zone ensures the stick is active when this is called.
	if (Controller->GetBoardActionState() == EML_PlayerBoardActionState::TurningToPlant) return;

	const float HoldDelay = Settings ? Settings->GetGamepadHoldRepeatDelay() : 0.4f;
	const float RepeatInterval = Settings ? FMath::Max(Settings->GetGamepadHoldRepeatInterval(), 0.01f) : 0.15f;

	// First frame after returning from neutral: select immediately, then start the hold timer.
	// Accumulators are already zeroed by ResetStickRepeatState on the previous neutral release.
	if (bStickWasNeutral)
	{
		bStickWasNeutral = false;
		SelectTileInStickDirection(StickValue);
		return;
	}

	// Stick is being held. Wait out the initial delay before the cursor starts auto-advancing.
	if (!bAutoRepeatActive)
	{
		StickHeldTime += DeltaTime;
		if (StickHeldTime < HoldDelay) return;

		// Delay elapsed: enter auto-advance and perform the first repeated step right away.
		bAutoRepeatActive = true;
		SelectTileInStickDirection(StickValue);
		return;
	}

	// Auto-advance active: step to the next tile once per RepeatInterval while held.
	RepeatTimer += DeltaTime;
	while (RepeatTimer >= RepeatInterval)
	{
		RepeatTimer -= RepeatInterval;
		SelectTileInStickDirection(StickValue);
	}
}

void UML_GamepadInputHandler::SelectTileInStickDirection(FVector2D StickValue)
{
	// The exit hold is itself a hold gesture; once it has started, the movement mode leaves
	// InsideBoard so this is not reached again, but guard anyway against re-requesting.
	if (Controller->IsHoldingExitInput()) return;

	AML_PlayerCharacter* Character = Controller->GetMycelandCharacter();
	if (!IsValid(Character) || !IsValid(Character->CurrentTileOn)) return;

	AML_BoardSpawner* Board = Character->CurrentTileOn->GetBoardSpawnerFromTile();
	if (!IsValid(Board)) return;

	// On a walkable border tile, use a stricter alignment threshold (cos 30° instead of cos 60°)
	// so a diagonal neighbor at ~60° off from the outward push doesn't block the exit gesture.
	const bool bOnWalkableBorder = IsOriginTileWalkableBorderTile(Board);
	const float NeighborThreshold = bOnWalkableBorder ? 0.866f : 0.5f;

	AML_Tile* NeighborTile = FindNeighborInStickDirection(StickValue, NeighborThreshold);
	if (IsValid(NeighborTile))
	{
		// Neighbor found (walkable or not): advance the selection cursor.
		FocusedTile = NeighborTile;
		Controller->SetForcedHoverTile(FocusedTile);
	}
	else if (!IsValid(NeighborTile) && bOnWalkableBorder)
	{
		// No walkable neighbor in this direction and the cursor is on a walkable border tile.
		// Mirror the mouse behavior: any walkable border tile is a valid exit point.
		const FVector StickWorldDir = StickToWorldDirection(StickValue);
		FVector ExitTarget = Character->GetActorLocation() + StickWorldDir * 5000.f;
		ExitTarget.Z = Character->GetActorLocation().Z;

		AML_Tile* ExitGate = Controller->FindReachableExitBorderTile(Board, ExitTarget);
		if (IsValid(ExitGate))
		{
			Controller->RequestExitHold(ExitGate, ExitTarget);
			// Keep the hover on the tile the player was navigating from, not on the exit gate.
			AML_Tile* HoverTile = IsValid(FocusedTile) ? FocusedTile : Character->CurrentTileOn;
			Controller->SetForcedHoverTile(HoverTile);
		}
	}
}

void UML_GamepadInputHandler::ResetStickRepeatState()
{
	bStickWasNeutral = true;
	StickHeldTime = 0.f;
	RepeatTimer = 0.f;
	bAutoRepeatActive = false;
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

	return (WorldForward * StickValue.Y + WorldRight * StickValue.X).GetSafeNormal();
}

AML_Tile* UML_GamepadInputHandler::FindNeighborInStickDirection(FVector2D StickValue, float AlignmentThreshold) const
{
	AML_PlayerCharacter* Character = Controller ? Controller->GetMycelandCharacter() : nullptr;
	if (!IsValid(Character) || !IsValid(Character->CurrentTileOn)) return nullptr;

	// Navigate from FocusedTile (the selection cursor), not from the player's physical tile.
	// This lets successive flicks advance the cursor through the board without requiring
	// the player to physically move between each flick.
	// Guard: discard FocusedTile if it belongs to a different board (stale from a previous visit).
	AML_BoardSpawner* Board = Character->CurrentTileOn->GetBoardSpawnerFromTile();
	if (!IsValid(Board)) return nullptr;

	AML_Tile* OriginTile = (IsValid(FocusedTile) && FocusedTile->GetBoardSpawnerFromTile() == Board)
		? FocusedTile
		: Character->CurrentTileOn;

	FML_TileNeighbors Neighbors;
	Board->GetNeighbors(OriginTile, Neighbors);
	if (Neighbors.IsEmpty()) return nullptr;

	const FVector StickWorldDir = StickToWorldDirection(StickValue);
	const FVector OriginPos = OriginTile->GetActorLocation();

	AML_Tile* BestNeighbor = nullptr;
	float BestDot = -1.f;

	for (AML_Tile* Neighbor : Neighbors)
	{
		// Skip only truly missing board slots. Non-walkable tiles (water, obstacles) are
		// included so they can be detected as "something is there" by the caller.
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

	return (BestDot >= AlignmentThreshold) ? BestNeighbor : nullptr;
}

bool UML_GamepadInputHandler::IsOriginTileWalkableBorderTile(const AML_BoardSpawner* Board) const
{
	AML_PlayerCharacter* Character = Controller ? Controller->GetMycelandCharacter() : nullptr;
	if (!IsValid(Character) || !IsValid(Character->CurrentTileOn) || !IsValid(Board)) return false;

	const AML_Tile* OriginTile = (IsValid(FocusedTile) && FocusedTile->GetBoardSpawnerFromTile() == Board)
		? FocusedTile
		: Character->CurrentTileOn;

	return UML_HexPathfinder::IsTileWalkable(OriginTile) && OriginTile->IsBorderTile();
}
