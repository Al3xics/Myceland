// Copyright Myceland Team, All Rights Reserved.

#include "Component/ML_HoverPreviewComponent.h"

#include "Component/ML_BoardTransitionComponent.h"
#include "Core/ML_TileTypeTraits.h"
#include "Developer Settings/ML_MycelandDeveloperSettings.h"
#include "Engine/Engine.h"
#include "Player/ML_PlayerController.h"
#include "Player/ML_PlayerCharacter.h"
#include "Player/ML_HexPathfinder.h"
#include "Subsystem/ML_WavePropagationSubsystem.h"
#include "Tiles/ML_Tile.h"
#include "Tiles/ML_BoardSpawner.h"

void UML_HoverPreviewComponent::Initialize(AML_PlayerController* Controller, AML_PlayerCharacter* Character)
{
	OwningController = Controller;
	PlayerCharacter = Character;

	// Drive the gamepad plantable highlight off the same signals that change the board state:
	// move start/stop (the "highlight while moving" gate) and the end of a wave propagation
	// (tiles changed type). Player-tile / mode / device changes are handled by the Notify* hooks.
	if (OwningController && OwningController->TransitionComponent)
		OwningController->TransitionComponent->OnBoardActivityStateChanged.AddUniqueDynamic(this, &UML_HoverPreviewComponent::HandleBoardActivityStateChanged);

	if (UWorld* World = GetWorld())
		if (UML_WavePropagationSubsystem* WaveSub = World->GetSubsystem<UML_WavePropagationSubsystem>())
		{
			WaveSub->OnWavePropagationFinished.AddUniqueDynamic(this, &UML_HoverPreviewComponent::HandleWavePropagationFinished);
			WaveSub->OnUndoAnimating.AddUniqueDynamic(this, &UML_HoverPreviewComponent::HandleRollbackAnimating);
			WaveSub->OnResetAnimating.AddUniqueDynamic(this, &UML_HoverPreviewComponent::HandleRollbackAnimating);
		}
}

void UML_HoverPreviewComponent::NotifyMovementModeChanged(EML_PlayerMovementMode NewMode)
{
	CurrentMovementMode = NewMode;

	// Leaving board mode — clear path preview
	if (NewMode != EML_PlayerMovementMode::InsideBoard && NewMode != EML_PlayerMovementMode::ExitingBoard)
	{
		ClearPathHoverPreview();
		ClearForcedHoverTile();
	}

	// The board visuals only show inside a board — re-evaluate selection + plantables on any mode change.
	RefreshGamepadBoardVisuals();
}

void UML_HoverPreviewComponent::NotifyPlayerTileChanged()
{
	if (IsValid(PlayerCharacter) && IsValid(PlayerCharacter->CurrentTileOn) &&
		(ForcedHoverTile == PlayerCharacter->CurrentTileOn ||
		 LastCursorHoveredTile == PlayerCharacter->CurrentTileOn))
	{
		// Player just arrived on the highlighted tile → clear its glow immediately.
		ClearCursorHoverPreview();
	}
	else if (bLastCursorTileIsPlayerTile && IsValid(LastCursorHoveredTile) && bCurrentHoveredTileReachable)
	{
		// Player left the tile the cursor is still hovering → refresh it from the
		// special player-tile glow back to a normal cursor hover.
		LastCursorHoveredTile->GlowCursorHovered(false, true);
		bLastCursorTileIsPlayerTile = false;
	}

	// Force the preview path update from the new position.
	LastPathHoveredTile = nullptr;
	if (HoverPreviewTimerHandle.IsValid())
		TickPathHoverPreview();

	// The board visuals depend on the player's tile — recompute selection + plantables around it.
	RefreshGamepadBoardVisuals();
}

void UML_HoverPreviewComponent::StartHoverPreviewTimer()
{
	if (!HoverPreviewTimerHandle.IsValid())
	{
		GetWorld()->GetTimerManager().SetTimer(
			HoverPreviewTimerHandle,
			this,
			&UML_HoverPreviewComponent::UpdateHoverPreview,
			UML_MycelandDeveloperSettings::GetMycelandDeveloperSettings()->GetCursorDetectionTickInterval(),
			true
		);
	}
}

void UML_HoverPreviewComponent::StopHoverPreviewTimer()
{
	GetWorld()->GetTimerManager().ClearTimer(HoverPreviewTimerHandle);
	ClearGamepadSelection();
	ClearExitHighlight();
	ClearPathHoverPreview();
	ClearCursorHoverPreview();
	ClearForcedHoverTile();
	ClearPlantableHighlight();
}

// Bypasses cursor detection and locks the hover preview onto the given tile until cleared.
// Invalidates LastPathHoveredTile so TickPathHoverPreview immediately recomputes the path.
void UML_HoverPreviewComponent::SetForcedHoverTile(AML_Tile* Tile)
{
	ForcedHoverTile = Tile;
	// Force immediate recalculation
	LastPathHoveredTile = nullptr;
	if (HoverPreviewTimerHandle.IsValid())
		TickPathHoverPreview();
}

// Removes the forced tile override and hands control back to cursor-based detection.
void UML_HoverPreviewComponent::ClearForcedHoverTile()
{
	ForcedHoverTile = nullptr;

	// Explicitly un-glow the path tiles. In gamepad mode there is no cursor to fall back to, so
	// TickPathHoverPreview would see HoveredTile(null) == LastPathHoveredTile and short-circuit,
	// leaving the previous path glowing (visible from where the player was). ClearPathHoverPreview
	// also resets LastPathHoveredTile, so the follow-up tick correctly rebuilds from the cursor (MK).
	ClearPathHoverPreview();
	if (HoverPreviewTimerHandle.IsValid())
		TickPathHoverPreview();
}

void UML_HoverPreviewComponent::NotifyInputDeviceChanged(EML_InputDevice NewDevice)
{
	bCursorHoverEnabled = (NewDevice == EML_InputDevice::MouseKeyboard);

	// Switching to gamepad: clear any cursor-driven hover that was still active.
	if (!bCursorHoverEnabled)
		ClearCursorHoverPreview();

	// Board visuals (plantables + selection) are gamepad-only: show on gamepad, clear on mouse/keyboard.
	RefreshGamepadBoardVisuals();
}

void UML_HoverPreviewComponent::UpdateShowPreviews(const bool Value)
{
	bShowPreviews = Value;
	
	// Disable previews if `bShowPreviews` is false
	if (!bShowPreviews)
	{
		ClearPathHoverPreview();
		ClearCursorHoverPreview();
		ClearGamepadSelection();
		ClearPlantableHighlight();
		ClearExitHighlight();
	}
	else
	{
		RefreshGamepadBoardVisuals();
	}
}

void UML_HoverPreviewComponent::UpdateHoverPreview()
{
	// Do not show preview if `bShowPreviews` is false
	if (!bShowPreviews) return;

	TickPathHoverPreview();
	TickCursorHoverPreview();
}

void UML_HoverPreviewComponent::TickCursorHoverPreview()
{
	if (!IsValid(OwningController))
		return;

	// ForcedHoverTile takes priority; in gamepad mode the physical cursor position is ignored.
	AML_Tile* CursorHoveredTile = ForcedHoverTile
		? ForcedHoverTile
		: (bCursorHoverEnabled ? OwningController->GetTileUnderCursor() : nullptr);

	if (CursorHoveredTile == LastCursorHoveredTile)
		return;

	if (!IsValid(CursorHoveredTile))
	{
		ClearCursorHoverPreview();
		return;
	}

	// Per-board glow switch: if the hovered tile's board has glow disabled, treat it as no hover.
	const AML_BoardSpawner* CursorBoard = CursorHoveredTile->GetBoardSpawnerFromTile();
	if (IsValid(CursorBoard) && !CursorBoard->IsGlowEnabled())
	{
		ClearCursorHoverPreview();
		return;
	}

	if (IsValid(LastCursorHoveredTile))
		LastCursorHoveredTile->StopGlowingCursorUnhovered();

	// Non-walkable tiles still glow on hover, just with a different (blocked) color driven by bIsWalkable.
	// The path glow is handled separately in TickPathHoverPreview and only triggers on reachable tiles.
	const bool bIsWalkable = UML_HexPathfinder::IsTileWalkable(CursorHoveredTile);
	const bool bIsPlayerTile = IsValid(PlayerCharacter) && IsValid(PlayerCharacter->CurrentTileOn) &&
	                           CursorHoveredTile == PlayerCharacter->CurrentTileOn;
	CursorHoveredTile->GlowCursorHovered(bIsPlayerTile, bIsWalkable);
	bLastCursorTileIsPlayerTile = bIsPlayerTile;

	LastCursorHoveredTile = CursorHoveredTile;
}

void UML_HoverPreviewComponent::ClearCursorHoverPreview()
{
	if (IsValid(LastCursorHoveredTile))
	{
		LastCursorHoveredTile->StopGlowingCursorUnhovered();
		LastCursorHoveredTile = nullptr;
	}
	bLastCursorTileIsPlayerTile = false;
}

void UML_HoverPreviewComponent::ClearActiveGlow()
{
	ClearPathHoverPreview();
	ClearCursorHoverPreview();
}

// ==================== Gamepad board visuals (plantable + selection) ====================

void UML_HoverPreviewComponent::SetGamepadSelectedTile(AML_Tile* Tile)
{
	GamepadSelectedTile = Tile;
	SetForcedHoverTile(Tile);    // cursor glow + path preview on the selected tile
	RefreshPlantableHighlight(); // exclude it from the "available" glow (and re-glow the previous one)
}

void UML_HoverPreviewComponent::ClearGamepadSelection()
{
	if (!IsValid(GamepadSelectedTile)) return; // nothing to clear — avoids churn while moving
	GamepadSelectedTile = nullptr;
	ClearForcedHoverTile();
	RefreshPlantableHighlight();
}

void UML_HoverPreviewComponent::UpdateGamepadSelection()
{
	// A plant selection is only offered in gamepad mode, inside a board, while stopped and not
	// turning-to-plant (IsMoveInProgress() is the reliable "stopped" signal — see ShouldHighlightPlantableNow).
	const bool bShouldSelect = !bCursorHoverEnabled && bShowPreviews && IsValid(OwningController)
		&& OwningController->GetMovementMode() == EML_PlayerMovementMode::InsideBoard
		&& !OwningController->IsMoveInProgress()
		&& OwningController->GetBoardActionState() != EML_PlayerBoardActionState::TurningToPlant;

	if (!bShouldSelect)
	{
		ClearGamepadSelection();
		return;
	}

	// Keep a still-valid selection (e.g. one the player aimed with the right stick) — plantable or not, so a
	// non-plantable tile the player deliberately hovered survives a refresh (mouse-parity stickiness).
	if (IsValid(GamepadSelectedTile) && IsSelectableNeighborOfPlayer(GamepadSelectedTile))
		return;

	// Nothing valid selected: rest on a tile around the player. A plantable neighbor is preferred (the plant
	// button is then immediately useful), but when none exists we still fall back to any neighbor so the
	// cursor always hovers a tile at rest instead of clearing.
	if (AML_Tile* Default = FindDefaultSelectableNeighbor())
		SetGamepadSelectedTile(Default);
	else
		ClearGamepadSelection();
}

void UML_HoverPreviewComponent::RefreshGamepadBoardVisuals()
{
	// Selection first (it drives which tile is excluded from the plantable glow), then the plantable set,
	// then the exit-plane availability. Running here (called from NotifyMovementModeChanged too) means the
	// exit highlights correctly right after board entry, once the movement mode has flipped to InsideBoard.
	UpdateGamepadSelection();
	RefreshPlantableHighlight();
	UpdateExitHighlight();
}

void UML_HoverPreviewComponent::UpdateExitHighlight()
{
	if (!IsValid(OwningController)) return;

	// Gamepad only, inside a board, while previews are on, standing on a tile that maps to an exit plane.
	AActor* NewPlane = nullptr;
	if (!bCursorHoverEnabled && bShowPreviews &&
		OwningController->GetMovementMode() == EML_PlayerMovementMode::InsideBoard &&
		IsValid(PlayerCharacter) && IsValid(PlayerCharacter->CurrentTileOn))
	{
		if (AML_BoardSpawner* Board = PlayerCharacter->CurrentTileOn->GetBoardSpawnerFromTile())
			// Only highlight a usable exit — GetActiveExitPlaneForTile also filters out disabled ones, so the
			// glow can never show for an exit the player can't actually take.
			NewPlane = Board->GetActiveExitPlaneForTile(PlayerCharacter->CurrentTileOn);
	}

	if (NewPlane == CurrentExitPlane) return;

	if (IsValid(CurrentExitPlane))
		OwningController->OnBoardExitAvailabilityChanged.Broadcast(CurrentExitPlane, false);

	CurrentExitPlane = NewPlane;

	if (IsValid(CurrentExitPlane))
		OwningController->OnBoardExitAvailabilityChanged.Broadcast(CurrentExitPlane, true);
}

void UML_HoverPreviewComponent::ClearExitHighlight()
{
	if (IsValid(OwningController) && IsValid(CurrentExitPlane))
		OwningController->OnBoardExitAvailabilityChanged.Broadcast(CurrentExitPlane, false);
	CurrentExitPlane = nullptr;
}

AML_Tile* UML_HoverPreviewComponent::FindDefaultSelectableNeighbor() const
{
	if (!IsValid(PlayerCharacter) || !IsValid(PlayerCharacter->CurrentTileOn)) return nullptr;

	AML_BoardSpawner* Board = PlayerCharacter->CurrentTileOn->GetBoardSpawnerFromTile();
	if (!IsValid(Board)) return nullptr;

	FML_TileNeighbors Neighbors;
	Board->GetNeighbors(PlayerCharacter->CurrentTileOn, Neighbors);

	// Prefer any plantable neighbor (direction-agnostic — the player may face an obstacle). If none is
	// plantable, fall back to the first valid neighbor so the cursor still rests on a tile around the player.
	AML_Tile* FirstNeighbor = nullptr;
	for (AML_Tile* Neighbor : Neighbors)
	{
		if (!IsValid(Neighbor)) continue;
		if (UML_TileTypeTraits::CanPlayerPlant(Neighbor->GetCurrentType()))
			return Neighbor;
		if (!FirstNeighbor) FirstNeighbor = Neighbor;
	}

	return FirstNeighbor;
}

bool UML_HoverPreviewComponent::IsSelectableNeighborOfPlayer(const AML_Tile* Tile) const
{
	if (!IsValid(Tile) || !IsValid(PlayerCharacter) || !IsValid(PlayerCharacter->CurrentTileOn)) return false;

	AML_BoardSpawner* Board = PlayerCharacter->CurrentTileOn->GetBoardSpawnerFromTile();
	if (!IsValid(Board)) return false;

	FML_TileNeighbors Neighbors;
	Board->GetNeighbors(PlayerCharacter->CurrentTileOn, Neighbors);
	return Neighbors.Contains(Tile);
}

void UML_HoverPreviewComponent::RefreshPlantableHighlight()
{
	ClearPlantableHighlight();

	if (!ShouldHighlightPlantableNow()) return;
	if (!IsValid(PlayerCharacter) || !IsValid(PlayerCharacter->CurrentTileOn)) return;

	AML_BoardSpawner* Board = PlayerCharacter->CurrentTileOn->GetBoardSpawnerFromTile();
	if (!IsValid(Board) || !Board->IsGlowEnabled()) return;

	FML_TileNeighbors Neighbors;
	Board->GetNeighbors(PlayerCharacter->CurrentTileOn, Neighbors);

	for (AML_Tile* Neighbor : Neighbors)
	{
		// Skip the currently-selected tile: it already shows the cursor glow (GlowCursorHovered),
		// and both glows write the same tile color, so they would otherwise fight over it.
		if (Neighbor == ForcedHoverTile) continue;

		if (IsValid(Neighbor) && UML_TileTypeTraits::CanPlayerPlant(Neighbor->GetCurrentType()))
		{
			Neighbor->GlowPlantableAvailable();
			HighlightedPlantables.Add(Neighbor);
		}
	}
}

void UML_HoverPreviewComponent::ClearPlantableHighlight()
{
	for (AML_Tile* Tile : HighlightedPlantables)
		if (IsValid(Tile))
			Tile->StopGlowingPlantableAvailable();
	HighlightedPlantables.Reset();
}

bool UML_HoverPreviewComponent::ShouldHighlightPlantableNow() const
{
	// Gamepad-only visual, and only while previews are enabled (hidden during cinematics).
	if (!bShowPreviews || bCursorHoverEnabled || !IsValid(OwningController))
		return false;

	if (OwningController->GetMovementMode() != EML_PlayerMovementMode::InsideBoard)
		return false;

	// Only when the player is stopped. Use IsMoveInProgress() (already false once a move commits)
	// rather than the Idle state: when a move finishes, OnBoardActivityStateChanged(false) is broadcast
	// just before the state flips to Idle, so testing == Idle here would miss the highlight at the stop.
	// Exclude the turn-to-plant state (input locked) explicitly.
	return !OwningController->IsMoveInProgress() &&
		OwningController->GetBoardActionState() != EML_PlayerBoardActionState::TurningToPlant;
}

void UML_HoverPreviewComponent::HandleBoardActivityStateChanged(bool bIsMoving)
{
	// Move start clears the selection; move stop re-offers a default one. Plantables follow too.
	RefreshGamepadBoardVisuals();
}

void UML_HoverPreviewComponent::HandleWavePropagationFinished()
{
	// Tiles may have changed type during the propagation — recompute selection + plantables once.
	RefreshGamepadBoardVisuals();
}

void UML_HoverPreviewComponent::HandleRollbackAnimating(bool bAnimating)
{
	if (bAnimating)
	{
		// Hide the board visuals while the undo/reset animation reverts tiles.
		ClearGamepadSelection();
		ClearPlantableHighlight();
		ClearExitHighlight();
	}
	else
	{
		// Animation done — the board is back to its restored state, recompute from it.
		RefreshGamepadBoardVisuals();
	}
}

void UML_HoverPreviewComponent::SetHoveredTileState(AML_Tile* HoveredTile, bool bIsReachable)
{
	bCurrentHoveredTileReachable = bIsReachable;
	OnHoveredTileChanged.Broadcast(HoveredTile, bIsReachable);
}

void UML_HoverPreviewComponent::TickPathHoverPreview()
{
	if (!IsValid(OwningController))
        return;

	// ForcedHoverTile takes priority; in gamepad mode the physical cursor position is ignored.
	AML_Tile* HoveredTile = ForcedHoverTile
		? ForcedHoverTile
		: (bCursorHoverEnabled ? OwningController->GetTileUnderCursor() : nullptr);

    // Same tile as before → no update needed unless it became non-walkable.
    if (HoveredTile == LastPathHoveredTile)
    {
        if (IsValid(HoveredTile) && bCurrentHoveredTileReachable && !UML_HexPathfinder::IsTileWalkable(HoveredTile))
        {
            ClearPathHoverPreview();
            ClearCursorHoverPreview();
        }
        return;
    }

    // Tile changed — clear old path visuals immediately
    for (AML_Tile* Tile : CurrentPreviewPath)
        if (IsValid(Tile)) Tile->StopGlowingPathWalk();
    CurrentPreviewPath.Empty();

    LastPathHoveredTile = HoveredTile;

    if (!IsValid(HoveredTile))
    {
        SetHoveredTileState(nullptr, false);
        return;
    }

    AML_Tile* StartTile = nullptr;
    const AML_BoardSpawner* Board = HoveredTile->GetBoardSpawnerFromTile();
    
    if (!IsValid(Board))
    {
        SetHoveredTileState(HoveredTile, false);
        return;
    }

    // Per-board glow switch: no path preview when this board's glow is disabled.
    if (!Board->IsGlowEnabled())
    {
        SetHoveredTileState(HoveredTile, false);
        return;
    }

    // If the player is on the board, use their current tile
    if (IsValid(PlayerCharacter) && IsValid(PlayerCharacter->CurrentTileOn) &&
        PlayerCharacter->CurrentTileOn->GetBoardSpawnerFromTile() == Board)
    {
        StartTile = PlayerCharacter->CurrentTileOn;
    }
    // Otherwise, predict the first border tile that the NavMesh path would enter through.
    else if (IsValid(PlayerCharacter))
    {
        StartTile = OwningController->PredictNavMeshEntryTile(Board, HoveredTile->GetActorLocation());
    }

    if (!IsValid(StartTile))
    {
        SetHoveredTileState(HoveredTile, false);
        return;
    }

    // Check that the hovered tile is walkable
    if (!UML_HexPathfinder::IsTileWalkable(HoveredTile))
    {
        SetHoveredTileState(HoveredTile, false);
        return;
    }

    // Build preview path
    TArray<AML_Tile*> NewPath = BuildPreviewPathFromTile(StartTile, HoveredTile);
    const bool bReachable = NewPath.Num() > 0;

    SetHoveredTileState(HoveredTile, bReachable);

	const bool bIsOnPlayerTile = IsValid(PlayerCharacter->CurrentTileOn) && HoveredTile == PlayerCharacter->CurrentTileOn;

    if (bReachable && !bIsOnPlayerTile)
    {
        for (AML_Tile* Tile : NewPath)
            Tile->GlowPathWalk();
        CurrentPreviewPath = NewPath;
    }
}

void UML_HoverPreviewComponent::ClearPathHoverPreview()
{
	for (AML_Tile* Tile : CurrentPreviewPath)
		if (IsValid(Tile)) Tile->StopGlowingPathWalk();
	CurrentPreviewPath.Empty();

	if (LastPathHoveredTile != nullptr)
		SetHoveredTileState(nullptr, false);

	LastPathHoveredTile = nullptr;
}

TArray<AML_Tile*> UML_HoverPreviewComponent::BuildPreviewPathFromTile(const AML_Tile* StartTile, const AML_Tile* TargetTile) const
{
	TArray<AML_Tile*> Result;

	if (!IsValid(StartTile) || !IsValid(TargetTile))
		return Result;

	const AML_BoardSpawner* Board = StartTile->GetBoardSpawnerFromTile();
	if (!IsValid(Board))
		return Result;

	const TMap<FIntPoint, AML_Tile*> GridMap = Board->GetGridMap();
	const FIntPoint StartAxial = StartTile->GetAxialCoord();
	const FIntPoint GoalAxial = TargetTile->GetAxialCoord();

	if (!GridMap.Contains(StartAxial) || !GridMap.Contains(GoalAxial))
		return Result;

	if (!UML_HexPathfinder::IsTileWalkable(GridMap[StartAxial]) || !UML_HexPathfinder::IsTileWalkable(GridMap[GoalAxial]))
		return Result;

	TArray<FIntPoint> AxialPath;
	if (!UML_HexPathfinder::BuildPath_AxialBFS(StartAxial, GoalAxial, GridMap, AxialPath))
		return Result;

	Result.Reserve(AxialPath.Num());
	for (const FIntPoint& Axial : AxialPath)
	{
		if (AML_Tile* const* TilePtr = GridMap.Find(Axial))
		{
			if (IsValid(*TilePtr))
				Result.Add(*TilePtr);
		}
	}

	return Result;
}
