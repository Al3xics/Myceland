// Copyright Myceland Team, All Rights Reserved.

#include "Player/ML_PlayerController.h"

#include "Developer Settings/ML_MycelandDeveloperSettings.h"
#include "Player/ML_PlayerCharacter.h"
#include "Subsystem/ML_WavePropagationSubsystem.h"
#include "Tiles/ML_Tile.h"
#include "Tiles/ML_TileBase.h"

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
	if (!IsValid(Tile))
		return false;

	const EML_TileType Type = Tile->GetCurrentType();
	return (Type == EML_TileType::Dirt || Type == EML_TileType::Grass) && !Tile->IsBlocked();
}

AML_Tile* AML_PlayerController::FindNearestWalkableTile(const FVector& WorldLocation, const TMap<FIntPoint, AML_Tile*>& GridMap) const
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


// ==================== Pathfinding ====================

bool AML_PlayerController::BuildPath_AxialBFS(const FIntPoint& StartAxial, const FIntPoint& GoalAxial, const TMap<FIntPoint, AML_Tile*>& GridMap, TArray<FIntPoint>& OutAxialPath) const
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

void AML_PlayerController::StartMoveAlongPath(const TArray<FIntPoint>& AxialPath, const TMap<FIntPoint, AML_Tile*>& GridMap)
{
	CurrentPathWorld.Reset();
	CurrentPathIndex = 0;
	CurrentPathWorld.Reserve(AxialPath.Num());

	for (const FIntPoint& Axial : AxialPath)
		if (AML_Tile* const* TilePtr = GridMap.Find(Axial))
			if (IsValid(*TilePtr))
				CurrentPathWorld.Add((*TilePtr)->GetActorLocation());

	// Skip first point if already basically there
	if (APawn* P = GetPawn())
		if (CurrentPathWorld.Num() > 0)
			if (FVector::DistSquared2D(P->GetActorLocation(), CurrentPathWorld[0]) <= FMath::Square(AcceptanceRadius))
				CurrentPathIndex = 1;
}

void AML_PlayerController::StartMoveToWorldLocation(const FVector& WorldLocation)
{
	CurrentPathWorld.Reset();
	CurrentPathIndex = 0;
	CurrentPathWorld.Add(WorldLocation);
}

void AML_PlayerController::TickMoveAlongPath(float DeltaTime)
{
	// In FreeMovement, stop as soon as the button is released
	if (CurrentMovementMode == EML_PlayerMovementMode::FreeMovement && !bIsHoldingFreeInput)
	{
		CurrentPathWorld.Reset();
		CurrentPathIndex = 0;
		bIsMoving = false;
	}

	if (CurrentPathWorld.Num() == 0 || CurrentPathIndex >= CurrentPathWorld.Num()) return;

	ensureMsgf(MycelandCharacter, TEXT("Player Character is not set!"));
	if (!IsValid(MycelandCharacter)) return;

	const FVector Loc = MycelandCharacter->GetActorLocation();

	// ---------- Corner cut ----------
	if (CornerCutStrength > 0.f && CurrentPathIndex + 1 < CurrentPathWorld.Num())
	{
		const FVector A = FVector(CurrentPathWorld[CurrentPathIndex].X,   CurrentPathWorld[CurrentPathIndex].Y,   0.f);
		const FVector B = FVector(CurrentPathWorld[CurrentPathIndex+1].X, CurrentPathWorld[CurrentPathIndex+1].Y, 0.f);
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

	if (To.Size() <= AcceptanceRadius)
	{
		CurrentPathIndex++;
		if (CurrentPathIndex >= CurrentPathWorld.Num())
		{
			CurrentPathWorld.Reset();
			CurrentPathIndex = 0;
			bIsMoving = false;
			OnPathFinished();
		}
		return;
	}

	To.Normalize();
	MycelandCharacter->AddMovementInput(To, MoveSpeedScale);
}

void AML_PlayerController::OnPathFinished()
{
	// Arrived at the border tile → switch to free movement
	if (bPendingFreeMovementOnArrival)
	{
		bPendingFreeMovementOnArrival = false;
		CurrentMovementMode = EML_PlayerMovementMode::FreeMovement;
		return;
	}

	// Arrived at the board entry tile → switch to board movement and immediately path to stored target
	if (bPendingBoardEntryOnArrival)
	{
		bPendingBoardEntryOnArrival = false;
		CurrentMovementMode = EML_PlayerMovementMode::InsideBoard;

		if (!IsValid(MycelandCharacter) || !IsValid(MycelandCharacter->CurrentTileOn)) return;

		AML_BoardSpawner* Board = MycelandCharacter->CurrentTileOn->GetBoardSpawnerFromTile();
		if (!IsValid(Board)) return;

		AML_Tile* TargetTile = PendingBoardEntryTargetTile;
		PendingBoardEntryTargetTile = nullptr;

		if (!IsValid(TargetTile) || TargetTile->GetOwner() != Board) return;

		const TMap<FIntPoint, AML_Tile*> GridMap = Board->GetGridMap();
		const FIntPoint StartAxial = MycelandCharacter->CurrentTileOn->GetAxialCoord();
		const FIntPoint GoalAxial  = TargetTile->GetAxialCoord();

		if (!GridMap.Contains(StartAxial) || !GridMap.Contains(GoalAxial)) return;
		if (!IsTileWalkable(GridMap[StartAxial]) || !IsTileWalkable(GridMap[GoalAxial])) return;

		TArray<FIntPoint> AxialPath;
		if (!BuildPath_AxialBFS(StartAxial, GoalAxial, GridMap, AxialPath)) return;

		StartMoveAlongPath(AxialPath, GridMap);
		bIsMoving = true;
	}
}


// ==================== Board Exit / Entry ====================

void AML_PlayerController::TickExitHold(float DeltaTime)
{
	if (CurrentMovementMode != EML_PlayerMovementMode::ExitingBoard) return;

	if (!bIsHoldingExitInput)
	{
		// Released too early → cancel
		ExitHoldTimer = 0.f;
		CurrentMovementMode = EML_PlayerMovementMode::InsideBoard;
		PendingExitTile = nullptr;
		return;
	}

	ExitHoldTimer += DeltaTime;
	if (ExitHoldTimer >= DevSettings->ExitBoardHoldDuration)
	{
		ExitHoldTimer = 0.f;
		ConfirmExitBoard();
	}
}

void AML_PlayerController::ConfirmExitBoard()
{
	if (!IsValid(PendingExitTile) || !IsValid(MycelandCharacter) || !IsValid(MycelandCharacter->CurrentTileOn)) return;

	AML_BoardSpawner* Board = MycelandCharacter->CurrentTileOn->GetBoardSpawnerFromTile();
	if (!IsValid(Board)) return;

	const TMap<FIntPoint, AML_Tile*> GridMap = Board->GetGridMap();
	const FIntPoint StartAxial = MycelandCharacter->CurrentTileOn->GetAxialCoord();
	const FIntPoint GoalAxial  = PendingExitTile->GetAxialCoord();

	if (!GridMap.Contains(StartAxial) || !GridMap.Contains(GoalAxial)) return;

	TArray<FIntPoint> AxialPath;
	if (!BuildPath_AxialBFS(StartAxial, GoalAxial, GridMap, AxialPath)) return;

	// Still in board mode during this walk; FreeMovement triggers on arrival
	CurrentMovementMode = EML_PlayerMovementMode::InsideBoard;
	bPendingFreeMovementOnArrival = true;
	PendingExitTile = nullptr;

	StartMoveAlongPath(AxialPath, GridMap);
	bIsMoving = true;
}


// ==================== Delegates ====================

void AML_PlayerController::HandleBoardStateChanged(const AML_Tile* NewTile)
{
	// ---------- Energy ----------
	if (NewTile)
		InitNumberOfEnergyForLevel(NewTile->GetBoardSpawnerFromTile()->GetEnergyForPuzzle());
	else
		InitNumberOfEnergyForLevel(0);
	
	// ---------- Transition: Free -> InsideBoard ----------
	if (CurrentMovementMode == EML_PlayerMovementMode::FreeMovement && IsValid(NewTile))
	{
		// Stop free movement logic
		bIsHoldingFreeInput = false;

		CurrentMovementMode = EML_PlayerMovementMode::InsideBoard;

		// Clear any previous path
		CurrentPathWorld.Reset();
		CurrentPathIndex = 0;

		if (!IsValid(MycelandCharacter))
			return;

		// Smoothly move to a tile center instead of teleport
		FVector TileCenter = NewTile->GetActorLocation();
		TileCenter.Z = MycelandCharacter->GetActorLocation().Z;

		CurrentPathWorld.Add(TileCenter);
		CurrentPathIndex = 0;
		bIsMoving = true;
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
	TickMoveAlongPath(DeltaTime);
	TickExitHold(DeltaTime);
}

void AML_PlayerController::OnPossess(APawn* aPawn)
{
	Super::OnPossess(aPawn);
	MycelandCharacter = Cast<AML_PlayerCharacter>(aPawn);
	if (MycelandCharacter)
		MycelandCharacter->OnBoardChanged.AddDynamic(this, &AML_PlayerController::HandleBoardStateChanged);
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
			const FIntPoint GoalAxial  = TargetTile->GetAxialCoord();

			if (!GridMap.Contains(StartAxial) || !GridMap.Contains(GoalAxial)) return;
			if (!IsTileWalkable(GridMap[StartAxial]) || !IsTileWalkable(GridMap[GoalAxial])) return;

			TArray<FIntPoint> AxialPath;
			if (!BuildPath_AxialBFS(StartAxial, GoalAxial, GridMap, AxialPath)) return;

			StartMoveAlongPath(AxialPath, GridMap);
			bIsMoving = true;
			return;
		}

		// Click outside the board → start exit hold
		FHitResult Hit;
		if (!GetHitResultUnderCursorByChannel(UEngineTypes::ConvertToTraceType(ECC_Visibility), true, Hit)) return;

		AML_Tile* NearestTile = FindNearestWalkableTile(Hit.Location, GridMap);
		if (!IsValid(NearestTile)) return;

		PendingExitTile = NearestTile;
		CurrentMovementMode = EML_PlayerMovementMode::ExitingBoard;
		bIsHoldingExitInput = true;
		ExitHoldTimer = 0.f;
		return;
	}

	// --- FREE MOVEMENT — click on a board tile → re-enter ---
	if (CurrentMovementMode == EML_PlayerMovementMode::FreeMovement)
	{
		AML_Tile* TargetTile = GetTileUnderCursor();
		if (!IsValid(TargetTile)) return;

		AML_BoardSpawner* Board = TargetTile->GetBoardSpawnerFromTile();
		if (!IsValid(Board)) return;

		// Store target so OnPathFinished can BFS to it after entry
		PendingBoardEntryTargetTile = TargetTile;
		bPendingBoardEntryOnArrival = true;
		CurrentMovementMode = EML_PlayerMovementMode::EnteringBoard;

		StartMoveToWorldLocation(TargetTile->GetActorLocation());
		bIsMoving = true;
	}
}

// Bound to OnTriggered — fires every frame while held
// Handles: continuous free movement toward cursor
void AML_PlayerController::OnSetDestinationTriggered()
{
	if (CurrentMovementMode != EML_PlayerMovementMode::FreeMovement) return;

	bIsHoldingFreeInput = true;

	FHitResult Hit;
	if (!GetHitResultUnderCursorByChannel(UEngineTypes::ConvertToTraceType(ECC_Visibility), true, Hit))
		return;

	// Always follow the mouse in free mode
	StartMoveToWorldLocation(Hit.Location);
	bIsMoving = true;
}

// Bound to OnCompleted / OnCanceled
void AML_PlayerController::OnSetDestinationReleased()
{
	bIsHoldingExitInput = false;
	bIsHoldingFreeInput = false;
}


// ==================== Energy ====================

void AML_PlayerController::InitNumberOfEnergyForLevel(const int32 Energy)
{
	CurrentEnergy = Energy;
}


// ==================== Actions ====================

void AML_PlayerController::TryPlantGrass(FHitResult HitResult, bool& CanPlantGrass, AML_Tile*& HitTile)
{
	if (bIsMoving) return;
	if (CurrentMovementMode != EML_PlayerMovementMode::InsideBoard) return;

	CanPlantGrass = false;
	HitTile = nullptr;

	if (!MycelandCharacter) return;

	AML_Tile* CurrentTileOn = MycelandCharacter->CurrentTileOn;
	if (!CurrentTileOn) return;

	const AActor* HitActor = HitResult.GetActor();
	if (!HitActor) return;

	TArray<AML_Tile*> Neighbors = CurrentTileOn->GetBoardSpawnerFromTile()->GetNeighbors(CurrentTileOn);
	for (const AML_Tile* Neighbor : Neighbors)
	{
		if (!Neighbor) continue;

		if (const AML_Tile* HitTileActor = Cast<AML_Tile>(HitActor))
		{
			if (HitTileActor == Neighbor &&
				Neighbor->GetCurrentType() == EML_TileType::Dirt &&
				CurrentEnergy > 0)
			{
				HitTile = const_cast<AML_Tile*>(HitTileActor);
				CanPlantGrass = true;
				return;
			}
		}
	}
}

void AML_PlayerController::ConfirmTurn(AML_Tile* HitTile)
{
	CurrentEnergy--;

	if (UML_WavePropagationSubsystem* WavePropagationSubsystem = GetWorld()->GetSubsystem<UML_WavePropagationSubsystem>())
		WavePropagationSubsystem->BeginTileResolved(HitTile);
}