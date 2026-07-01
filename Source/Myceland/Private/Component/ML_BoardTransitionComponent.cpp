// Copyright Myceland Team, All Rights Reserved.

#include "Component/ML_BoardTransitionComponent.h"

#include "Core/ML_TileTypeTraits.h"
#include "Developer Settings/ML_MycelandDeveloperSettings.h"
#include "Player/ML_HexPathfinder.h"
#include "Player/ML_PlayerCharacter.h"
#include "Player/ML_PlayerController.h"
#include "Tiles/ML_BoardSpawner.h"
#include "Tiles/ML_Tile.h"

// ==================== Lifecycle ====================

void UML_BoardTransitionComponent::Initialize(AML_PlayerController* Controller, AML_PlayerCharacter* Character, const UML_MycelandDeveloperSettings* Settings, float InRotateSpeed)
{
	OwningController = Controller;
	PlayerCharacter  = Character;
	DevSettings      = Settings;
	RotateSpeed      = InRotateSpeed;
}

// ==================== Mode management ====================

void UML_BoardTransitionComponent::SwitchToMode(EML_PlayerMovementMode NewMode)
{
	const EML_PlayerMovementMode OldMode = CurrentMovementMode;
	CurrentMovementMode = NewMode;

	// ExitingBoard → InsideBoard: this is a CANCELLED exit — clean up exit state
	if (OldMode == EML_PlayerMovementMode::ExitingBoard &&
		NewMode  == EML_PlayerMovementMode::InsideBoard &&
		!bPendingFreeMovementOnArrival)
	{
		PendingExitBorderTile = nullptr;
		bHasExitTargetWorld = false;
	}
}

void UML_BoardTransitionComponent::NotifyIsMoving(bool bIsMoving)
{
	// Only broadcast when inside the board
	if (CurrentMovementMode != EML_PlayerMovementMode::InsideBoard)
	{
		bWasMovingInBoard = false;
		return;
	}

	if (bIsMoving != bWasMovingInBoard)
	{
		OnBoardActivityStateChanged.Broadcast(bIsMoving);
		bWasMovingInBoard = bIsMoving;
	}
}

bool UML_BoardTransitionComponent::IsOutsideBoardMovementMode() const
{
	return CurrentMovementMode == EML_PlayerMovementMode::FreeMovement ||
		CurrentMovementMode == EML_PlayerMovementMode::EnteringBoard;
}

void UML_BoardTransitionComponent::ForceFreeMovement()
{
	// Clear the exit-hold timer + state (SetMovementMode alone would leak the running timer).
	if (UWorld* World = GetWorld())
		World->GetTimerManager().ClearTimer(ExitHoldTimerHandle);

	if (bIsHoldingExitInput || bWasExitingLastFrame)
		OnExitCursorHold.Broadcast(false, 0.f);

	ExitHoldTimer         = 0.f;
	bIsHoldingExitInput   = false;
	bWasExitingLastFrame  = false;
	bHasExitTargetWorld   = false;
	LastBroadcastProgress = -1.f;
	PendingExitBorderTile = nullptr;

	// Clear pending board-entry state.
	bPendingBoardEntryOnArrival   = false;
	PendingBoardEntryTargetTile   = nullptr;
	bPendingFreeMovementOnArrival = false;

	OwningController->SetMovementMode(EML_PlayerMovementMode::FreeMovement);
}

// ==================== Exit hold ====================

void UML_BoardTransitionComponent::RequestExitHold(AML_Tile* ExitBorderTile, const FVector& WorldTarget)
{
	PendingExitBorderTile = ExitBorderTile;
	PendingExitTargetWorld = WorldTarget;
	bHasExitTargetWorld    = true;

	OwningController->SetMovementMode(EML_PlayerMovementMode::ExitingBoard);
	bIsHoldingExitInput = true;

	// Start exit hold timer
	ExitHoldTimer        = 0.f;
	bWasExitingLastFrame = false;
	LastBroadcastProgress = -1.f;

	GetWorld()->GetTimerManager().SetTimer(
		ExitHoldTimerHandle,
		this,
		&UML_BoardTransitionComponent::TickExitHold,
		1.f / 60.f,
		true
	);
}

void UML_BoardTransitionComponent::CancelExitHold()
{
	bIsHoldingExitInput = false;
	// TickExitHold detects the cleared flag on the next tick and performs cleanup.
}

void UML_BoardTransitionComponent::TickExitHold()
{
	if (!IsValid(OwningController) || !IsValid(DevSettings))
	{
		GetWorld()->GetTimerManager().ClearTimer(ExitHoldTimerHandle);
		bIsHoldingExitInput = false;
		return;
	}

	const bool bIsCurrentlyExiting = (CurrentMovementMode == EML_PlayerMovementMode::ExitingBoard && bIsHoldingExitInput);

	// Not exiting anymore — cancellation
	if (!bIsCurrentlyExiting)
	{
		if (CurrentMovementMode == EML_PlayerMovementMode::ExitingBoard)
		{
			GetWorld()->GetTimerManager().ClearTimer(ExitHoldTimerHandle);
			ExitHoldTimer = 0.f;

			OnExitCursorHold.Broadcast(false, 0.0f);
			bWasExitingLastFrame  = false;
			LastBroadcastProgress = -1.f;

			// Hover cleanup on cancel is the input handler's responsibility (mouse clears, gamepad keeps).

			// Return to board — SwitchToMode handles clearing exit state
			OwningController->SetMovementMode(EML_PlayerMovementMode::InsideBoard);
		}
		return;
	}

	// Calculate progress
	const float Progress = FMath::Clamp(ExitHoldTimer / DevSettings->ExitBoardHoldDuration, 0.f, 1.f);

	const bool bJustStartedExiting = !bWasExitingLastFrame;
	const bool bProgressChanged    = FMath::Abs(Progress - LastBroadcastProgress) > 0.01f;

	if (bJustStartedExiting || bProgressChanged)
	{
		OnExitCursorHold.Broadcast(true, Progress);
		LastBroadcastProgress = Progress;
	}

	bWasExitingLastFrame = true;
	ExitHoldTimer += 1.f / 60.f;

	if (ExitHoldTimer >= DevSettings->ExitBoardHoldDuration)
	{
		GetWorld()->GetTimerManager().ClearTimer(ExitHoldTimerHandle);
		ExitHoldTimer         = 0.f;
		bWasExitingLastFrame  = false;
		LastBroadcastProgress = -1.f;

		OnExitCursorHold.Broadcast(false, 1.0f);
		
		OwningController->ClearForcedHoverTile();
		ConfirmExitBoard();
	}
}

void UML_BoardTransitionComponent::ConfirmExitBoard()
{
	UE_LOG(LogTemp, Warning, TEXT("[EXIT] ConfirmExitBoard called"));

	if (!IsValid(PendingExitBorderTile) || !IsValid(PlayerCharacter) || !IsValid(PlayerCharacter->CurrentTileOn)) return;

	AML_BoardSpawner* Board = PlayerCharacter->CurrentTileOn->GetBoardSpawnerFromTile();
	if (!IsValid(Board)) return;

	const TMap<FIntPoint, AML_Tile*> GridMap = Board->GetGridMap();
	const FIntPoint StartAxial = PlayerCharacter->CurrentTileOn->GetAxialCoord();
	const FIntPoint GoalAxial  = PendingExitBorderTile->GetAxialCoord();

	UE_LOG(LogTemp, Warning, TEXT("[EXIT] StartAxial=%s, GoalAxial=%s"),
		*StartAxial.ToString(), *GoalAxial.ToString());

	if (StartAxial == GoalAxial)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EXIT] Direct exit (already on border tile)"));
		OwningController->SetMovementMode(EML_PlayerMovementMode::FreeMovement);

		if (bHasExitTargetWorld)
		{
			OwningController->StartNavMeshMovement(PendingExitTargetWorld);
			bHasExitTargetWorld = false;
		}

		PendingExitBorderTile = nullptr;
		return;
	}

	if (!GridMap.Contains(StartAxial) || !GridMap.Contains(GoalAxial)) return;

	TArray<FIntPoint> AxialPath;
	if (!UML_HexPathfinder::BuildPath_AxialBFS(StartAxial, GoalAxial, GridMap, AxialPath)) return;

	UE_LOG(LogTemp, Warning, TEXT("[EXIT] Path to border. Length=%d, bHasExitTargetWorld=%d"),
		AxialPath.Num(), bHasExitTargetWorld);

	bPendingFreeMovementOnArrival = true;
	// Walk to border inside board; FreeMovement triggers on arrival via HandlePathFinished
	OwningController->SetMovementMode(EML_PlayerMovementMode::InsideBoard);
	// Don't clear pending exit state: HandlePathFinished needs PendingExitTargetWorld.

	OwningController->StartMoveAlongPath(AxialPath, GridMap);
}

// ==================== Board entry / action state ====================

void UML_BoardTransitionComponent::SetBoardActionState(EML_PlayerBoardActionState State, AML_Tile* PlantTarget)
{
	BoardActionState       = State;
	PendingPlantTargetTile = PlantTarget;
}

void UML_BoardTransitionComponent::RequestBoardEntry(AML_Tile* TargetTile)
{
	// Per-board transition switch: if the target board has entry/exit disabled, do nothing.
	// The NavMesh move issued by the caller still runs, so the player walks to the tile in free movement.
	const AML_BoardSpawner* Board = IsValid(TargetTile) ? TargetTile->GetBoardSpawnerFromTile() : nullptr;
	if (IsValid(Board) && !Board->IsBoardTransitionEnabled()) return;

	PendingBoardEntryTargetTile = TargetTile;
	bPendingBoardEntryOnArrival = true;
	OwningController->SetMovementMode(EML_PlayerMovementMode::EnteringBoard);
}

void UML_BoardTransitionComponent::CancelPendingBoardEntry()
{
	if (!bPendingBoardEntryOnArrival) return;

	bPendingBoardEntryOnArrival = false;
	PendingBoardEntryTargetTile = nullptr;

	if (CurrentMovementMode == EML_PlayerMovementMode::EnteringBoard)
		OwningController->SetMovementMode(EML_PlayerMovementMode::FreeMovement);
}

// ==================== Turn toward tile ====================

void UML_BoardTransitionComponent::StartTurnTowardTile(AML_Tile* Target)
{
	PendingPlantTargetTile = Target;
	BoardActionState       = EML_PlayerBoardActionState::TurningToPlant;
	OnBoardActivityStateChanged.Broadcast(true);

	if (!TurnTowardTileTimerHandle.IsValid())
	{
		GetWorld()->GetTimerManager().SetTimer(
			TurnTowardTileTimerHandle,
			this,
			&UML_BoardTransitionComponent::UpdateTurnTowardPendingTile,
			1.f / 60.f,
			true
		);
	}
}

bool UML_BoardTransitionComponent::RotateCharacterTowardTile(const AML_Tile* Target, float DeltaTime, float TurnSpd) const
{
	if (!IsValid(PlayerCharacter) || !IsValid(Target)) return false;

	FVector Dir = Target->GetActorLocation() - PlayerCharacter->GetActorLocation();
	Dir.Z = 0.f;

	if (Dir.IsNearlyZero())
		return false;

	const float DesiredYaw      = Dir.Rotation().Yaw;
	const FRotator CurrentRot   = PlayerCharacter->GetActorRotation();
	const FRotator DesiredRot(0.f, DesiredYaw, 0.f);
	const FRotator NewRot       = FMath::RInterpTo(CurrentRot, DesiredRot, DeltaTime, TurnSpd);

	PlayerCharacter->SetActorRotation(NewRot);

	const float YawError = FMath::Abs(FMath::FindDeltaAngleDegrees(NewRot.Yaw, DesiredYaw));
	return YawError > 1.0f; // true = still turning
}

void UML_BoardTransitionComponent::UpdateTurnTowardPendingTile()
{
	if (BoardActionState != EML_PlayerBoardActionState::TurningToPlant ||
		!IsValid(PendingPlantTargetTile) || !IsValid(PlayerCharacter))
	{
		GetWorld()->GetTimerManager().ClearTimer(TurnTowardTileTimerHandle);
		BoardActionState = EML_PlayerBoardActionState::Idle;
		return;
	}

	const bool bStillTurning = RotateCharacterTowardTile(PendingPlantTargetTile, 1.f / 60.f, RotateSpeed);

	if (!bStillTurning)
	{
		GetWorld()->GetTimerManager().ClearTimer(TurnTowardTileTimerHandle);
		BoardActionState = EML_PlayerBoardActionState::Idle;
		OnBoardActivityStateChanged.Broadcast(false);

		AML_Tile* Target = PendingPlantTargetTile;
		PendingPlantTargetTile = nullptr;

		if (OwningController->HasEnergy())
			OwningController->ConfirmTurn(Target);
	}
}

// ==================== Path completion ====================

FBoardTransitionCommand UML_BoardTransitionComponent::HandlePathFinished(AML_PlayerCharacter* Character)
{
	FBoardTransitionCommand Cmd;

	// Case 1: We just walked to the board border — switch to free movement
	if (bPendingFreeMovementOnArrival)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EXIT] OnPathFinished: bPendingFreeMovementOnArrival=true"));

		bPendingFreeMovementOnArrival = false;
		OwningController->SetMovementMode(EML_PlayerMovementMode::FreeMovement);

		if (bHasExitTargetWorld)
		{
			UE_LOG(LogTemp, Warning, TEXT("[EXIT] Returning StartNavMesh command to %s"),
				*PendingExitTargetWorld.ToString());
			Cmd.bStartNavMesh   = true;
			Cmd.NavMeshTarget   = PendingExitTargetWorld;
			bHasExitTargetWorld = false;
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[EXIT] ERROR: bHasExitTargetWorld is FALSE!"));
		}

		PendingExitBorderTile = nullptr;
		return Cmd;
	}

	// Case 2: We arrived at the board entry tile via NavMesh — start tile-by-tile to actual target
	if (bPendingBoardEntryOnArrival)
	{
		bPendingBoardEntryOnArrival = false;
		OwningController->SetMovementMode(EML_PlayerMovementMode::InsideBoard);

		if (!IsValid(Character) || !IsValid(Character->CurrentTileOn)) return Cmd;

		AML_BoardSpawner* Board = Character->CurrentTileOn->GetBoardSpawnerFromTile();
		if (!IsValid(Board)) return Cmd;

		AML_Tile* TargetTile = PendingBoardEntryTargetTile;
		PendingBoardEntryTargetTile = nullptr;

		if (!IsValid(TargetTile) || TargetTile->GetOwner() != Board) return Cmd;

		const TMap<FIntPoint, AML_Tile*> GridMap = Board->GetGridMap();
		const FIntPoint StartAxial = Character->CurrentTileOn->GetAxialCoord();
		const FIntPoint GoalAxial  = TargetTile->GetAxialCoord();

		if (!GridMap.Contains(StartAxial) || !GridMap.Contains(GoalAxial)) return Cmd;
		if (!UML_HexPathfinder::IsTileWalkable(GridMap[StartAxial]) ||
			!UML_HexPathfinder::IsTileWalkable(GridMap[GoalAxial])) return Cmd;

		TArray<FIntPoint> AxialPath;
		if (!UML_HexPathfinder::BuildPath_AxialBFS(StartAxial, GoalAxial, GridMap, AxialPath)) return Cmd;

		BoardActionState = EML_PlayerBoardActionState::Moving;

		Cmd.bStartBoardPath = true;
		Cmd.BoardPath       = AxialPath;
		Cmd.BoardGridMap    = GridMap;
		return Cmd;
	}

	// Case 3: Arrived at tile adjacent to plant target — initiate turn
	if (BoardActionState == EML_PlayerBoardActionState::MovingToPlant)
	{
		if (!IsValid(PendingPlantTargetTile) || !IsValid(Character) || !IsValid(Character->CurrentTileOn))
		{
			PendingPlantTargetTile = nullptr;
			BoardActionState = EML_PlayerBoardActionState::Idle;
			return Cmd;
		}

		AML_BoardSpawner* Board = Character->CurrentTileOn->GetBoardSpawnerFromTile();
		if (!IsValid(Board))
		{
			PendingPlantTargetTile = nullptr;
			BoardActionState = EML_PlayerBoardActionState::Idle;
			return Cmd;
		}

		TArray<AML_Tile*> Neighbors = Board->GetNeighbors(Character->CurrentTileOn);
		if (Neighbors.Contains(PendingPlantTargetTile) &&
			UML_TileTypeTraits::CanPlayerPlant(PendingPlantTargetTile->GetCurrentType()) &&
			OwningController->HasEnergy())
		{
			StartTurnTowardTile(PendingPlantTargetTile);
			return Cmd;
		}

		PendingPlantTargetTile = nullptr;
		BoardActionState = EML_PlayerBoardActionState::Idle;
		return Cmd;
	}

	BoardActionState = EML_PlayerBoardActionState::Idle;
	return Cmd;
}
