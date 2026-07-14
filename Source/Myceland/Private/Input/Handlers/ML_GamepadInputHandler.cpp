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
}

void UML_GamepadInputHandler::OnStickReleased()
{
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
	AML_Tile* Neighbor = FindNeighborInStickDirection(StickValue, Threshold, /*bPlantableOnly=*/false);
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

void UML_GamepadInputHandler::OnExitAction()
{
	if (!Controller || Controller->GetMovementMode() != EML_PlayerMovementMode::InsideBoard) return;

	AML_PlayerCharacter* Character = Controller->GetMycelandCharacter();
	if (!IsValid(Character) || !IsValid(Character->CurrentTileOn)) return;

	AML_BoardSpawner* Board = Character->CurrentTileOn->GetBoardSpawnerFromTile();
	if (!IsValid(Board)) return;

	const FML_BoardExit* Exit = Board->FindExitForTile(Character->CurrentTileOn);
	if (!Exit || !IsValid(Exit->ExitPlane)) return;

	// Reuse the shared exit-hold flow (same as the mouse). With ExitBoardHoldDuration = 0 this
	// resolves on the first tick, i.e. an instant press. The player already stands on the exit
	// border tile, so the hold confirms into a direct exit toward the plane location.
	Controller->RequestExitHold(Character->CurrentTileOn, Exit->ExitPlane->GetActorLocation());
	Controller->SetForcedHoverTile(Character->CurrentTileOn);
}

void UML_GamepadInputHandler::OnExitReleased()
{
	if (!Controller) return;
	if (Controller->IsHoldingExitInput())
	{
		Controller->CancelExitHold();
		Controller->ClearForcedHoverTile();
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

	return (WorldForward * StickValue.Y + WorldRight * StickValue.X).GetSafeNormal();
}

AML_Tile* UML_GamepadInputHandler::FindNeighborInStickDirection(FVector2D StickValue, float AlignmentThreshold, bool bPlantableOnly) const
{
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

	return (BestDot >= AlignmentThreshold) ? BestNeighbor : nullptr;
}
