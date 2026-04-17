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
		ClearHoverPreview();
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
}

void UML_HoverPreviewComponent::UpdateHoverPreview()
{
	// In board modes, update both hover and cursor preview
	if (CurrentMovementMode == EML_PlayerMovementMode::InsideBoard ||
		CurrentMovementMode == EML_PlayerMovementMode::ExitingBoard)
	{
		TickHoverPreview();
		TickCursorHoverPreview();
	}
	// In free movement, only update cursor glow (no path preview)
	else if (CurrentMovementMode == EML_PlayerMovementMode::FreeMovement)
	{
		TickCursorHoverPreview();
	}
}

void UML_HoverPreviewComponent::TickCursorHoverPreview()
{
	if (!IsValid(OwningController))
		return;

	AML_Tile* CursorHoveredTile = OwningController->GetTileUnderCursor();

	if (CursorHoveredTile == LastCursorHoveredTile)
		return;

	if (!IsValid(CursorHoveredTile))
	{
		ClearCursorHoverPreview();
		return;
	}

	if (IsValid(LastCursorHoveredTile))
		LastCursorHoveredTile->StopGlowingCursorUnhovered();

	const bool bIsOnPlayerTile = IsValid(PlayerCharacter) &&
								  IsValid(PlayerCharacter->CurrentTileOn) &&
								  CursorHoveredTile == PlayerCharacter->CurrentTileOn;

	// Suppress cursor glow when the tile is unreachable in board mode.
	// TickHoverPreview already ran this frame and set bCurrentHoveredTileReachable.
	// Also suppress if hovering the player's current tile.
	const bool bIsInBoardMode = (CurrentMovementMode == EML_PlayerMovementMode::InsideBoard ||
								 CurrentMovementMode == EML_PlayerMovementMode::ExitingBoard);

	if (!bIsOnPlayerTile && (!bIsInBoardMode || bCurrentHoveredTileReachable))
		CursorHoveredTile->GlowCursorHovered();

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

void UML_HoverPreviewComponent::TickHoverPreview()
{
	// Only preview in board mode
	if (CurrentMovementMode != EML_PlayerMovementMode::InsideBoard &&
		CurrentMovementMode != EML_PlayerMovementMode::ExitingBoard)
	{
		ClearHoverPreview();
		return;
	}

	if (!IsValid(PlayerCharacter) || !IsValid(PlayerCharacter->CurrentTileOn))
	{
		ClearHoverPreview();
		return;
	}

	if (!IsValid(OwningController))
		return;

	AML_Tile* HoveredTile = OwningController->GetTileUnderCursor();

	// Same tile as before → no update needed
	if (HoveredTile == LastHoveredTile)
		return;

	// Tile changed — clear old path visuals immediately
	for (AML_Tile* Tile : CurrentPreviewPath)
		if (IsValid(Tile)) Tile->StopGlowingPathWalk();
	CurrentPreviewPath.Empty();

	LastHoveredTile = HoveredTile;

	if (!IsValid(HoveredTile))
	{
		bCurrentHoveredTileReachable = false;
		OnHoveredTileChanged.Broadcast(nullptr, false);
		return;
	}

	AML_BoardSpawner* Board = PlayerCharacter->CurrentTileOn->GetBoardSpawnerFromTile();
	if (!IsValid(Board) || HoveredTile->GetOwner() != Board || !UML_HexPathfinder::IsTileWalkable(HoveredTile))
	{
		bCurrentHoveredTileReachable = false;
		OnHoveredTileChanged.Broadcast(HoveredTile, false);
		return;
	}

	// Build preview path — empty result means the path is blocked by an obstacle
	TArray<AML_Tile*> NewPath = BuildPreviewPath(HoveredTile);
	const bool bReachable = NewPath.Num() > 0;

	bCurrentHoveredTileReachable = bReachable;
	OnHoveredTileChanged.Broadcast(HoveredTile, bReachable);

	const bool bIsOnPlayerTile = IsValid(PlayerCharacter) &&
								  IsValid(PlayerCharacter->CurrentTileOn) &&
								  HoveredTile == PlayerCharacter->CurrentTileOn;

	if (bReachable)
	{
		for (AML_Tile* Tile : NewPath)
			if (!bIsOnPlayerTile)
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
	{
		bCurrentHoveredTileReachable = false;
		OnHoveredTileChanged.Broadcast(nullptr, false);
	}

	LastHoveredTile = nullptr;
	bCurrentHoveredTileReachable = false;
}

TArray<AML_Tile*> UML_HoverPreviewComponent::BuildPreviewPath(const AML_Tile* TargetTile) const
{
	TArray<AML_Tile*> Result;

	if (!IsValid(TargetTile) || !IsValid(PlayerCharacter) || !IsValid(PlayerCharacter->CurrentTileOn))
		return Result;

	AML_BoardSpawner* Board = PlayerCharacter->CurrentTileOn->GetBoardSpawnerFromTile();
	if (!IsValid(Board))
		return Result;

	const TMap<FIntPoint, AML_Tile*> GridMap = Board->GetGridMap();
	const FIntPoint StartAxial = PlayerCharacter->CurrentTileOn->GetAxialCoord();
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
