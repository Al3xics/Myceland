// Copyright Myceland Team, All Rights Reserved.

#include "Player/ML_PlayerController.h"

#include "AIController.h"
#include "NavigationSystem.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "Developer Settings/ML_MycelandDeveloperSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/ML_PlayerCharacter.h"
#include "Subsystem/ML_WavePropagationSubsystem.h"
#include "Tiles/ML_Tile.h"

class UML_WavePropagationSubsystem;

// ==================== Helpers ====================

AML_Tile* AML_PlayerController::GetTileUnderCursor() const
{
	FHitResult Hit;
	if (!GetHitResultUnderCursorByChannel(UEngineTypes::ConvertToTraceType(ECC_Visibility), true, Hit))
		return nullptr;

	if (AML_Tile* Tile = Cast<AML_Tile>(Hit.GetActor()))
		return Tile;

	if (UPrimitiveComponent* Comp = Hit.GetComponent())
		if (AML_Tile* OuterTile = Comp->GetTypedOuter<AML_Tile>())
			return OuterTile;

	if (AActor* HitActor = Hit.GetActor())
	{
		if (AActor* Parent = HitActor->GetParentActor())
			if (AML_Tile* ParentTile = Cast<AML_Tile>(Parent))
				return ParentTile;

		if (AActor* Owning = HitActor->GetOwner())
			if (AML_Tile* OwnerTile = Cast<AML_Tile>(Owning))
				return OwnerTile;
	}

	return nullptr;
}

bool AML_PlayerController::IsTileWalkable(const AML_Tile* Tile) const
{
	if (!IsValid(Tile)) return false;

	const EML_TileType Type = Tile->GetCurrentType();
	return (Type == EML_TileType::Dirt || Type == EML_TileType::Grass) && !Tile->IsBlocked();
}

AML_Tile* AML_PlayerController::FindNearestWalkableTile(const FVector& WorldLocation,
                                                        const TMap<FIntPoint, AML_Tile*>& GridMap) const
{
	AML_Tile* Best = nullptr;
	float BestDistSq = FLT_MAX;

	for (const TPair<FIntPoint, AML_Tile*>& Pair : GridMap)
	{
		AML_Tile* Tile = Pair.Value;
		if (!IsValid(Tile) || !IsTileWalkable(Tile))
			continue;

		const float DistSq = FVector::DistSquared2D(WorldLocation, Tile->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Tile;
		}
	}
	return Best;
}

void AML_PlayerController::SetIsMoving(bool bNewIsMoving)
{
	// Update internal state
	bIsMoving = bNewIsMoving;
	
	// Only broadcast when inside the board
	if (CurrentMovementMode != EML_PlayerMovementMode::InsideBoard)
	{
		bWasMovingInBoard = false;
		return;
	}
	
	// Only broadcast if the state actually changed (avoid spam)
	if (bIsMoving != bWasMovingInBoard)
	{
		OnBoardMovementStateChanged.Broadcast(bIsMoving);
		bWasMovingInBoard = bIsMoving;
	}
}

void AML_PlayerController::SetMovementMode(EML_PlayerMovementMode NewMode)
{
	if (CurrentMovementMode == NewMode)
		return;
	
	const EML_PlayerMovementMode OldMode = CurrentMovementMode;
	CurrentMovementMode = NewMode;
	
	// Handle transitions OUT of old mode
	switch (OldMode)
	{
	case EML_PlayerMovementMode::FreeMovement:
		// Leaving free movement
		StopNavMeshMovement();
		break;
		
	case EML_PlayerMovementMode::InsideBoard:
		// Leaving board - clear path preview but keep cursor glow
		ClearHoverPreview();
		break;
		
	case EML_PlayerMovementMode::ExitingBoard:
		// Only clean up if we're CANCELLING the exit (going back to InsideBoard)
		// Don't clean up if we're confirming the exit (going to FreeMovement or staying in InsideBoard for pathing)
		if (NewMode == EML_PlayerMovementMode::InsideBoard && !bPendingFreeMovementOnArrival)
		{
			// Cancelled exit - clean up
			PendingExitTile = nullptr;
			bHasExitTargetWorld = false;
		}
		// Otherwise, let ConfirmExitBoard handle the cleanup
		break;
		
	case EML_PlayerMovementMode::EnteringBoard:
		// Nothing special needed
		break;
	}
	
	// Handle transitions INTO new mode
	switch (NewMode)
	{
	case EML_PlayerMovementMode::FreeMovement:
		// Entering free movement - nav mesh will be started separately
		break;
		
	case EML_PlayerMovementMode::InsideBoard:
		// Entering board - tile-by-tile movement
		break;
		
	case EML_PlayerMovementMode::ExitingBoard:
		// Starting to exit
		break;
		
	case EML_PlayerMovementMode::EnteringBoard:
		// Starting to enter
		ClearHoverPreview();
		break;
	}
}


// ==================== Pathfinding ====================

bool AML_PlayerController::BuildPath_AxialBFS(const FIntPoint& StartAxial, const FIntPoint& GoalAxial,
                                              const TMap<FIntPoint, AML_Tile*>& GridMap,
                                              TArray<FIntPoint>& OutAxialPath) const
{
	OutAxialPath.Reset();

	if (StartAxial == GoalAxial)
	{
		OutAxialPath.Add(StartAxial);
		return true;
	}

	TArray<FIntPoint> Queue;
	Queue.Reserve(GridMap.Num());
	int32 Head = 0;

	TMap<FIntPoint, FIntPoint> CameFrom;
	CameFrom.Reserve(GridMap.Num());

	Queue.Add(StartAxial);
	CameFrom.Add(StartAxial, StartAxial);

	while (Head < Queue.Num())
	{
		const FIntPoint Current = Queue[Head++];

		for (const FIntPoint& Dir : Directions)
		{
			const FIntPoint Next = Current + Dir;

			if (CameFrom.Contains(Next))
				continue;

			const AML_Tile* const* NextTilePtr = GridMap.Find(Next);
			if (!NextTilePtr || !IsTileWalkable(*NextTilePtr))
				continue;

			CameFrom.Add(Next, Current);

			if (Next == GoalAxial)
			{
				FIntPoint Step = GoalAxial;
				while (Step != StartAxial)
				{
					OutAxialPath.Add(Step);
					Step = CameFrom[Step];
				}
				OutAxialPath.Add(StartAxial);
				Algo::Reverse(OutAxialPath);
				return true;
			}

			Queue.Add(Next);
		}
	}

	return false;
}


// ==================== Movement ====================

void AML_PlayerController::StartMoveAlongPath(const TArray<FIntPoint>& AxialPath,
                                              const TMap<FIntPoint, AML_Tile*>& GridMap)
{
	// Stop any nav mesh movement when starting tile-by-tile movement
	StopNavMeshMovement();
	
	CurrentPathWorld.Reset();
	CurrentPathIndex = 0;
	CurrentPathWorld.Reserve(AxialPath.Num());

	for (const FIntPoint& Axial : AxialPath)
		if (AML_Tile* const* TilePtr = GridMap.Find(Axial))
			if (IsValid(*TilePtr))
				CurrentPathWorld.Add((*TilePtr)->GetActorLocation());
	
	// If no valid path → not moving
	if (CurrentPathWorld.Num() == 0)
	{
		SetIsMoving(false);
		return;
	}

	// Skip first point if already there
	if (APawn* P = GetPawn())
		if (FVector::DistSquared2D(P->GetActorLocation(), CurrentPathWorld[0]) <= FMath::Square(AcceptanceRadius))
			CurrentPathIndex = 1;

	// If already at destination
	if (CurrentPathIndex >= CurrentPathWorld.Num())
	{
		CurrentPathWorld.Reset();
		CurrentPathIndex = 0;
		SetIsMoving(false);
		OnPathFinished();
		return;
	}
	
	SetIsMoving(true);
}

void AML_PlayerController::StartNavMeshMovement(const FVector& WorldLocation)
{
	if (!IsValid(MycelandCharacter))
		return;

	// Use SimpleMoveToLocation for nav mesh movement
	UAIBlueprintHelperLibrary::SimpleMoveToLocation(this, WorldLocation);
	
	PendingFreeMovementTarget = WorldLocation;
	bHasFreeMovementTarget = true;
	bIsUsingNavMeshMovement = true;
	SetIsMoving(true);
}

void AML_PlayerController::StopNavMeshMovement()
{
	if (bIsUsingNavMeshMovement)
	{
		// Stop the AI movement
		if (IsValid(MycelandCharacter))
		{
			if (UCharacterMovementComponent* MC = MycelandCharacter->GetCharacterMovement())
			{
				MC->StopMovementImmediately();
			}
		}
		
		bIsUsingNavMeshMovement = false;
		bHasFreeMovementTarget = false;
	}
}

void AML_PlayerController::TickNavMeshMovement(float DeltaTime)
{
	if (!bIsUsingNavMeshMovement || !bHasFreeMovementTarget)
		return;

	if (!IsValid(MycelandCharacter))
	{
		StopNavMeshMovement();
		return;
	}

	// Check if we've reached the destination
	const FVector CurrentLoc = MycelandCharacter->GetActorLocation();
	const float DistSq = FVector::DistSquared2D(CurrentLoc, PendingFreeMovementTarget);

	if (DistSq <= FMath::Square(NavMeshAcceptanceRadius))
	{
		StopNavMeshMovement();
		SetIsMoving(false);
		
		// Trigger OnPathFinished for board entry transitions
		if (bPendingBoardEntryOnArrival)
		{
			OnPathFinished();
		}
	}
}

void AML_PlayerController::TickMoveAlongPath(float DeltaTime)
{
	// Early exit if not moving
	if (!bIsMoving)
		return;
	
	// Handle nav mesh movement (FreeMovement and EnteringBoard modes)
	if (CurrentMovementMode == EML_PlayerMovementMode::FreeMovement || 
	    CurrentMovementMode == EML_PlayerMovementMode::EnteringBoard)
	{
		TickNavMeshMovement(DeltaTime);
		return;
	}

	// Tile-by-tile movement (InsideBoard and ExitingBoard modes)
	if (CurrentPathWorld.Num() == 0 || CurrentPathIndex >= CurrentPathWorld.Num()) return;

	ensureMsgf(MycelandCharacter, TEXT("Player Character is not set!"));
	if (!IsValid(MycelandCharacter)) return;

	const FVector Loc = MycelandCharacter->GetActorLocation();

	// ---------- Corner cut ----------
	if (CornerCutStrength > 0.f && CurrentPathIndex + 1 < CurrentPathWorld.Num())
	{
		const FVector A = FVector(CurrentPathWorld[CurrentPathIndex].X, CurrentPathWorld[CurrentPathIndex].Y, 0.f);
		const FVector B = FVector(CurrentPathWorld[CurrentPathIndex + 1].X, CurrentPathWorld[CurrentPathIndex + 1].Y,
		                          0.f);
		const FVector P = FVector(Loc.X, Loc.Y, 0.f);
		const FVector AB = B - A;
		const float AB2 = AB.SizeSquared();

		if (AB2 > KINDA_SMALL_NUMBER)
		{
			const float T = FMath::Clamp(FVector::DotProduct(P - A, AB) / AB2, 0.f, 1.f);
			if (FVector::Dist(P, A + T * AB) <= BaseCornerCutDistance * CornerCutStrength)
				CurrentPathIndex++;
		}
	}

	// ---------- Move toward current waypoint ----------
	FVector To = CurrentPathWorld[CurrentPathIndex] - Loc;
	To.Z = 0.f;
	const float Dist = To.Size();

	if (Dist <= AcceptanceRadius)
	{
		const int32 ReachedIndex = CurrentPathIndex;
		CurrentPathIndex++;

		// During undo-move playback, restore collectibles *behind the player*.
		// We restore when the player reaches a tile, meaning they just left the previous one.
		if (bUndoMovePlayback && bUndoRestoreCollectibles)
		{
			const int32 LeftIndex = ReachedIndex - 1; // tile behind the player
			if (LeftIndex >= 0 && LeftIndex < ActiveMoveAxialPath.Num())
			{
				const FIntPoint LeftAxial = ActiveMoveAxialPath[LeftIndex];

				if (UndoMoveRemainingCollectibles.Contains(LeftAxial))
				{
					if (UML_WavePropagationSubsystem* S = GetWorld()->GetSubsystem<UML_WavePropagationSubsystem>())
					{
						S->RestoreCollectibleDuringUndoMove(LeftAxial);
					}
					UndoMoveRemainingCollectibles.Remove(LeftAxial);
				}
			}
		}

		if (CurrentPathIndex >= CurrentPathWorld.Num())
		{
			if (UML_WavePropagationSubsystem* S = GetWorld()->GetSubsystem<UML_WavePropagationSubsystem>())
			{
				if (bUndoMovePlayback)
				{
					bUndoMovePlayback = false;
					bSuppressMoveRecording = false;

					// Restore any leftovers (e.g. original start tile depending on timing)
					if (bUndoRestoreCollectibles && UndoMoveRemainingCollectibles.Num() > 0)
					{
						for (const FIntPoint& Ax : UndoMoveRemainingCollectibles)
						{
							S->RestoreCollectibleDuringUndoMove(Ax);
						}
						UndoMoveRemainingCollectibles.Reset();
					}
					bUndoRestoreCollectibles = false;

					S->FinishUndoAnimation();
				}
				else if (bMoveInProgress && ActiveMoveAxialPath.Num() > 0)
				{
					if (!bSuppressMoveRecording)
					{
						const TArray<FIntPoint> Picked = ActiveMovePickedCollectibles.Array();
						AML_PlayerCharacter* PC = Cast<AML_PlayerCharacter>(GetPawn());
						if (!PC->CurrentTileOn) return;
						S->NotifyMoveCompleted(
							MoveStartAxial,
							MoveEndAxial,
							ActiveMoveAxialPath,
							MoveStartWorld,
							MoveEndWorld,
							Picked
						);
					}
					else
					{
						bSuppressMoveRecording = false;
					}
				}
			}

			bMoveInProgress = false;
			ActiveMoveAxialPath.Reset();
			ActiveMovePickedCollectibles.Reset();

			// Snap to exact tile center and kill momentum so the character
			// doesn't slide past due to CMC deceleration.
			const FVector DestCenter = CurrentPathWorld[ReachedIndex];
			const FVector SnappedLoc = FVector(DestCenter.X, DestCenter.Y, MycelandCharacter->GetActorLocation().Z);
			MycelandCharacter->SetActorLocation(SnappedLoc, false, nullptr, ETeleportType::TeleportPhysics);
			if (UCharacterMovementComponent* MC = MycelandCharacter->GetCharacterMovement())
				MC->StopMovementImmediately();

			CurrentPathWorld.Reset();
			CurrentPathIndex = 0;
			SetIsMoving(false);
			OnPathFinished();

			return;
		}
		return;
	}
	
	MycelandCharacter->AddMovementInput(To / Dist, MoveSpeedScale);
}

void AML_PlayerController::OnPathFinished()
{
	// Arrived at the border tile → switch to free movement
	if (bPendingFreeMovementOnArrival)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EXIT] OnPathFinished: bPendingFreeMovementOnArrival=true"));
		UE_LOG(LogTemp, Warning, TEXT("[EXIT] bHasExitTargetWorld=%d, PendingExitTargetWorld=%s"), 
			bHasExitTargetWorld, *PendingExitTargetWorld.ToString());
		
		bPendingFreeMovementOnArrival = false;
		SetMovementMode(EML_PlayerMovementMode::FreeMovement);

		if (bHasExitTargetWorld)
		{
			UE_LOG(LogTemp, Warning, TEXT("[EXIT] Calling StartNavMeshMovement to %s"), *PendingExitTargetWorld.ToString());
			StartNavMeshMovement(PendingExitTargetWorld);
			bHasExitTargetWorld = false;
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[EXIT] ERROR: bHasExitTargetWorld is FALSE!"));
		}
		
		// Clean up exit state now that we're done with it
		PendingExitTile = nullptr;

		return;
	}

	// Arrived at the board entry tile → switch to board movement and immediately path to stored target
	if (bPendingBoardEntryOnArrival)
	{
		bPendingBoardEntryOnArrival = false;
		SetMovementMode(EML_PlayerMovementMode::InsideBoard);

		if (!IsValid(MycelandCharacter) || !IsValid(MycelandCharacter->CurrentTileOn)) return;

		AML_BoardSpawner* Board = MycelandCharacter->CurrentTileOn->GetBoardSpawnerFromTile();
		if (!IsValid(Board)) return;

		AML_Tile* TargetTile = PendingBoardEntryTargetTile;
		PendingBoardEntryTargetTile = nullptr;

		if (!IsValid(TargetTile) || TargetTile->GetOwner() != Board) return;

		const TMap<FIntPoint, AML_Tile*> GridMap = Board->GetGridMap();
		const FIntPoint StartAxial = MycelandCharacter->CurrentTileOn->GetAxialCoord();
		const FIntPoint GoalAxial = TargetTile->GetAxialCoord();

		if (!GridMap.Contains(StartAxial) || !GridMap.Contains(GoalAxial)) return;
		if (!IsTileWalkable(GridMap[StartAxial]) || !IsTileWalkable(GridMap[GoalAxial])) return;

		TArray<FIntPoint> AxialPath;
		if (!BuildPath_AxialBFS(StartAxial, GoalAxial, GridMap, AxialPath)) return;

		StartMoveAlongPath(AxialPath, GridMap);
	}

	if (bPendingPlantOnArrival)
	{
		bPendingPlantOnArrival = false;

		if (!IsValid(PendingPlantTargetTile) || !IsValid(MycelandCharacter) || !IsValid(
			MycelandCharacter->CurrentTileOn))
		{
			PendingPlantTargetTile = nullptr;
			return;
		}

		AML_BoardSpawner* Board = MycelandCharacter->CurrentTileOn->GetBoardSpawnerFromTile();
		if (!IsValid(Board))
		{
			PendingPlantTargetTile = nullptr;
			return;
		}

		// Verify the target is still a neighbor and is Dirt
		TArray<AML_Tile*> Neighbors = Board->GetNeighbors(MycelandCharacter->CurrentTileOn);
		if (Neighbors.Contains(PendingPlantTargetTile) &&
			PendingPlantTargetTile->GetCurrentType() == EML_TileType::Dirt &&
			CurrentEnergy > 0)
		{
			bTurningToTile = true;
			StartTurnTowardTileTimer();
			return; // planting will happen after turn completes
		}

		PendingPlantTargetTile = nullptr;
	}
}


// ==================== Board Exit / Entry ====================

void AML_PlayerController::StartExitHoldTimer()
{
	ExitHoldTimer = 0.f;
	bWasExitingLastFrame = false;
	LastBroadcastProgress = -1.f;
	
	// Start timer that updates at 60Hz for smooth progress
	GetWorld()->GetTimerManager().SetTimer(
		ExitHoldTimerHandle,
		this,
		&AML_PlayerController::TickExitHold,
		1.f / 60.f,
		true
	);
}

void AML_PlayerController::StopExitHoldTimer()
{
	GetWorld()->GetTimerManager().ClearTimer(ExitHoldTimerHandle);
	
	// Clean up state
	ExitHoldTimer = 0.f;
	bWasExitingLastFrame = false;
	LastBroadcastProgress = -1.f;
	
	// Broadcast cancellation if needed
	if (CurrentMovementMode == EML_PlayerMovementMode::ExitingBoard)
	{
		OnExitCursorHold.Broadcast(false, 0.0f);
	}
}

void AML_PlayerController::TickExitHold()
{
	const bool bIsCurrentlyExiting = (CurrentMovementMode == EML_PlayerMovementMode::ExitingBoard && bIsHoldingExitInput);
    
	// Not exiting anymore - cancellation
	if (!bIsCurrentlyExiting)
	{
		// Only clean up if we were actually in ExitingBoard mode
		if (CurrentMovementMode == EML_PlayerMovementMode::ExitingBoard)
		{
			// Stop the timer and reset state
			GetWorld()->GetTimerManager().ClearTimer(ExitHoldTimerHandle);
			ExitHoldTimer = 0.f;
			
			// Broadcast cancellation
			OnExitCursorHold.Broadcast(false, 0.0f);
			bWasExitingLastFrame = false;
			LastBroadcastProgress = -1.f;
			
			// Return to InsideBoard mode (this will clean up exit state via SetMovementMode)
			SetMovementMode(EML_PlayerMovementMode::InsideBoard);
		}
		return;
	}
    
	// Calculate progress
	const float Progress = FMath::Clamp(ExitHoldTimer / DevSettings->ExitBoardHoldDuration, 0.f, 1.f);
    
	// Only broadcast if:
	// 1. We just started exiting (transition)
	// 2. The progress has changed significantly (avoids micro-variations)
	const bool bJustStartedExiting = !bWasExitingLastFrame;
	const bool bProgressChanged = FMath::Abs(Progress - LastBroadcastProgress) > 0.01f; // 1% difference
    
	if (bJustStartedExiting || bProgressChanged)
	{
		OnExitCursorHold.Broadcast(true, Progress);
		LastBroadcastProgress = Progress;
	}
    
	bWasExitingLastFrame = true;

	ExitHoldTimer += 1.f / 60.f; // Fixed timestep
	if (ExitHoldTimer >= DevSettings->ExitBoardHoldDuration)
	{
		// Stop timer and clean state
		GetWorld()->GetTimerManager().ClearTimer(ExitHoldTimerHandle);
		ExitHoldTimer = 0.f;
		bWasExitingLastFrame = false;
		LastBroadcastProgress = -1.f;
		
		// Broadcast completion
		OnExitCursorHold.Broadcast(false, 1.0f);
		
		// Execute the exit
		ConfirmExitBoard();
	}
}

void AML_PlayerController::ConfirmExitBoard()
{
	UE_LOG(LogTemp, Warning, TEXT("[EXIT] ConfirmExitBoard called"));
	
	if (!IsValid(PendingExitTile) || !IsValid(MycelandCharacter) || !IsValid(MycelandCharacter->CurrentTileOn)) return;

	AML_BoardSpawner* Board = MycelandCharacter->CurrentTileOn->GetBoardSpawnerFromTile();
	if (!IsValid(Board)) return;

	const TMap<FIntPoint, AML_Tile*> GridMap = Board->GetGridMap();
	const FIntPoint StartAxial = MycelandCharacter->CurrentTileOn->GetAxialCoord();
	const FIntPoint GoalAxial = PendingExitTile->GetAxialCoord();
	
	UE_LOG(LogTemp, Warning, TEXT("[EXIT] StartAxial=%s, GoalAxial=%s"), *StartAxial.ToString(), *GoalAxial.ToString());
	
	if (StartAxial == GoalAxial)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EXIT] Direct exit (already on border tile)"));
		// Direct exit, no path needed
		SetMovementMode(EML_PlayerMovementMode::FreeMovement);

		if (bHasExitTargetWorld)
		{
			StartNavMeshMovement(PendingExitTargetWorld);
			bHasExitTargetWorld = false;
		}

		PendingExitTile = nullptr;
		return;
	}

	if (!GridMap.Contains(StartAxial) || !GridMap.Contains(GoalAxial)) return;

	TArray<FIntPoint> AxialPath;
	if (!BuildPath_AxialBFS(StartAxial, GoalAxial, GridMap, AxialPath)) return;

	UE_LOG(LogTemp, Warning, TEXT("[EXIT] Need to path to border tile. Path length=%d"), AxialPath.Num());
	UE_LOG(LogTemp, Warning, TEXT("[EXIT] Setting bPendingFreeMovementOnArrival=true, bHasExitTargetWorld=%d"), bHasExitTargetWorld);
	
	bPendingFreeMovementOnArrival = true;
	// Still in board mode during this walk; FreeMovement triggers on arrival
	SetMovementMode(EML_PlayerMovementMode::InsideBoard);
	// Don't clear PendingExitTile yet - OnPathFinished needs bHasExitTargetWorld
	// PendingExitTile will be cleared in OnPathFinished

	StartMoveAlongPath(AxialPath, GridMap);
}


// ==================== Delegates ====================

void AML_PlayerController::HandleBoardStateChanged(const AML_Tile* NewTile)
{
	// ---------- Energy ----------
	if (NewTile)
		InitNumberOfEnergyForLevel(NewTile->GetBoardSpawnerFromTile()->GetEnergyForPuzzle());
	else
		InitNumberOfEnergyForLevel(0);

	// ---------- Automatic transition: Free ↔ InsideBoard ----------
	// This is the ONLY place where we auto-switch based on tile presence
	const bool bShouldBeInBoard = IsValid(NewTile);
	const bool bCurrentlyInFreeMovement = (CurrentMovementMode == EML_PlayerMovementMode::FreeMovement);
	
	if (bShouldBeInBoard && bCurrentlyInFreeMovement)
	{
		// Entering a board from free movement
		SetMovementMode(EML_PlayerMovementMode::InsideBoard);

		// Clear any previous path
		CurrentPathWorld.Reset();
		CurrentPathIndex = 0;

		if (!IsValid(MycelandCharacter))
			return;

		// Smoothly move to tile center
		FVector TileCenter = NewTile->GetActorLocation();
		TileCenter.Z = MycelandCharacter->GetActorLocation().Z;

		CurrentPathWorld.Add(TileCenter);
		CurrentPathIndex = 0;
		SetIsMoving(true);
	}
	else if (!bShouldBeInBoard && !bCurrentlyInFreeMovement)
	{
		// Left a board - switch to free movement
		SetMovementMode(EML_PlayerMovementMode::FreeMovement);
	}
}


// ==================== Actions ====================

void AML_PlayerController::ConfirmTurn(AML_Tile* HitTile)
{
	AddEnergy(-1);

	if (UML_WavePropagationSubsystem* WavePropagationSubsystem = GetWorld()->GetSubsystem<
		UML_WavePropagationSubsystem>())
	{
		OnGrassPlanted.Broadcast(HitTile);
		WavePropagationSubsystem->BeginTileResolved(HitTile);
	}
}


// ==================== Lifecycle ====================

void AML_PlayerController::BeginPlay()
{
	Super::BeginPlay();
	GetWorld()->GetSubsystem<UML_WavePropagationSubsystem>()->EnsureInitialized();
	DevSettings = UML_MycelandDeveloperSettings::GetMycelandDeveloperSettings();
}

void AML_PlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	
	// Only tick if moving
	if (bIsMoving)
		TickMoveAlongPath(DeltaTime);
}

void AML_PlayerController::OnPossess(APawn* aPawn)
{
	Super::OnPossess(aPawn);
	MycelandCharacter = Cast<AML_PlayerCharacter>(aPawn);
	if (MycelandCharacter)
	{
		MycelandCharacter->OnBoardChanged.AddDynamic(this, &AML_PlayerController::HandleBoardStateChanged);
		
		if (MycelandCharacter->CurrentTileOn)
		{
			CurrentMovementMode = EML_PlayerMovementMode::InsideBoard;
		}
		else
		{
			CurrentMovementMode = EML_PlayerMovementMode::FreeMovement;
		}
		
		// Always start hover timer for cursor glow
		StartHoverPreviewTimer();
	}
}


// ==================== Input ====================

// Bound to OnStarted — fires once per click
// Handles: board BFS movement, exit hold trigger, board re-entry
void AML_PlayerController::OnSetDestinationStarted()
{
	// --- INSIDE BOARD ---
	if (CurrentMovementMode == EML_PlayerMovementMode::InsideBoard)
	{
		if (!IsValid(MycelandCharacter) || !IsValid(MycelandCharacter->CurrentTileOn)) return;

		AML_BoardSpawner* Board = MycelandCharacter->CurrentTileOn->GetBoardSpawnerFromTile();
		if (!IsValid(Board)) return;

		const TMap<FIntPoint, AML_Tile*> GridMap = Board->GetGridMap();
		AML_Tile* TargetTile = GetTileUnderCursor();

		// Click inside the board → BFS
		if (IsValid(TargetTile) && TargetTile->GetOwner() == Board)
		{
			const FIntPoint StartAxial = MycelandCharacter->CurrentTileOn->GetAxialCoord();
			const FIntPoint GoalAxial = TargetTile->GetAxialCoord();

			if (!GridMap.Contains(StartAxial) || !GridMap.Contains(GoalAxial)) return;
			if (!IsTileWalkable(GridMap[StartAxial]) || !IsTileWalkable(GridMap[GoalAxial])) return;

			TArray<FIntPoint> AxialPath;
			if (!BuildPath_AxialBFS(StartAxial, GoalAxial, GridMap, AxialPath)) return;

			// --- Arm move recording (NORMAL board move) ---
			bMoveInProgress = true;
			bUndoMovePlayback = false; // safety: this is NOT an undo playback
			bSuppressMoveRecording = false; // safety: allow recording

			MoveStartAxial = StartAxial;
			MoveEndAxial = GoalAxial;

			MoveStartWorld = MycelandCharacter->GetActorLocation();
			MoveEndWorld = TargetTile->GetActorLocation();

			// This is the deterministic path we will record/undo
			ActiveMoveAxialPath = AxialPath;
			ActiveMovePickedCollectibles.Reset();
			// ---------------------------------------------

			StartMoveAlongPath(AxialPath, GridMap);
			return;
		}

		// Click outside the board → start exit hold
		FHitResult Hit;
		if (!GetHitResultUnderCursorByChannel(UEngineTypes::ConvertToTraceType(ECC_Visibility), true, Hit)) return;

		AML_Tile* NearestTile = FindNearestWalkableTile(Hit.Location, GridMap);
		if (!IsValid(NearestTile)) return;

		PendingExitTile = NearestTile;
		PendingExitTargetWorld = Hit.Location;
		bHasExitTargetWorld = true;
		SetMovementMode(EML_PlayerMovementMode::ExitingBoard);
		bIsHoldingExitInput = true;
		StartExitHoldTimer();
		// Keep hover preview timer running in ExitingBoard mode
		return;
	}

	// // --- FREE MOVEMENT — click on a board tile → re-enter ---
	// if (CurrentMovementMode == EML_PlayerMovementMode::FreeMovement)
	// {
	// 	AML_Tile* TargetTile = GetTileUnderCursor();
	// 	if (IsValid(TargetTile))
	// 	{
	// 		AML_BoardSpawner* Board = TargetTile->GetBoardSpawnerFromTile();
	// 		if (!IsValid(Board)) return;
	//
	// 		// Find the nearest border/entry tile
	// 		const TMap<FIntPoint, AML_Tile*> GridMap = Board->GetGridMap();
	// 		AML_Tile* NearestBorderTile = FindNearestWalkableTile(MycelandCharacter->GetActorLocation(), GridMap);
	//
	// 		// Setup board entry state
	// 		PendingBoardEntryTargetTile = TargetTile;
	// 		bPendingBoardEntryOnArrival = true;
	// 		SetMovementMode(EML_PlayerMovementMode::EnteringBoard);
	//
	// 		if (!IsValid(NearestBorderTile))
	// 		{
	// 			// Fallback: go directly to target if no border tile found
	// 			StartNavMeshMovement(TargetTile->GetActorLocation());
	// 		}
	// 		else
	// 		{
	// 			// Move to the border tile first (not directly to target)
	// 			StartNavMeshMovement(NearestBorderTile->GetActorLocation());
	// 		}
	// 		return;
	// 	}
	//
	// 	// Click outside board → move to that position using nav mesh
	// 	FHitResult Hit;
	// 	if (!GetHitResultUnderCursorByChannel(UEngineTypes::ConvertToTraceType(ECC_Visibility), true, Hit))
	// 		return;
	//
	// 	// Start nav mesh movement to clicked location
	// 	StartNavMeshMovement(Hit.Location);
	//
	// 	return;
	// }
	
	// --- FREE MOVEMENT — click on a board tile → re-enter ---
	if (CurrentMovementMode == EML_PlayerMovementMode::FreeMovement)
	{
		AML_Tile* TargetTile = GetTileUnderCursor();
		if (IsValid(TargetTile))
		{
			AML_BoardSpawner* Board = TargetTile->GetBoardSpawnerFromTile();
			if (!IsValid(Board)) return;

			// Use the board's EntryTile instead of nearest walkable tile
			AML_Tile* EntryTile = Board->EntryTile;
			if (!IsValid(EntryTile)) return;

			// Setup board entry state
			PendingBoardEntryTargetTile = TargetTile;
			bPendingBoardEntryOnArrival = true;
			SetMovementMode(EML_PlayerMovementMode::EnteringBoard);

			StartNavMeshMovement(EntryTile->GetActorLocation());
			return;
		}

		// Click outside board → move to that position using nav mesh
		FHitResult Hit;
		if (!GetHitResultUnderCursorByChannel(UEngineTypes::ConvertToTraceType(ECC_Visibility), true, Hit))
			return;

		StartNavMeshMovement(Hit.Location);
		return;
	}
}

// Bound to OnCompleted / OnCanceled
void AML_PlayerController::OnSetDestinationReleased()
{
	bIsHoldingExitInput = false;
	// TickExitHold will handle the cleanup on next tick
}

void AML_PlayerController::OnMoveAndPlantStarted()
{
	// Only works in board mode
	if (CurrentMovementMode != EML_PlayerMovementMode::InsideBoard) return;
	if (!IsValid(MycelandCharacter) || !IsValid(MycelandCharacter->CurrentTileOn)) return;
	if (CurrentEnergy <= 0) return; // Need energy to plant

	AML_BoardSpawner* Board = MycelandCharacter->CurrentTileOn->GetBoardSpawnerFromTile();
	if (!IsValid(Board)) return;

	const TMap<FIntPoint, AML_Tile*> GridMap = Board->GetGridMap();
	AML_Tile* TargetTile = GetTileUnderCursor();

	// Must click on a valid tile in the same board
	if (!IsValid(TargetTile) || TargetTile->GetOwner() != Board) return;

	// Target must be Dirt
	if (TargetTile->GetCurrentType() != EML_TileType::Dirt) return;

	const FIntPoint StartAxial = MycelandCharacter->CurrentTileOn->GetAxialCoord();
	const FIntPoint TargetAxial = TargetTile->GetAxialCoord();

	if (!GridMap.Contains(StartAxial) || !GridMap.Contains(TargetAxial)) return;

	// Check if target is already a neighbor (adjacent)
	TArray<AML_Tile*> CurrentNeighbors = Board->GetNeighbors(MycelandCharacter->CurrentTileOn);
	if (CurrentNeighbors.Contains(TargetTile))
	{
		// Already adjacent → just plant immediately (like right-click behavior)
		PendingPlantTargetTile = TargetTile;
		bTurningToTile = true;
		StartTurnTowardTileTimer();
		return;
	}

	// Target is NOT adjacent → need to path there
	// Build full path to target
	TArray<FIntPoint> FullPath;
	if (!BuildPath_AxialBFS(StartAxial, TargetAxial, GridMap, FullPath)) return;

	// Need at least 2 tiles in path (start + at least one step)
	if (FullPath.Num() < 2) return;

	// Remove the last tile (we want to stop BEFORE the target, not ON it)
	FullPath.RemoveAt(FullPath.Num() - 1);

	// Verify the new end position is walkable
	const FIntPoint StopAxial = FullPath.Last();
	if (!GridMap.Contains(StopAxial) || !IsTileWalkable(GridMap[StopAxial])) return;

	// Verify that from the stop position, target is a neighbor
	AML_Tile* StopTile = GridMap[StopAxial];
	TArray<AML_Tile*> StopNeighbors = Board->GetNeighbors(StopTile);
	if (!StopNeighbors.Contains(TargetTile)) return;

	// All checks passed → move and plant!
	PendingPlantTargetTile = TargetTile;

	bPendingPlantOnArrival = true;

	StartMoveAlongPath(FullPath, GridMap);
}


// ==================== Hover Preview ====================

void AML_PlayerController::StartHoverPreviewTimer()
{
	if (!HoverPreviewTimerHandle.IsValid())
	{
		GetWorld()->GetTimerManager().SetTimer(
			HoverPreviewTimerHandle,
			this,
			&AML_PlayerController::UpdateHoverPreview,
			1.f / 30.f, // 30Hz is enough for hover preview
			true
		);
	}
}

void AML_PlayerController::StopHoverPreviewTimer()
{
	GetWorld()->GetTimerManager().ClearTimer(HoverPreviewTimerHandle);
	ClearHoverPreview();
	ClearCursorHoverPreview();
}

void AML_PlayerController::UpdateHoverPreview()
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

void AML_PlayerController::TickCursorHoverPreview()
{
	// Get tile under cursor
	AML_Tile* CursorHoveredTile = GetTileUnderCursor();

	if (CursorHoveredTile == LastCursorHoveredTile)
		return;

	if (!IsValid(CursorHoveredTile))
	{
		ClearCursorHoverPreview();
		return;
	}

	if (IsValid(LastCursorHoveredTile))
		LastCursorHoveredTile->StopGlowingCursorUnhovered();
	
	const bool bIsOnPlayerTile = IsValid(MycelandCharacter) &&
								 IsValid(MycelandCharacter->CurrentTileOn) &&
								 CursorHoveredTile == MycelandCharacter->CurrentTileOn;

	// Suppress cursor glow when the tile is unreachable in board mode.
	// TickHoverPreview already ran this frame and set bCurrentHoveredTileReachable.
	// Also suppress if hovering the player's current tile.
	const bool bIsInBoardMode = (CurrentMovementMode == EML_PlayerMovementMode::InsideBoard || 
	                             CurrentMovementMode == EML_PlayerMovementMode::ExitingBoard);
	
	if (!bIsOnPlayerTile && (!bIsInBoardMode || bCurrentHoveredTileReachable))
		CursorHoveredTile->GlowCursorHovered();

	LastCursorHoveredTile = CursorHoveredTile;
}

void AML_PlayerController::ClearCursorHoverPreview()
{
	if (IsValid(LastCursorHoveredTile))
	{
		LastCursorHoveredTile->StopGlowingCursorUnhovered();
		LastCursorHoveredTile = nullptr;
	}
}

void AML_PlayerController::TickHoverPreview()
{
	// Only preview in board mode (InsideBoard or ExitingBoard)
	if (CurrentMovementMode != EML_PlayerMovementMode::InsideBoard && 
	    CurrentMovementMode != EML_PlayerMovementMode::ExitingBoard)
	{
		ClearHoverPreview();
		return;
	}

	if (!IsValid(MycelandCharacter) || !IsValid(MycelandCharacter->CurrentTileOn))
	{
		ClearHoverPreview();
		return;
	}

	AML_Tile* HoveredTile = GetTileUnderCursor();

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

	AML_BoardSpawner* Board = MycelandCharacter->CurrentTileOn->GetBoardSpawnerFromTile();
	if (!IsValid(Board) || HoveredTile->GetOwner() != Board || !IsTileWalkable(HoveredTile))
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
	
	const bool bIsOnPlayerTile = IsValid(MycelandCharacter) &&
							 IsValid(MycelandCharacter->CurrentTileOn) &&
							 HoveredTile == MycelandCharacter->CurrentTileOn;

	if (bReachable)
	{
		for (AML_Tile* Tile : NewPath)
			if (!bIsOnPlayerTile)
				Tile->GlowPathWalk();
		CurrentPreviewPath = NewPath;
	}
}

void AML_PlayerController::ClearHoverPreview()
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

TArray<AML_Tile*> AML_PlayerController::BuildPreviewPath(const AML_Tile* TargetTile) const
{
	TArray<AML_Tile*> Result;

	if (!IsValid(TargetTile) || !IsValid(MycelandCharacter) || !IsValid(MycelandCharacter->CurrentTileOn))
		return Result;

	AML_BoardSpawner* Board = MycelandCharacter->CurrentTileOn->GetBoardSpawnerFromTile();
	if (!IsValid(Board))
		return Result;

	const TMap<FIntPoint, AML_Tile*> GridMap = Board->GetGridMap();
	const FIntPoint StartAxial = MycelandCharacter->CurrentTileOn->GetAxialCoord();
	const FIntPoint GoalAxial = TargetTile->GetAxialCoord();

	if (!GridMap.Contains(StartAxial) || !GridMap.Contains(GoalAxial))
		return Result;

	if (!IsTileWalkable(GridMap[StartAxial]) || !IsTileWalkable(GridMap[GoalAxial]))
		return Result;

	// Build axial path using BFS
	TArray<FIntPoint> AxialPath;
	if (!BuildPath_AxialBFS(StartAxial, GoalAxial, GridMap, AxialPath))
		return Result;

	// Convert axial path to tile array
	Result.Reserve(AxialPath.Num());
	for (const FIntPoint& Axial : AxialPath)
	{
		if (AML_Tile* const* TilePtr = GridMap.Find(Axial))
		{
			if (IsValid(*TilePtr))
			{
				Result.Add(*TilePtr);
			}
		}
	}

	return Result;
}


// ==================== Energy ====================

void AML_PlayerController::SetCurrentEnergy(int32 NewEnergy)
{
	NewEnergy = FMath::Clamp(NewEnergy, 0, INT32_MAX);

	if (CurrentEnergy == NewEnergy)
		return;

	CurrentEnergy = NewEnergy;
	OnEnergyChanged.Broadcast(CurrentEnergy);
}

void AML_PlayerController::AddEnergy(int32 Delta)
{
	SetCurrentEnergy(CurrentEnergy + Delta);
}

void AML_PlayerController::InitNumberOfEnergyForLevel(const int32 Energy)
{
	SetCurrentEnergy(Energy);
}


// ==================== Actions ====================

bool AML_PlayerController::MovePlayerToAxial(const FIntPoint& TargetAxial, bool bUsePath, bool bFallbackTeleport,
                                             const FVector& TeleportFallbackWorld)
{
	if (!IsValid(MycelandCharacter) || !IsValid(MycelandCharacter->CurrentTileOn)) return false;

	AML_BoardSpawner* Board = MycelandCharacter->CurrentTileOn->GetBoardSpawnerFromTile();
	if (!IsValid(Board)) return false;

	const TMap<FIntPoint, AML_Tile*> GridMap = Board->GetGridMap();
	const FIntPoint StartAxial = MycelandCharacter->CurrentTileOn->GetAxialCoord();

	if (!GridMap.Contains(StartAxial) || !GridMap.Contains(TargetAxial))
	{
		if (bFallbackTeleport)
			MycelandCharacter->SetActorLocation(TeleportFallbackWorld);
		return false;
	}

	if (!bUsePath)
	{
		if (AML_Tile* const* TilePtr = GridMap.Find(TargetAxial))
		{
			if (IsValid(*TilePtr))
			{
				MycelandCharacter->SetActorLocation((*TilePtr)->GetActorLocation());
				return true;
			}
		}
		return false;
	}

	TArray<FIntPoint> AxialPath;
	if (!BuildPath_AxialBFS(StartAxial, TargetAxial, GridMap, AxialPath))
	{
		if (bFallbackTeleport)
			MycelandCharacter->SetActorLocation(TeleportFallbackWorld);
		return false;
	}

	bMoveInProgress = true;
	MoveStartAxial = StartAxial;
	MoveEndAxial = TargetAxial;

	if (APawn* P = GetPawn())
		MoveStartWorld = P->GetActorLocation();

	if (AML_Tile* const* TilePtr = GridMap.Find(TargetAxial))
		MoveEndWorld = IsValid(*TilePtr) ? (*TilePtr)->GetActorLocation() : TeleportFallbackWorld;
	else
		MoveEndWorld = TeleportFallbackWorld;

	ActiveMoveAxialPath = AxialPath;
	ActiveMovePickedCollectibles.Reset();

	StartMoveAlongPath(AxialPath, GridMap);
	return true;
}

void AML_PlayerController::StartMoveAlongAxialPathForUndo(const TArray<FIntPoint>& AxialPath,
                                                          const TArray<FIntPoint>& PickedCollectibleAxials)
{
	if (!IsValid(MycelandCharacter) || !IsValid(MycelandCharacter->CurrentTileOn)) return;

	AML_BoardSpawner* Board = MycelandCharacter->CurrentTileOn->GetBoardSpawnerFromTile();
	if (!IsValid(Board)) return;

	const TMap<FIntPoint, AML_Tile*> GridMap = Board->GetGridMap();

	bUndoMovePlayback = true;
	bSuppressMoveRecording = true;

	// Setup collectible restore set for this undo playback.
	bUndoRestoreCollectibles = (PickedCollectibleAxials.Num() > 0);
	UndoMoveRemainingCollectibles.Reset();
	for (const FIntPoint& Ax : PickedCollectibleAxials)
	{
		UndoMoveRemainingCollectibles.Add(Ax);
	}

	// Ensure end-of-path logic triggers.
	bMoveInProgress = true;
	ActiveMoveAxialPath = AxialPath;
	ActiveMovePickedCollectibles.Reset();

	StartMoveAlongPath(AxialPath, GridMap);
}

void AML_PlayerController::NotifyCollectiblePickedOnAxial(const FIntPoint& Axial)
{
	if (!bMoveInProgress) return;
	ActiveMovePickedCollectibles.Add(Axial);
}

void AML_PlayerController::RotateCharacterTowardTile(const AML_Tile* HitTileActor, float DeltaTime, float TurnSpeed)
{
	AML_PlayerCharacter* PlayerCharacter = Cast<AML_PlayerCharacter>(GetCharacter());
	if (!IsValid(PlayerCharacter) || !IsValid(HitTileActor)) return;

	FVector Dir = HitTileActor->GetActorLocation() - PlayerCharacter->GetActorLocation();
	Dir.Z = 0.f;

	if (Dir.IsNearlyZero())
	{
		bTurningToTile = false;
		return;
	}

	const float DesiredYaw = Dir.Rotation().Yaw;
	const FRotator CurrentRot = PlayerCharacter->GetActorRotation();
	const FRotator DesiredRot(0.f, DesiredYaw, 0.f);

	const FRotator NewRot = FMath::RInterpTo(CurrentRot, DesiredRot, DeltaTime, TurnSpeed);
	PlayerCharacter->SetActorRotation(NewRot);

	const float YawError = FMath::Abs(FMath::FindDeltaAngleDegrees(NewRot.Yaw, DesiredYaw));
	bTurningToTile = (YawError > 1.0f);
}

void AML_PlayerController::StartTurnTowardTileTimer()
{
	if (!TurnTowardTileTimerHandle.IsValid())
	{
		GetWorld()->GetTimerManager().SetTimer(
			TurnTowardTileTimerHandle,
			this,
			&AML_PlayerController::UpdateTurnTowardPendingTile,
			1.f / 60.f,
			true
		);
	}
}

void AML_PlayerController::StopTurnTowardTileTimer()
{
	GetWorld()->GetTimerManager().ClearTimer(TurnTowardTileTimerHandle);
}

void AML_PlayerController::UpdateTurnTowardPendingTile()
{
	if (!bTurningToTile || !IsValid(PendingPlantTargetTile) || !IsValid(MycelandCharacter))
	{
		StopTurnTowardTileTimer();
		return;
	}

	RotateCharacterTowardTile(PendingPlantTargetTile, 1.f / 60.f, RotateSpeed);

	if (!bTurningToTile)
	{
		StopTurnTowardTileTimer();
		
		if (CurrentEnergy > 0)
		{
			ConfirmTurn(PendingPlantTargetTile);
		}
		PendingPlantTargetTile = nullptr;
	}
}