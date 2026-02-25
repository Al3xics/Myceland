// Copyright Myceland Team, All Rights Reserved.


#include "Player/ML_PlayerController.h"

#include "Developer Settings/ML_MycelandDeveloperSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Player/ML_PlayerCharacter.h"
#include "Subsystem/ML_WavePropagationSubsystem.h"
#include "Tiles/ML_Tile.h"
#include "Tiles/ML_TileBase.h"

class UML_WavePropagationSubsystem;

AML_Tile* AML_PlayerController::GetTileUnderCursor() const
{
	FHitResult Hit;
	if (!GetHitResultUnderCursorByChannel(UEngineTypes::ConvertToTraceType(ECC_Visibility), true, Hit))
	{
		return nullptr;
	}

	// 1) Direct actor hit
	if (AML_Tile* Tile = Cast<AML_Tile>(Hit.GetActor()))
	{
		return Tile;
	}

	// 2) If we hit a component that belongs to something else (ChildActor, mesh actor, etc.)
	if (UPrimitiveComponent* Comp = Hit.GetComponent())
	{
		// Try outer chain
		if (AML_Tile* OuterTile = Comp->GetTypedOuter<AML_Tile>())
		{
			return OuterTile;
		}
	}

	// 3) If we hit a child actor spawned by your tile (TileChildActor)
	if (AActor* HitActor = Hit.GetActor())
	{
		if (AActor* Parent = HitActor->GetParentActor())
		{
			if (AML_Tile* ParentTile = Cast<AML_Tile>(Parent))
			{
				return ParentTile;
			}
		}

		// 4) Sometimes the tile is the owner of the hit actor
		if (AActor* Owning = HitActor->GetOwner())
		{
			if (AML_Tile* OwnerTile = Cast<AML_Tile>(Owning))
			{
				return OwnerTile;
			}
		}
	}

	return nullptr;
}

AML_BoardSpawner* AML_PlayerController::GetBoardFromCurrentTile(const AML_PlayerCharacter* MycelandCharacter) const
{
	if (!IsValid(MycelandCharacter) || !IsValid(MycelandCharacter->CurrentTileOn))
		return nullptr;

	return Cast<AML_BoardSpawner>(MycelandCharacter->CurrentTileOn->GetOwner());
}

bool AML_PlayerController::IsTileWalkable(const AML_Tile* Tile) const
{
	if (!IsValid(Tile))
		return false;

	const EML_TileType Type = Tile->GetCurrentType();
	const bool bTypeWalkable = (Type == EML_TileType::Dirt || Type == EML_TileType::Grass);

	return bTypeWalkable && !Tile->IsBlocked();
}

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
			if (!NextTilePtr)
				continue;

			if (!IsTileWalkable(*NextTilePtr))
				continue;

			CameFrom.Add(Next, Current);

			if (Next == GoalAxial)
			{
				// reconstruct
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

void AML_PlayerController::StartMoveAlongPath(const TArray<FIntPoint>& AxialPath,
                                              const TMap<FIntPoint, AML_Tile*>& GridMap)
{
	CurrentPathWorld.Reset();
	CurrentPathIndex = 0;

	CurrentPathWorld.Reserve(AxialPath.Num());

	for (const FIntPoint& Axial : AxialPath)
	{
		if (AML_Tile* const* TilePtr = GridMap.Find(Axial))
		{
			if (IsValid(*TilePtr))
				CurrentPathWorld.Add((*TilePtr)->GetActorLocation());
		}
	}

	// Skip first point if already basically there
	if (APawn* P = GetPawn())
	{
		if (CurrentPathWorld.Num() > 0)
		{
			if (FVector::DistSquared2D(P->GetActorLocation(), CurrentPathWorld[0]) <= FMath::Square(AcceptanceRadius))
				CurrentPathIndex = 1;
		}
	}
}

void AML_PlayerController::TickMoveAlongPath(float DeltaTime)
{
	ensureMsgf(CurrentPathWorld.Num() == CurrentPathIndex, TEXT("Path is not initialized!"));
	if (CurrentPathWorld.Num() == 0 || CurrentPathIndex >= CurrentPathWorld.Num()) return;

	AML_PlayerCharacter* MycelandCharacter = Cast<AML_PlayerCharacter>(GetPawn());
	ensureMsgf(MycelandCharacter, TEXT("Player Character is not set!"));
	if (!IsValid(MycelandCharacter)) return;

	const FVector Loc = MycelandCharacter->GetActorLocation();

	// ---------- Corner cut / skip waypoint ----------
	// If we have at least one more waypoint ahead, we can try to skip the current one.
	if (CornerCutStrength > 0.f && CurrentPathIndex + 1 < CurrentPathWorld.Num())
	{
		const FVector P1 = CurrentPathWorld[CurrentPathIndex];
		const FVector P2 = CurrentPathWorld[CurrentPathIndex + 1];

		// Compute 2D distance from player to the segment [P1 -> P2]
		const FVector A = FVector(P1.X, P1.Y, 0.f);
		const FVector B = FVector(P2.X, P2.Y, 0.f);
		const FVector P = FVector(Loc.X, Loc.Y, 0.f);

		const FVector AB = (B - A);
		const float AB2 = AB.SizeSquared();

		if (AB2 > KINDA_SMALL_NUMBER)
		{
			const float T = FMath::Clamp(FVector::DotProduct(P - A, AB) / AB2, 0.f, 1.f);
			const FVector Closest = A + T * AB;
			const float DistToSegment = FVector::Dist(P, Closest);

			const float CutDist = BaseCornerCutDistance * CornerCutStrength;

			// If we're close enough to the shortcut segment, skip the middle waypoint
			if (DistToSegment <= CutDist)
				CurrentPathIndex++; // skip P1, go directly toward P2
		}
	}

	// ---------- Normal movement toward current waypoint ----------
	const FVector Target = CurrentPathWorld[CurrentPathIndex];
	FVector To = Target - Loc;
	To.Z = 0.f;

	const float Dist = To.Size();
	if (Dist <= AcceptanceRadius)
	{
		CurrentPathIndex++;
		if (CurrentPathIndex >= CurrentPathWorld.Num())
		{
			CurrentPathWorld.Reset();
			CurrentPathIndex = 0;
		}
		return;
	}

	To.Normalize();
	MycelandCharacter->AddMovementInput(To, MoveSpeedScale);
}

void AML_PlayerController::InitNumberOfEnergyForLevel()
{
	const UML_MycelandDeveloperSettings* Settings = GetDefault<UML_MycelandDeveloperSettings>();
	if (!Settings) return;

	const FString CurrentLevelName = UGameplayStatics::GetCurrentLevelName(this, true);

	for (const TPair<FGameplayTag, TSoftObjectPtr<UWorld>>& Pair : Settings->Levels)
	{
		if (!Pair.Value.IsNull())
		{
			FString AssetName = Pair.Value.GetAssetName();

			if (AssetName == CurrentLevelName)
			{
				if (const int* Energy = Settings->EnergyPerLevel.Find(Pair.Key))
					CurrentEnergy = *Energy;

				break;
			}
		}
	}
}

void AML_PlayerController::BeginPlay()
{
	Super::BeginPlay();
	InitNumberOfEnergyForLevel();
	GetWorld()->GetSubsystem<UML_WavePropagationSubsystem>()->EnsureInitialized();
}

void AML_PlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	TickMoveAlongPath(DeltaTime);

	if (!bTurningToTile) return;

	RotateCharacterTowardTile(RotateTargetTile, DeltaTime, RotateSpeed);
}

void AML_PlayerController::OnSetDestinationTriggered()
{
	AML_PlayerCharacter* MycelandCharacter = Cast<AML_PlayerCharacter>(GetPawn());
	ensureMsgf(MycelandCharacter, TEXT("Player Character is not set!"));
	if (!IsValid(MycelandCharacter)) return;

	ensureMsgf(IsValid(MycelandCharacter->CurrentTileOn), TEXT("Current tile is not set!"));
	if (!IsValid(MycelandCharacter->CurrentTileOn)) return;

	AML_BoardSpawner* Board = GetBoardFromCurrentTile(MycelandCharacter);
	ensureMsgf(Board, TEXT("Board is not set!"));
	if (!IsValid(Board)) return;

	AML_Tile* TargetTile = GetTileUnderCursor();
	ensureMsgf(IsValid(TargetTile), TEXT("Target tile is not set!"));
	if (!IsValid(TargetTile)) return;

	ensureMsgf(TargetTile->GetOwner() == Board, TEXT("Target tile is not owned by the board!"));
	if (TargetTile->GetOwner() != Board) return;

	const FIntPoint StartAxial = MycelandCharacter->CurrentTileOn->GetAxialCoord();
	const FIntPoint GoalAxial = TargetTile->GetAxialCoord();

	const TMap<FIntPoint, AML_Tile*> GridMap = Board->GetGridMap();

	ensureMsgf(GridMap.Contains(StartAxial), TEXT("Start axial is not in the grid map!"));
	if (!GridMap.Contains(StartAxial) || !GridMap.Contains(GoalAxial)) return;

	ensureMsgf(IsTileWalkable(GridMap[StartAxial]) && IsTileWalkable(GridMap[GoalAxial]),
	           TEXT("Start or goal axial is not walkable!"));
	if (!IsTileWalkable(GridMap[StartAxial]) || !IsTileWalkable(GridMap[GoalAxial])) return;

	TArray<FIntPoint> AxialPath;
	ensureMsgf(BuildPath_AxialBFS(StartAxial, GoalAxial, GridMap, AxialPath), TEXT("Path is not valid!"));
	if (!BuildPath_AxialBFS(StartAxial, GoalAxial, GridMap, AxialPath)) return;

	StartMoveAlongPath(AxialPath, GridMap);
}

void AML_PlayerController::RotateCharacterTowardTile(const AML_Tile* HitTileActor, float DeltaTime,
                                                     float TurnSpeed)
{
	AML_PlayerCharacter* MycelandCharacter = Cast<AML_PlayerCharacter>(GetCharacter());
	if (!IsValid(MycelandCharacter) || !IsValid(HitTileActor)) return;

	StopMovement();

	FVector Dir = HitTileActor->GetActorLocation() - MycelandCharacter->GetActorLocation();
	Dir.Z = 0.f;
	if (Dir.IsNearlyZero()) return;

	const float DesiredYaw = Dir.Rotation().Yaw;
	FRotator CurrentRot = MycelandCharacter->GetActorRotation();
	FRotator DesiredRot = FRotator(0.f, DesiredYaw, 0.f);
	const FRotator NewRot = FMath::RInterpTo(CurrentRot, DesiredRot, DeltaTime, TurnSpeed);

	MycelandCharacter->SetActorRotation(NewRot);
	const float YawError = FMath::Abs(FMath::FindDeltaAngleDegrees(NewRot.Yaw, DesiredYaw));
	const float AcceptYawErrorDeg = 0.1f;
	if (YawError <= AcceptYawErrorDeg)
	{
		bTurningToTile = false;
	}
}

void AML_PlayerController::TryPlantGrass(FHitResult HitResult, bool& CanPlantGrass, AML_Tile*& HitTile)
{
	CanPlantGrass = false;
	HitTile = nullptr;

	const AML_PlayerCharacter* MycelandCharacter = Cast<AML_PlayerCharacter>(GetCharacter());
	ensureMsgf(MycelandCharacter, TEXT("Player Character is not set!"));
	if (!MycelandCharacter) return;

	AML_Tile* CurrentTileOn = MycelandCharacter->CurrentTileOn;
	ensureMsgf(CurrentTileOn, TEXT("Current tile is not set!"));
	if (!CurrentTileOn) return;

	const AActor* HitActor = HitResult.GetActor();
	ensureMsgf(HitActor, TEXT("Hit actor is not set!"));
	if (!HitActor) return;

	// Get all the neighbors of the tile the player is on
	TArray<AML_Tile*> Neighbors = CurrentTileOn->GetBoardSpawnerFromTile()->GetNeighbors(CurrentTileOn);
	for (const AML_Tile* Neighbor : Neighbors)
	{
		if (!Neighbor) continue;

		// Get the child actor (TileChildActor) of TileBase
		const AML_TileBase* Tile = Cast<AML_TileBase>(Neighbor->GetTileChildActor()->GetChildActor());

		// Get the parent of the HitActor and do multiple checks
		if (const AML_Tile* HitTileActor = Cast<AML_Tile>(HitActor))
		{
			if (HitTileActor == Neighbor &&
				Neighbor->GetCurrentType() == EML_TileType::Dirt &&
				CurrentEnergy > 0)
			{
				RotateTargetTile = HitTileActor;
				bTurningToTile = true;
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

	if (UML_WavePropagationSubsystem* WavePropagationSubsystem = GetWorld()->GetSubsystem<
		UML_WavePropagationSubsystem>())
		WavePropagationSubsystem->BeginTileResolved(HitTile);
}