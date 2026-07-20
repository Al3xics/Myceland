// Copyright Myceland Team, All Rights Reserved.

#include "Input/Handlers/ML_GamepadInputHandler.h"

#include "Component/ML_BoardTransitionComponent.h"
#include "Core/ML_TileTypeTraits.h"
#include "Developer Settings/ML_MycelandDeveloperSettings.h"
#include "Player/ML_PlayerController.h"
#include "Player/ML_PlayerCharacter.h"
#include "Player/ML_HexPathfinder.h"
#include "Tiles/ML_Tile.h"
#include "Tiles/ML_BoardSpawner.h"
#include "User Settings/ML_GameUserSettings.h"

// ==================== Lifecycle ====================

void UML_GamepadInputHandler::OnActivated()
{
	Settings = UML_GameUserSettings::GetMycelandGameUserSettings();
	DevSettings = UML_MycelandDeveloperSettings::GetMycelandDeveloperSettings();

	ResetMoveRepeatState();
	// All board visuals (plant selection, plantable highlight, exit-plane availability) are owned by the
	// HoverPreviewComponent and refreshed when the device switches to gamepad. The handler is input-only.
}

void UML_GamepadInputHandler::OnDeactivated()
{
	if (!Controller) return;

	if (Controller->IsHoldingExitInput())
		Controller->CancelExitHold();
	Controller->ClearForcedHoverTile();
	// The HoverPreviewComponent clears its board visuals (selection / plantable / exit) on the device switch.
}

// ==================== Left stick (movement) ====================

void UML_GamepadInputHandler::OnStickAxis(FVector2D StickValue, float DeltaTime)
{
	if (!Controller) return;

	const EML_PlayerMovementMode Mode = Controller->GetMovementMode();
	if (Mode == EML_PlayerMovementMode::FreeMovement)
		HandleFreeMovementStick(StickValue);
	else if (Mode == EML_PlayerMovementMode::InsideBoard)
		HandleInsideBoardMoveStick(StickValue, DeltaTime);
	else if (Mode == EML_PlayerMovementMode::ExitingBoard)
		HandleExitingBoardStick(StickValue);
}

void UML_GamepadInputHandler::OnStickReleased()
{
	// Releasing the stick during an exit hold cancels it (mouse-parity: releasing the button cancels).
	if (Controller && Controller->IsHoldingExitInput())
	{
		Controller->CancelExitHold();
		Controller->ClearForcedHoverTile();
	}

	ResetMoveRepeatState();
}

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

void UML_GamepadInputHandler::HandleInsideBoardMoveStick(FVector2D StickValue, float DeltaTime)
{
	// No movement while the character is turning to plant (no input accepted in that state).
	if (Controller->GetBoardActionState() == EML_PlayerBoardActionState::TurningToPlant) return;

	const float HoldDelay = Settings ? Settings->GetGamepadHoldRepeatDelay() : 0.3f;
	const float RepeatInterval = Settings ? FMath::Max(Settings->GetGamepadHoldRepeatInterval(), 0.01f) : 0.1f;

	// First frame after returning from neutral: step immediately, then start the hold timer.
	if (bMoveStickWasNeutral)
	{
		bMoveStickWasNeutral = false;
		StepMoveInStickDirection(StickValue);
		return;
	}

	// Stick held: wait out the initial delay before the player starts auto-stepping to the next tile.
	if (!bMoveAutoRepeatActive)
	{
		MoveStickHeldTime += DeltaTime;
		if (MoveStickHeldTime < HoldDelay) return;

		bMoveAutoRepeatActive = true;
		StepMoveInStickDirection(StickValue);
		return;
	}

	// Auto-step active: step to the next tile once per RepeatInterval while held.
	MoveRepeatTimer += DeltaTime;
	while (MoveRepeatTimer >= RepeatInterval)
	{
		MoveRepeatTimer -= RepeatInterval;
		StepMoveInStickDirection(StickValue);
	}
}

void UML_GamepadInputHandler::StepMoveInStickDirection(FVector2D StickValue)
{
	AML_PlayerCharacter* Character = Controller->GetMycelandCharacter();
	if (!IsValid(Character) || !IsValid(Character->CurrentTileOn)) return;

	AML_BoardSpawner* Board = Character->CurrentTileOn->GetBoardSpawnerFromTile();
	if (!IsValid(Board)) return;

	const float Threshold = DevSettings ? DevSettings->GamepadMoveAlignmentThreshold : 0.5f;

	// Best walkable neighbor aligned with the stick (NeighborDot measures how well it matches).
	float NeighborDot = -1.f;
	AML_Tile* Neighbor = FindNeighborInStickDirection(StickValue, Threshold, /*bPlantableOnly=*/false, &NeighborDot);

	// On an exit border tile, pushing the stick toward the exit plane starts leaving the board — there is
	// no dedicated exit button anymore. The exit direction competes with the neighbor tiles: it only wins
	// when the stick points at the plane at least as well as at any walkable neighbor.
	float ExitDot = -1.f;
	FVector ExitTarget = FVector::ZeroVector;
	if (EvaluateStickExit(StickValue, ExitDot, ExitTarget) && ExitDot >= Threshold && ExitDot >= NeighborDot)
	{
		// Reuse the shared exit-hold flow (same as the mouse). With ExitBoardHoldDurationGamepad = 0 it resolves
		// on the first tick (instant); with a positive duration the player must keep the stick pointed at
		// the exit — HandleExitingBoardStick keeps the hold alive, releasing/steering away cancels it.
		Controller->RequestExitHold(Character->CurrentTileOn, ExitTarget);
		Controller->SetForcedHoverTile(Character->CurrentTileOn); // Exit-tile glow while the hold is active.
		return;
	}

	if (IsValid(Neighbor))
		Controller->Move(Neighbor);
}

void UML_GamepadInputHandler::ResetMoveRepeatState()
{
	bMoveStickWasNeutral = true;
	MoveStickHeldTime = 0.f;
	MoveRepeatTimer = 0.f;
	bMoveAutoRepeatActive = false;
}

// ==================== Right stick (plant selection) ====================

void UML_GamepadInputHandler::OnPlantSelectAxis(FVector2D StickValue, float DeltaTime)
{
	if (!Controller || Controller->GetMovementMode() != EML_PlayerMovementMode::InsideBoard) return;
	if (Controller->GetBoardActionState() == EML_PlayerBoardActionState::TurningToPlant) return;
	UpdatePlantSelection(StickValue);
}

void UML_GamepadInputHandler::OnPlantSelectReleased()
{
	// Selection is persistent (mirrors the mouse cursor staying on the last hovered tile):
	// nothing to reset here. The plant button acts on the HoverPreviewComponent's selection.
}

void UML_GamepadInputHandler::UpdatePlantSelection(FVector2D StickValue)
{
	if (StickValue.IsNearlyZero()) return;

	const float Threshold = DevSettings ? DevSettings->GamepadSelectAlignmentThreshold : 0.5f;
	AML_Tile* Best = FindNeighborInStickDirection(StickValue, Threshold, /*bPlantableOnly=*/true);
	if (!IsValid(Best) || Best == Controller->GetGamepadSelectedTile()) return;

	// The HoverPreviewComponent owns the selection state + its cursor glow.
	Controller->SetGamepadSelectedTile(Best);
}

void UML_GamepadInputHandler::OnMoveAndPlantAction()
{
	if (!Controller) return;
	Controller->Plant(Controller->GetGamepadSelectedTile()); // Plant() rejects gracefully if null
}

// ==================== Exit ====================

void UML_GamepadInputHandler::HandleExitingBoardStick(FVector2D StickValue)
{
	if (!Controller->IsHoldingExitInput()) return;

	// Mouse-parity hold: the exit only completes if the player keeps the stick pointed at the exit for
	// the full ExitBoardHoldDurationGamepad. Steering the stick away from the exit cancels the hold; a full
	// release is handled by OnStickReleased. (With ExitBoardHoldDurationGamepad = 0 the hold resolves before
	// this ever runs, so this path only matters for a positive hold duration.)
	const float Threshold = DevSettings ? DevSettings->GamepadMoveAlignmentThreshold : 0.5f;

	float ExitDot = -1.f;
	FVector ExitTarget = FVector::ZeroVector;
	if (!EvaluateStickExit(StickValue, ExitDot, ExitTarget) || ExitDot < Threshold)
	{
		Controller->CancelExitHold();
		Controller->ClearForcedHoverTile();
	}
}

// ==================== Helpers ====================

bool UML_GamepadInputHandler::EvaluateStickExit(FVector2D StickValue, float& OutExitDot, FVector& OutExitTarget) const
{
	AML_PlayerCharacter* Character = Controller ? Controller->GetMycelandCharacter() : nullptr;
	if (!IsValid(Character) || !IsValid(Character->CurrentTileOn)) return false;

	AML_BoardSpawner* Board = Character->CurrentTileOn->GetBoardSpawnerFromTile();
	if (!IsValid(Board)) return false;

	const FML_BoardExit* Exit = Board->FindExitForTile(Character->CurrentTileOn);
	if (!Exit || !IsValid(Exit->ExitPlane)) return false;

	OutExitTarget = Exit->ExitPlane->GetActorLocation();
	const FVector StickWorldDir = StickToWorldDirection(StickValue);
	const FVector ToExit = (OutExitTarget - Character->CurrentTileOn->GetActorLocation()).GetSafeNormal2D();
	OutExitDot = FVector::DotProduct(StickWorldDir, ToExit);
	return true;
}

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

AML_Tile* UML_GamepadInputHandler::FindNeighborInStickDirection(FVector2D StickValue, float AlignmentThreshold, bool bPlantableOnly, float* OutBestDot) const
{
	if (OutBestDot) *OutBestDot = -1.f;

	AML_PlayerCharacter* Character = Controller ? Controller->GetMycelandCharacter() : nullptr;
	if (!IsValid(Character) || !IsValid(Character->CurrentTileOn)) return nullptr;

	AML_BoardSpawner* Board = Character->CurrentTileOn->GetBoardSpawnerFromTile();
	if (!IsValid(Board)) return nullptr;

	AML_Tile* OriginTile = Character->CurrentTileOn;

	FML_TileNeighbors Neighbors;
	Board->GetNeighbors(OriginTile, Neighbors);
	if (Neighbors.IsEmpty()) return nullptr;

	const FVector StickWorldDir = StickToWorldDirection(StickValue);
	const FVector OriginPos = OriginTile->GetActorLocation();

	AML_Tile* BestNeighbor = nullptr;
	float BestDot = -1.f;

	for (AML_Tile* Neighbor : Neighbors)
	{
		if (!IsValid(Neighbor)) continue;

		// Movement only walks onto walkable tiles; plant selection only picks plantable tiles.
		const bool bCandidate = bPlantableOnly
			? UML_TileTypeTraits::CanPlayerPlant(Neighbor->GetCurrentType())
			: UML_HexPathfinder::IsTileWalkable(Neighbor);
		if (!bCandidate) continue;

		const FVector ToNeighbor = (Neighbor->GetActorLocation() - OriginPos).GetSafeNormal2D();
		const float Dot = FVector::DotProduct(StickWorldDir, ToNeighbor);

		if (Dot > BestDot)
		{
			BestDot = Dot;
			BestNeighbor = Neighbor;
		}
	}

	const bool bClears = (BestDot >= AlignmentThreshold);
	if (OutBestDot) *OutBestDot = bClears ? BestDot : -1.f;
	return bClears ? BestNeighbor : nullptr;
}
