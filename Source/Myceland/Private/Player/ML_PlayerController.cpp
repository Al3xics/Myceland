// Copyright Myceland Team, All Rights Reserved.

#include "Player/ML_PlayerController.h"

#include "Component/ML_EnergyComponent.h"
#include "Component/ML_HoverPreviewComponent.h"
#include "Component/ML_MoveRecordingComponent.h"
#include "Core/ML_TileTypeTraits.h"
#include "Player/ML_HexPathfinder.h"
#include "Component/ML_BoardTransitionComponent.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "Developer Settings/ML_MycelandDeveloperSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/ML_PlayerCharacter.h"
#include "Subsystem/ML_RollBackSubsystem.h"
#include "Subsystem/ML_WavePropagationSubsystem.h"
#include "Tiles/ML_Tile.h"

class UML_WavePropagationSubsystem;

// ==================== Constructor ====================

AML_PlayerController::AML_PlayerController()
{
	EnergyComponent = CreateDefaultSubobject<UML_EnergyComponent>(TEXT("EnergyComponent"));
	HoverPreviewComponent = CreateDefaultSubobject<UML_HoverPreviewComponent>(TEXT("HoverPreviewComponent"));
	MoveRecordingComponent = CreateDefaultSubobject<UML_MoveRecordingComponent>(TEXT("MoveRecordingComponent"));
	TransitionComponent = CreateDefaultSubobject<UML_BoardTransitionComponent>(TEXT("TransitionComponent"));
}

// ==================== Helpers ====================

AML_Tile* AML_PlayerController::GetTileUnderCursor() const
{
	FHitResult Hit;
	if (!GetHitResultUnderCursorByChannel(UEngineTypes::ConvertToTraceType(ECC_Visibility), true, Hit))
		return nullptr;
	
	if (!IsClickableGround(Hit))
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


void AML_PlayerController::SetIsMoving(bool bNewIsMoving)
{
	bIsMoving = bNewIsMoving;
	TransitionComponent->NotifyIsMoving(bNewIsMoving);
}

void AML_PlayerController::SetMovementMode(EML_PlayerMovementMode NewMode)
{
	if (TransitionComponent->GetMovementMode() == NewMode)
		return;

	const EML_PlayerMovementMode OldMode = TransitionComponent->GetMovementMode();

	// Let the component update its state and handle pending-state cleanup
	TransitionComponent->SwitchToMode(NewMode);

	// Notify hover system
	HoverPreviewComponent->NotifyMovementModeChanged(NewMode);

	// Controller-side reaction: stop NavMesh when entering the board
	if (OldMode == EML_PlayerMovementMode::FreeMovement)
		StopNavMeshMovement();
}

bool AML_PlayerController::IsClickableGround(const FHitResult& Hit) const
{
	if (!Hit.bBlockingHit || !Hit.Component.IsValid())
		return false;
    // GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Red, FString::Printf(TEXT("IsClickableGround: %s"), *Hit.Component->GetName()));
	ECollisionChannel ObjectType = Hit.Component->GetCollisionObjectType();
	return ObjectType == ECC_GameTraceChannel1;
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

/**
 * Redirects the active world-space path to follow a new AxialPath without resetting
 * CurrentPathIndex to zero. Used when the player clicks a new destination mid-move.
 *
 * The new AxialPath is the fully merged path produced by ExtendMoveRecord, so it
 * already contains the walked prefix. We just rebuild CurrentPathWorld from it,
 * then set CurrentPathIndex to the first waypoint that is AFTER the player's current
 * position — preserving the in-progress movement seamlessly.
 */
void AML_PlayerController::ExtendMoveAlongPath(const TArray<FIntPoint>& FullMergedAxialPath,
                                               const TMap<FIntPoint, AML_Tile*>& GridMap,
                                               int32 PreservedPathIndex)
{
	// Rebuild the full world-space path from the merged axial path.
	TArray<FVector> NewPathWorld;
	NewPathWorld.Reserve(FullMergedAxialPath.Num());

	for (const FIntPoint& Axial : FullMergedAxialPath)
		if (AML_Tile* const* TilePtr = GridMap.Find(Axial))
			if (IsValid(*TilePtr))
				NewPathWorld.Add((*TilePtr)->GetActorLocation());

	if (NewPathWorld.Num() == 0)
		return; // Nothing valid — keep the current path as-is.

	// The merged axial path preserves the current logical target at the same index.
	// Keep that exact waypoint instead of trying to rediscover it from world positions.
	const FVector PlayerLoc = IsValid(MycelandCharacter) ? MycelandCharacter->GetActorLocation() : FVector::ZeroVector;
	int32 NewIndex = FMath::Clamp(PreservedPathIndex, 0, NewPathWorld.Num() - 1);

	// Advance past any waypoints the player has already reached.
	while (NewIndex < NewPathWorld.Num() &&
	       FVector::DistSquared2D(PlayerLoc, NewPathWorld[NewIndex]) <= FMath::Square(AcceptanceRadius))
	{
		NewIndex++;
	}

	if (NewIndex >= NewPathWorld.Num())
	{
		// Player is already at or past the new destination — treat as finished.
		CurrentPathWorld = MoveTemp(NewPathWorld);
		CurrentPathIndex = CurrentPathWorld.Num();
		return;
	}

	CurrentPathWorld  = MoveTemp(NewPathWorld);
	CurrentPathIndex  = NewIndex;
	// bIsMoving is already true; no need to call SetIsMoving again.
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
		if (TransitionComponent->IsPendingBoardEntry())
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
	if (TransitionComponent->GetMovementMode() == EML_PlayerMovementMode::FreeMovement ||
	    TransitionComponent->GetMovementMode() == EML_PlayerMovementMode::EnteringBoard)
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
		{
			UML_RollBackSubsystem* RollBackSubsystem = GetWorld()->GetSubsystem<UML_RollBackSubsystem>();
			MoveRecordingComponent->TickUndoRestore(ReachedIndex, RollBackSubsystem);
		}

		if (CurrentPathIndex >= CurrentPathWorld.Num())
		{
			UML_RollBackSubsystem* RollBackSubsystem = GetWorld()->GetSubsystem<UML_RollBackSubsystem>();
			if (!MoveRecordingComponent->CommitMoveRecord(MycelandCharacter, RollBackSubsystem))
				return;

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
	FBoardTransitionCommand Cmd = TransitionComponent->HandlePathFinished(MycelandCharacter);

	if (Cmd.bStartNavMesh)
		StartNavMeshMovement(Cmd.NavMeshTarget);
	else if (Cmd.bStartBoardPath)
		StartMoveAlongPath(Cmd.BoardPath, Cmd.BoardGridMap);
}


bool AML_PlayerController::StartRecordedBoardMove(const TArray<FIntPoint>& AxialPath, const TMap<FIntPoint, AML_Tile*>& GridMap,
	EML_PlayerBoardActionState ActionState, AML_Tile* PlantTarget)
{
	if (!IsValid(MycelandCharacter) || !IsValid(MycelandCharacter->CurrentTileOn)) return false;

	const bool bMoveAlreadyInProgress =
		MoveRecordingComponent->IsMoveInProgress() &&
		!MoveRecordingComponent->IsUndoMovePlayback();

	if ((!bMoveAlreadyInProgress && AxialPath.Num() < 2) || (bMoveAlreadyInProgress && AxialPath.Num() < 1))
		return false;

	const FIntPoint GoalAxial = AxialPath.Last();

	AML_Tile* const* TargetTilePtr = GridMap.Find(GoalAxial);
	if (!TargetTilePtr || !IsValid(*TargetTilePtr)) return false;

	if (bMoveAlreadyInProgress)
	{
		// Redirect from the waypoint currently being aimed at in the recorded path,
		// not from CurrentTileOn. That keeps the merged path valid even after many
		// consecutive redirects before the logical tile ownership updates.
		const TArray<FIntPoint>& RecordedPath = MoveRecordingComponent->GetActiveMoveAxialPath();
		if (RecordedPath.Num() < 2) return false;

		const int32 JunctionIndex = FMath::Clamp(CurrentPathIndex, 0, RecordedPath.Num() - 1);
		const FIntPoint JunctionAxial = RecordedPath[JunctionIndex];

		if (!GridMap.Contains(JunctionAxial) || !GridMap.Contains(GoalAxial)) return false;

		TArray<FIntPoint> RedirectSubPath;
		if (GoalAxial == JunctionAxial)
		{
			RedirectSubPath.Add(JunctionAxial);
		}
		else
		{
			if (!UML_HexPathfinder::BuildPath_AxialBFS(JunctionAxial, GoalAxial, GridMap, RedirectSubPath)) return false;
			if (RedirectSubPath.Num() < 2) return false;
		}

		const TArray<FIntPoint>& FullMergedPath = MoveRecordingComponent->ExtendMoveRecord(
			GoalAxial,
			(*TargetTilePtr)->GetActorLocation(),
			RedirectSubPath,
			JunctionIndex
		);

		// Rebuild the world path from the merged record and keep the same logical
		// target index, so repeated redirects stay stable.
		ExtendMoveAlongPath(FullMergedPath, GridMap, JunctionIndex);

		// Update the action state (e.g. Moving → MovingToPlant) if needed.
		TransitionComponent->SetBoardActionState(ActionState, PlantTarget);
	}
	else
	{
		// Fresh move — normal begin record.
		const FIntPoint StartAxial = MycelandCharacter->CurrentTileOn->GetAxialCoord();

		MoveRecordingComponent->BeginMoveRecord(
			StartAxial,
			GoalAxial,
			MycelandCharacter->GetActorLocation(),
			(*TargetTilePtr)->GetActorLocation(),
			AxialPath
		);

		TransitionComponent->SetBoardActionState(ActionState, PlantTarget);
		StartMoveAlongPath(AxialPath, GridMap);
	}

	return true;
}

bool AML_PlayerController::Move(AML_Tile* TargetTile, int32 StopBeforeTarget)
{
	if (!IsValid(MycelandCharacter) || !IsValid(MycelandCharacter->CurrentTileOn)) return false;
	if (!IsValid(TargetTile)) return false;

	AML_BoardSpawner* Board = MycelandCharacter->CurrentTileOn->GetBoardSpawnerFromTile();
	if (!IsValid(Board) || TargetTile->GetOwner() != Board) return false;
	if (TransitionComponent->GetBoardActionState() == EML_PlayerBoardActionState::TurningToPlant) return false;

	const TMap<FIntPoint, AML_Tile*> GridMap = Board->GetGridMap();
	const FIntPoint StartAxial = MycelandCharacter->CurrentTileOn->GetAxialCoord();
	const FIntPoint GoalAxial = TargetTile->GetAxialCoord();

	if (!GridMap.Contains(StartAxial) || !GridMap.Contains(GoalAxial)) return false;
	if (!UML_HexPathfinder::IsTileWalkable(GridMap[StartAxial]) ||
		!UML_HexPathfinder::IsTileWalkable(GridMap[GoalAxial])) return false;

	TArray<FIntPoint> AxialPath;
	if (!UML_HexPathfinder::BuildPath_AxialBFS(StartAxial, GoalAxial, GridMap, AxialPath)) return false;

	if (StopBeforeTarget > 0)
	{
		if (AxialPath.Num() <= StopBeforeTarget) return false;
		AxialPath.SetNum(AxialPath.Num() - StopBeforeTarget);
	}

	return StartRecordedBoardMove(AxialPath, GridMap);
}

bool AML_PlayerController::Plant(AML_Tile* TargetTile)
{
	if (TransitionComponent->GetMovementMode() != EML_PlayerMovementMode::InsideBoard) return false;
	if (TransitionComponent->GetBoardActionState() == EML_PlayerBoardActionState::TurningToPlant) return false;
	if (!IsValid(MycelandCharacter) || !IsValid(MycelandCharacter->CurrentTileOn)) return false;
	if (EnergyComponent->GetCurrentEnergy() <= 0) return false;
	if (!IsValid(TargetTile)) return false;

	AML_BoardSpawner* Board = MycelandCharacter->CurrentTileOn->GetBoardSpawnerFromTile();
	if (!IsValid(Board) || TargetTile->GetOwner() != Board) return false;
	if (!UML_TileTypeTraits::CanPlayerPlant(TargetTile->GetCurrentType())) return false;

	const TMap<FIntPoint, AML_Tile*> GridMap = Board->GetGridMap();
	FIntPoint StartAxial = MycelandCharacter->CurrentTileOn->GetAxialCoord();
	if (MoveRecordingComponent->IsMoveInProgress() && !MoveRecordingComponent->IsUndoMovePlayback())
	{
		const TArray<FIntPoint>& RecordedPath = MoveRecordingComponent->GetActiveMoveAxialPath();
		if (RecordedPath.IsValidIndex(CurrentPathIndex))
		{
			StartAxial = RecordedPath[CurrentPathIndex];
		}
	}
	const FIntPoint TargetAxial = TargetTile->GetAxialCoord();

	if (!GridMap.Contains(StartAxial) || !GridMap.Contains(TargetAxial)) return false;

	TArray<AML_Tile*> CurrentNeighbors = Board->GetNeighbors(MycelandCharacter->CurrentTileOn);
	if (!MoveRecordingComponent->IsMoveInProgress() && CurrentNeighbors.Contains(TargetTile))
	{
		TransitionComponent->StartTurnTowardTile(TargetTile);
		return true;
	}

	TArray<FIntPoint> FullPath;
	if (!UML_HexPathfinder::BuildPath_AxialBFS(StartAxial, TargetAxial, GridMap, FullPath)) return false;
	if (FullPath.Num() < 2) return false;

	const FIntPoint StopAxial = FullPath[FullPath.Num() - 2];
	if (!GridMap.Contains(StopAxial) || !UML_HexPathfinder::IsTileWalkable(GridMap[StopAxial])) return false;

	AML_Tile* StopTile = GridMap[StopAxial];
	TArray<AML_Tile*> StopNeighbors = Board->GetNeighbors(StopTile);
	if (!StopNeighbors.Contains(TargetTile)) return false;

	TArray<FIntPoint> MovePath = FullPath;
	MovePath.RemoveAt(MovePath.Num() - 1);
	if (MovePath.Num() == 0)
	{
		MovePath.Add(StartAxial);
	}

	return StartRecordedBoardMove(MovePath, GridMap, EML_PlayerBoardActionState::MovingToPlant, TargetTile);
}

void AML_PlayerController::ExecutePlant(AML_Tile* HitTile)
{
	EnergyComponent->AddEnergy(-1);

	if (UML_WavePropagationSubsystem* WavePropagationSubsystem = GetWorld()->GetSubsystem<UML_WavePropagationSubsystem>())
	{
		OnGrassPlanted.Broadcast(HitTile);
		WavePropagationSubsystem->BeginTileResolved(HitTile);
	}
}


// ==================== Delegates ====================

void AML_PlayerController::HandleCurrentTileChanged(const AML_Tile* OldTile, const AML_Tile* NewTile)
{
	if (HoverPreviewComponent)
		HoverPreviewComponent->NotifyPlayerTileChanged();
}

void AML_PlayerController::HandleBoardStateChanged(const AML_Tile* OldTile, const AML_Tile* NewTile)
{
	// ---------- Energy ----------
	if (NewTile)
		EnergyComponent->InitNumberOfEnergyForLevel(NewTile->GetBoardSpawnerFromTile()->GetEnergyForPuzzle());
	else
		EnergyComponent->InitNumberOfEnergyForLevel(0);

	// ---------- Automatic transition: Free ↔ InsideBoard ----------
	const bool bShouldBeInBoard = IsValid(NewTile);
	const bool bCurrentlyInFreeMovement = (TransitionComponent->GetMovementMode() == EML_PlayerMovementMode::FreeMovement);

	if (bShouldBeInBoard && bCurrentlyInFreeMovement)
	{
		SetMovementMode(EML_PlayerMovementMode::InsideBoard);

		CurrentPathWorld.Reset();
		CurrentPathIndex = 0;

		if (!IsValid(MycelandCharacter))
			return;

		FVector TileCenter = NewTile->GetActorLocation();
		TileCenter.Z = MycelandCharacter->GetActorLocation().Z;

		CurrentPathWorld.Add(TileCenter);
		CurrentPathIndex = 0;
		SetIsMoving(true);
	}
	else if (!bShouldBeInBoard && !bCurrentlyInFreeMovement)
	{
		SetMovementMode(EML_PlayerMovementMode::FreeMovement);
	}
}


// ==================== Actions ====================

void AML_PlayerController::ConfirmTurn(AML_Tile* HitTile)
{
	ExecutePlant(HitTile);
}


// ==================== Lifecycle ====================

void AML_PlayerController::BeginPlay()
{
	Super::BeginPlay();
	GetWorld()->GetSubsystem<UML_WavePropagationSubsystem>()->EnsureInitialized();
	GetWorld()->GetSubsystem<UML_RollBackSubsystem>()->EnsureInitialized();
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

	// BeginPlay sets DevSettings, but OnPossess can fire first (UE startup order is not guaranteed).
	if (!DevSettings)
		DevSettings = UML_MycelandDeveloperSettings::GetMycelandDeveloperSettings();

	MycelandCharacter = Cast<AML_PlayerCharacter>(aPawn);
	if (MycelandCharacter)
	{
		MycelandCharacter->OnCurrentTileChanged.AddDynamic(this, &AML_PlayerController::HandleCurrentTileChanged);
		MycelandCharacter->OnBoardChanged.AddDynamic(this, &AML_PlayerController::HandleBoardStateChanged);
		HoverPreviewComponent->Initialize(this, MycelandCharacter);
		TransitionComponent->Initialize(this, MycelandCharacter, EnergyComponent, DevSettings, RotateSpeed);

		const EML_PlayerMovementMode InitialMode = MycelandCharacter->CurrentTileOn
			? EML_PlayerMovementMode::InsideBoard
			: EML_PlayerMovementMode::FreeMovement;
		TransitionComponent->SwitchToMode(InitialMode);
		
		// Always start hover timer for cursor glow
		HoverPreviewComponent->StartHoverPreviewTimer();
	}
}


// ==================== Input ====================

// Bound to OnStarted — fires once per click. Dispatches to the appropriate sub-handler.
void AML_PlayerController::OnSetDestinationStarted()
{
	FollowTime = 0.f;

	const EML_PlayerMovementMode Mode = TransitionComponent->GetMovementMode();
	if (Mode == EML_PlayerMovementMode::InsideBoard)
		HandleInsideBoardClick();
	else if (Mode == EML_PlayerMovementMode::FreeMovement)
		HandleFreeMovementClick();
}

// Called when the player clicks while inside a board.
// Either starts a BFS move to a tile, or begins an exit hold when clicking outside.
void AML_PlayerController::HandleInsideBoardClick()
{
	if (!IsValid(MycelandCharacter) || !IsValid(MycelandCharacter->CurrentTileOn)) return;

	AML_BoardSpawner* Board = MycelandCharacter->CurrentTileOn->GetBoardSpawnerFromTile();
	if (!IsValid(Board)) return;

	// TurningToPlant locks all input — propagation is imminent
	if (TransitionComponent->GetBoardActionState() == EML_PlayerBoardActionState::TurningToPlant) return;

	const TMap<FIntPoint, AML_Tile*> GridMap = Board->GetGridMap();
	AML_Tile* TargetTile = GetTileUnderCursor();

	if (IsValid(TargetTile) && TargetTile->GetOwner() == Board)
	{
		Move(TargetTile);
		return;
	}

	// Click outside the board → hold to exit toward the closest gate tile (EntryTile or ExitTile)
	FHitResult Hit;
	if (!GetHitResultUnderCursorByChannel(UEngineTypes::ConvertToTraceType(ECC_Visibility), true, Hit)) return;
	if (!IsClickableGround(Hit)) return;
	
	AML_Tile* ExitGate = Board->GetClosestGateTile(Hit.Location);
	if (!IsValid(ExitGate)) return;

	// If the path to the gate is blocked (e.g. obstacles in the way), don't start the hold
	const FIntPoint CurrentAxial = MycelandCharacter->CurrentTileOn->GetAxialCoord();
	const FIntPoint ExitAxial    = ExitGate->GetAxialCoord();
	TArray<FIntPoint> TestPath;
	if (!UML_HexPathfinder::BuildPath_AxialBFS(CurrentAxial, ExitAxial, GridMap, TestPath)) return;

	TransitionComponent->RequestExitHold(ExitGate, Hit.Location);
}

// Called when the player clicks while in free movement.
// Re-enters a board if a board tile is clicked; otherwise caches the ground destination.
void AML_PlayerController::HandleFreeMovementClick()
{
	AML_Tile* TargetTile = GetTileUnderCursor();
	if (IsValid(TargetTile))
	{
		AML_BoardSpawner* Board = TargetTile->GetBoardSpawnerFromTile();
		if (!IsValid(Board)) return;

		if (!IsValid(MycelandCharacter)) return;
		AML_Tile* GateTile = Board->GetClosestGateTile(MycelandCharacter->GetActorLocation());
		if (!IsValid(GateTile)) return;

		TransitionComponent->RequestBoardEntry(TargetTile);
		StartNavMeshMovement(GateTile->GetActorLocation());
		return;
	}

	// Click on open ground → cache destination; continuous movement is driven by OnSetDestinationTriggered.
	FHitResult Hit;
	if (!GetHitResultUnderCursorByChannel(UEngineTypes::ConvertToTraceType(ECC_Visibility), true, Hit)) return;
	if (!IsClickableGround(Hit)) return;
	HoldMoveCachedDestination = Hit.Location;
}

// Bound to OnTriggered — fires every frame while the button is held.
// In FreeMovement only: continuously moves the character toward the cursor (hold-to-move).
// In board modes the existing tile-by-tile logic drives movement; this function is a no-op there.
void AML_PlayerController::OnSetDestinationTriggered()
{
	// Accumulate hold time every frame the input is held
	FollowTime += GetWorld()->GetDeltaSeconds();

	// Hold-to-move is only active in free movement
	if (TransitionComponent->GetMovementMode() != EML_PlayerMovementMode::FreeMovement)
		return;

	if (!IsValid(MycelandCharacter))
		return;

	// Update the cached destination to the current cursor position every frame
	FHitResult Hit;
	if (!GetHitResultUnderCursorByChannel(UEngineTypes::ConvertToTraceType(ECC_Visibility), true, Hit)) return;
	if (!IsClickableGround(Hit)) return;
	
	// Only follow the cursor on open ground — ignore board tiles so that
	// clicking on a board still triggers the re-entry logic on release.
	if (!Cast<AML_Tile>(Hit.GetActor()))
		HoldMoveCachedDestination = Hit.Location;

	// Push the character toward the cached destination every frame
	const FVector WorldDirection = (HoldMoveCachedDestination - MycelandCharacter->GetActorLocation()).GetSafeNormal();
	MycelandCharacter->AddMovementInput(WorldDirection, MoveSpeedScale);
}

// Bound to OnCompleted / OnCanceled
void AML_PlayerController::OnSetDestinationReleased()
{
	TransitionComponent->CancelExitHold();
	// TickExitHold detects the cleared flag and performs cleanup on the next tick.

	// In free movement, if this was a short tap (not a hold), use SimpleMoveToLocation
	// so the character navigates precisely to the clicked point via the nav mesh.
	if (TransitionComponent->GetMovementMode() == EML_PlayerMovementMode::FreeMovement)
	{
		if (FollowTime <= ShortPressThreshold)
			StartNavMeshMovement(HoldMoveCachedDestination);
		
		// If it was a long hold, movement was already applied frame-by-frame; nothing extra needed.
	}

	FollowTime = 0.f;
}

void AML_PlayerController::OnMoveAndPlantStarted()
{
	AML_Tile* TargetTile = GetTileUnderCursor();
	Plant(TargetTile);
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
	if (!UML_HexPathfinder::BuildPath_AxialBFS(StartAxial, TargetAxial, GridMap, AxialPath))
	{
		if (bFallbackTeleport)
			MycelandCharacter->SetActorLocation(TeleportFallbackWorld);
		return false;
	}

	return StartRecordedBoardMove(AxialPath, GridMap);
}

void AML_PlayerController::StartMoveAlongAxialPathForUndo(const TArray<FIntPoint>& AxialPath,
                                                          const TArray<FIntPoint>& PickedCollectibleAxials)
{
	if (!IsValid(MycelandCharacter) || !IsValid(MycelandCharacter->CurrentTileOn)) return;

	AML_BoardSpawner* Board = MycelandCharacter->CurrentTileOn->GetBoardSpawnerFromTile();
	if (!IsValid(Board)) return;

	const TMap<FIntPoint, AML_Tile*> GridMap = Board->GetGridMap();

	MoveRecordingComponent->BeginUndoPlayback(AxialPath, PickedCollectibleAxials);
	StartMoveAlongPath(AxialPath, GridMap);
}

void AML_PlayerController::NotifyCollectiblePickedOnAxial(const FIntPoint& Axial)
{
	MoveRecordingComponent->NotifyCollectiblePicked(Axial);
}
