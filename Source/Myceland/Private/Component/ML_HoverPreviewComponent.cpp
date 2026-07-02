// Copyright Myceland Team, All Rights Reserved.

#include "Component/ML_HoverPreviewComponent.h"

#include "Engine/Engine.h"
#include "Player/ML_PlayerController.h"
#include "Player/ML_PlayerCharacter.h"
#include "Player/ML_HexPathfinder.h"
#include "Tiles/ML_Tile.h"
#include "Tiles/ML_BoardSpawner.h"

void UML_HoverPreviewComponent::Initialize(AML_PlayerController* Controller, AML_PlayerCharacter* Character)
{
	OwningController = Controller;
	PlayerCharacter = Character;
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
}

void UML_HoverPreviewComponent::StartHoverPreviewTimer()
{
	if (!HoverPreviewTimerHandle.IsValid())
	{
		GetWorld()->GetTimerManager().SetTimer(
			HoverPreviewTimerHandle,
			this,
			&UML_HoverPreviewComponent::UpdateHoverPreview,
			1.f / 30.f, // 30Hz is enough for hover preview
			true
		);
	}
}

void UML_HoverPreviewComponent::StopHoverPreviewTimer()
{
	GetWorld()->GetTimerManager().ClearTimer(HoverPreviewTimerHandle);
	ClearPathHoverPreview();
	ClearCursorHoverPreview();
	ClearForcedHoverTile();
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
	// Force recalculation to return to normal cursor hover
	LastPathHoveredTile = nullptr;
	if (HoverPreviewTimerHandle.IsValid())
		TickPathHoverPreview();
}

void UML_HoverPreviewComponent::NotifyInputDeviceChanged(EML_InputDevice NewDevice)
{
	bCursorHoverEnabled = (NewDevice == EML_InputDevice::MouseKeyboard);

	// Switching to gamepad: clear any cursor-driven hover that was still active.
	if (!bCursorHoverEnabled)
		ClearCursorHoverPreview();
}

void UML_HoverPreviewComponent::UpdateShowPreviews(const bool Value)
{
	bShowPreviews = Value;
	
	// Disable previews if `bShowPreviews` is false
	if (!bShowPreviews)
	{
		ClearPathHoverPreview();
		ClearCursorHoverPreview();
	}
}

void UML_HoverPreviewComponent::UpdateHoverPreview()
{
	UpdateCursorOverEmptySpace();
	
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

void UML_HoverPreviewComponent::UpdateCursorOverEmptySpace()
{
	if (!IsValid(OwningController)) return;
	
	// ExitingBoard counts as a board mode too: while the player holds the exit, the cursor is still
	// over empty space and the exit cursor must stay visible. RequestExitHold switches the mode to
	// ExitingBoard the instant the hold begins, so checking only InsideBoard would wrongly flip to false.
	const bool bInBoardMode = CurrentMovementMode == EML_PlayerMovementMode::InsideBoard || CurrentMovementMode == EML_PlayerMovementMode::ExitingBoard;
	const bool bOverEmptySpace = bInBoardMode && bCursorHoverEnabled && !IsValid(OwningController->GetTileUnderCursor()); // Player in a board mode AND is using mouse AND tile under cursor is not valid
	
	if (bOverEmptySpace != bWasCursorOverEmptySpace)
	{
		bWasCursorOverEmptySpace = bOverEmptySpace;
		OnCursorOverEmptySpaceChanged.Broadcast(bOverEmptySpace);
	}
}
