// Copyright Myceland Team, All Rights Reserved.

#include "Component/ML_HoverPreviewComponent.h"

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
		ClearHoverPreview();
		ClearForcedHoverTile();
	}
}

void UML_HoverPreviewComponent::NotifyPlayerTileChanged()
{
	// If the player just arrived on the highlighted tile, clear its glow immediately.
	if (IsValid(PlayerCharacter) && IsValid(PlayerCharacter->CurrentTileOn) &&
		(ForcedHoverTile == PlayerCharacter->CurrentTileOn ||
		 LastCursorHoveredTile == PlayerCharacter->CurrentTileOn))
	{
		ClearCursorHoverPreview();
	}

	// Force the preview path update from the new position.
	LastHoveredTile = nullptr;
	if (HoverPreviewTimerHandle.IsValid())
		TickHoverPreview();
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
	ClearHoverPreview();
	ClearCursorHoverPreview();
	ClearForcedHoverTile();
}

// Bypasses cursor detection and locks the hover preview onto the given tile until cleared.
// Invalidates LastHoveredTile so TickHoverPreview immediately recomputes the path.
void UML_HoverPreviewComponent::SetForcedHoverTile(AML_Tile* Tile)
{
	ForcedHoverTile = Tile;
	// Force immediate recalculation
	LastHoveredTile = nullptr;
	if (HoverPreviewTimerHandle.IsValid())
		TickHoverPreview();
}

// Removes the forced tile override and hands control back to cursor-based detection.
void UML_HoverPreviewComponent::ClearForcedHoverTile()
{
	ForcedHoverTile = nullptr;
	// Force recalculation to return to normal cursor hover
	LastHoveredTile = nullptr;
	if (HoverPreviewTimerHandle.IsValid())
		TickHoverPreview();
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
		ClearHoverPreview();
		ClearCursorHoverPreview();
	}
}

void UML_HoverPreviewComponent::UpdateHoverPreview()
{
	// Do not show preview if `bShowPreviews` is false
	if (!bShowPreviews) return;

	TickHoverPreview();
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

	if (IsValid(LastCursorHoveredTile))
		LastCursorHoveredTile->StopGlowingCursorUnhovered();

	if (bCurrentHoveredTileReachable)
	{
		const bool bIsPlayerTile = IsValid(PlayerCharacter) && IsValid(PlayerCharacter->CurrentTileOn) &&
		                           CursorHoveredTile == PlayerCharacter->CurrentTileOn;
		CursorHoveredTile->GlowCursorHovered(bIsPlayerTile);
	}

	LastCursorHoveredTile = CursorHoveredTile;
}

void UML_HoverPreviewComponent::ClearCursorHoverPreview()
{
	if (IsValid(LastCursorHoveredTile))
	{
		LastCursorHoveredTile->StopGlowingCursorUnhovered();
		LastCursorHoveredTile = nullptr;
	}
}

void UML_HoverPreviewComponent::SetHoveredTileState(AML_Tile* HoveredTile, bool bIsReachable)
{
	bCurrentHoveredTileReachable = bIsReachable;
	OnHoveredTileChanged.Broadcast(HoveredTile, bIsReachable);
}

void UML_HoverPreviewComponent::TickHoverPreview()
{
	if (!IsValid(OwningController))
        return;

	// ForcedHoverTile takes priority; in gamepad mode the physical cursor position is ignored.
	AML_Tile* HoveredTile = ForcedHoverTile
		? ForcedHoverTile
		: (bCursorHoverEnabled ? OwningController->GetTileUnderCursor() : nullptr);

    // Same tile as before → no update needed unless it became non-walkable.
    if (HoveredTile == LastHoveredTile)
    {
        if (IsValid(HoveredTile) && bCurrentHoveredTileReachable && !UML_HexPathfinder::IsTileWalkable(HoveredTile))
        {
            ClearHoverPreview();
            ClearCursorHoverPreview();
        }
        return;
    }

    // Tile changed — clear old path visuals immediately
    for (AML_Tile* Tile : CurrentPreviewPath)
        if (IsValid(Tile)) Tile->StopGlowingPathWalk();
    CurrentPreviewPath.Empty();

    LastHoveredTile = HoveredTile;

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

void UML_HoverPreviewComponent::ClearHoverPreview()
{
	for (AML_Tile* Tile : CurrentPreviewPath)
		if (IsValid(Tile)) Tile->StopGlowingPathWalk();
	CurrentPreviewPath.Empty();

	if (LastHoveredTile != nullptr)
		SetHoveredTileState(nullptr, false);

	LastHoveredTile = nullptr;
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
