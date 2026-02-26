// Copyright Myceland Team, All Rights Reserved.


#include "Player/ML_PlayerController.h"

#include "Developer Settings/ML_MycelandDeveloperSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Player/ML_PlayerCharacter.h"
#include "Subsystem/ML_WavePropagationSubsystem.h"
#include "Tiles/ML_Tile.h"
#include "Tiles/ML_TileBase.h"

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

AML_BoardSpawner* AML_PlayerController::GetBoardFromCurrentTile(const AML_PlayerCharacter* MycelandCharacter) const
{
	if (!IsValid(MycelandCharacter) || !IsValid(MycelandCharacter->CurrentTileOn))
		return nullptr;

	return Cast<AML_BoardSpawner>(MycelandCharacter->CurrentTileOn->GetOwner());
}

bool AML_PlayerController::IsTileWalkable(const AML_Tile* Tile) const
{
	if (!IsValid(Tile)) return false;

	const EML_TileType Type = Tile->GetCurrentType();
	const bool bTypeWalkable = (Type == EML_TileType::Dirt || Type == EML_TileType::Grass);
	return bTypeWalkable && !Tile->IsBlocked();
}

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
			if (!NextTilePtr) continue;
			if (!IsTileWalkable(*NextTilePtr)) continue;

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

void AML_PlayerController::StartMoveAlongPath(const TArray<FIntPoint>& AxialPath, const TMap<FIntPoint, AML_Tile*>& GridMap)
{
	CurrentPathWorld.Reset();
	CurrentPathIndex = 0;

	CurrentPathWorld.Reserve(AxialPath.Num());

	for (const FIntPoint& Axial : AxialPath)
	{
		if (AML_Tile* const* TilePtr = GridMap.Find(Axial))
			if (IsValid(*TilePtr))
				CurrentPathWorld.Add((*TilePtr)->GetActorLocation());
	}

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
	if (CurrentPathWorld.Num() == 0 || CurrentPathIndex >= CurrentPathWorld.Num())
		return;

	AML_PlayerCharacter* MycelandCharacter = Cast<AML_PlayerCharacter>(GetPawn());
	if (!IsValid(MycelandCharacter)) return;

	const FVector Loc = MycelandCharacter->GetActorLocation();

	// Corner cut
	if (CornerCutStrength > 0.f && CurrentPathIndex + 1 < CurrentPathWorld.Num())
	{
		const FVector P1 = CurrentPathWorld[CurrentPathIndex];
		const FVector P2 = CurrentPathWorld[CurrentPathIndex + 1];

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
			if (DistToSegment <= CutDist)
				CurrentPathIndex++;
		}
	}

	const FVector Target = CurrentPathWorld[CurrentPathIndex];
	FVector To = Target - Loc;
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
			// end
			CurrentPathWorld.Reset();
			CurrentPathIndex = 0;

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

					S->NotifyUndoMoveFinished();
				}
				else if (bMoveInProgress && ActiveMoveAxialPath.Num() > 0)
				{
					if (!bSuppressMoveRecording)
					{
						const TArray<FIntPoint> Picked = ActiveMovePickedCollectibles.Array();

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
			return;
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
			if (Pair.Value.GetAssetName() == CurrentLevelName)
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
}

void AML_PlayerController::OnSetDestinationTriggered()
{
	AML_PlayerCharacter* MycelandCharacter = Cast<AML_PlayerCharacter>(GetPawn());
	if (!IsValid(MycelandCharacter) || !IsValid(MycelandCharacter->CurrentTileOn)) return;

	AML_BoardSpawner* Board = GetBoardFromCurrentTile(MycelandCharacter);
	if (!IsValid(Board)) return;

	AML_Tile* TargetTile = GetTileUnderCursor();
	if (!IsValid(TargetTile)) return;
	if (TargetTile->GetOwner() != Board) return;

	const FIntPoint StartAxial = MycelandCharacter->CurrentTileOn->GetAxialCoord();
	const FIntPoint GoalAxial  = TargetTile->GetAxialCoord();

	const TMap<FIntPoint, AML_Tile*> GridMap = Board->GetGridMap();
	if (!GridMap.Contains(StartAxial) || !GridMap.Contains(GoalAxial)) return;
	if (!IsTileWalkable(GridMap[StartAxial]) || !IsTileWalkable(GridMap[GoalAxial])) return;

	TArray<FIntPoint> AxialPath;
	if (!BuildPath_AxialBFS(StartAxial, GoalAxial, GridMap, AxialPath)) return;

	// record move
	bMoveInProgress = true;
	MoveStartAxial = StartAxial;
	MoveEndAxial   = GoalAxial;

	if (APawn* P = GetPawn())
		MoveStartWorld = P->GetActorLocation();

	MoveEndWorld = TargetTile->GetActorLocation();

	ActiveMoveAxialPath = AxialPath;
	ActiveMovePickedCollectibles.Reset();

	StartMoveAlongPath(AxialPath, GridMap);
}

void AML_PlayerController::TryPlantGrass(FHitResult HitResult, bool& CanPlantGrass, AML_Tile*& HitTile)
{
	CanPlantGrass = false;
	HitTile = nullptr;

	const AML_PlayerCharacter* MycelandCharacter = Cast<AML_PlayerCharacter>(GetCharacter());
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
	if (UML_WavePropagationSubsystem* S = GetWorld()->GetSubsystem<UML_WavePropagationSubsystem>())
		S->BeginTileResolved(HitTile);

	CurrentEnergy--;
}

bool AML_PlayerController::MovePlayerToAxial(const FIntPoint& TargetAxial, bool bUsePath, bool bFallbackTeleport, const FVector& TeleportFallbackWorld)
{
	AML_PlayerCharacter* MycelandCharacter = Cast<AML_PlayerCharacter>(GetPawn());
	if (!IsValid(MycelandCharacter) || !IsValid(MycelandCharacter->CurrentTileOn)) return false;

	AML_BoardSpawner* Board = GetBoardFromCurrentTile(MycelandCharacter);
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
	MoveEndAxial   = TargetAxial;

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

void AML_PlayerController::StartMoveAlongAxialPathForUndo(
	const TArray<FIntPoint>& AxialPath,
	const TArray<FIntPoint>& PickedCollectibleAxials)
{
	AML_PlayerCharacter* MycelandCharacter = Cast<AML_PlayerCharacter>(GetPawn());
	if (!IsValid(MycelandCharacter) || !IsValid(MycelandCharacter->CurrentTileOn)) return;

	AML_BoardSpawner* Board = GetBoardFromCurrentTile(MycelandCharacter);
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