#include "MyceliumPlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"

#include "MyceliumCharacter.h"
#include "Tiles/ML_Tile.h"
#include "Tiles/ML_BoardSpawner.h"
#include "Core/CoreData.h" // EML_TileType

const FIntPoint AMyceliumPlayerController::AxialDirections[6] =
{
	FIntPoint( 1,  0),
	FIntPoint( 1, -1),
	FIntPoint( 0, -1),
	FIntPoint(-1,  0),
	FIntPoint(-1,  1),
	FIntPoint( 0,  1)
};

AMyceliumPlayerController::AMyceliumPlayerController()
{
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
}

void AMyceliumPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Add IMC from editor
	if (DefaultIMC)
	{
		if (ULocalPlayer* LP = GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsys = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				Subsys->AddMappingContext(DefaultIMC, DefaultIMCPriority);
			}
		}
	}
}

void AMyceliumPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EIC)
	{
		return;
	}

	if (IA_SetDestination_Click)
	{
		EIC->BindAction(IA_SetDestination_Click, ETriggerEvent::Started, this, &AMyceliumPlayerController::OnSetDestinationTriggered);
	}
}

void AMyceliumPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	TickMoveAlongPath(DeltaTime);
}

AML_BoardSpawner* AMyceliumPlayerController::GetBoardFromCurrentTile(const AMyceliumCharacter* MyChar) const
{
	if (!IsValid(MyChar) || !IsValid(MyChar->CurrentTileOn))
	{
		return nullptr;
	}

	return Cast<AML_BoardSpawner>(MyChar->CurrentTileOn->GetOwner());
}

AML_Tile* AMyceliumPlayerController::GetTileUnderCursor() const
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

bool AMyceliumPlayerController::IsTileWalkable(const AML_Tile* Tile) const
{
	if (!IsValid(Tile))
	{
		return false;
	}

	const EML_TileType Type = Tile->GetCurrentType();
	const bool bTypeWalkable = (Type == EML_TileType::Dirt || Type == EML_TileType::Grass);

	return bTypeWalkable && !Tile->IsBlocked();
}

#define ML_LOG(Format, ...) UE_LOG(LogTemp, Warning, TEXT("[MyceliumMove] " Format), ##__VA_ARGS__)

void AMyceliumPlayerController::OnSetDestinationTriggered(const FInputActionValue& Value)
{
	ML_LOG("Click input received");

	AMyceliumCharacter* MyChar = Cast<AMyceliumCharacter>(GetPawn());
	if (!IsValid(MyChar))
	{
		ML_LOG("No pawn / not MyceliumCharacter");
		return;
	}

	if (!IsValid(MyChar->CurrentTileOn))
	{
		ML_LOG("CurrentTileOn is NULL -> cannot start path");
		return;
	}

	AML_BoardSpawner* Board = GetBoardFromCurrentTile(MyChar);
	if (!IsValid(Board))
	{
		ML_LOG("Board is NULL (owner cast failed)");
		return;
	}

	AML_Tile* TargetTile = GetTileUnderCursor();
	if (!IsValid(TargetTile))
	{
		ML_LOG("No tile under cursor (hit failed or wrong collision)");
		return;
	}

	ML_LOG("StartTile: %s  TargetTile: %s", *MyChar->CurrentTileOn->GetName(), *TargetTile->GetName());

	if (TargetTile->GetOwner() != Board)
	{
		ML_LOG("Target tile is not on same board");
		return;
	}

	const FIntPoint StartAxial = MyChar->CurrentTileOn->GetAxialCoord();
	const FIntPoint GoalAxial  = TargetTile->GetAxialCoord();

	const TMap<FIntPoint, AML_Tile*> GridMap = Board->GetGridMap();
	ML_LOG("GridMap size: %d", GridMap.Num());

	if (!GridMap.Contains(StartAxial) || !GridMap.Contains(GoalAxial))
	{
		ML_LOG("Start or Goal axial not in GridMap");
		return;
	}

	if (!IsTileWalkable(GridMap[StartAxial]) || !IsTileWalkable(GridMap[GoalAxial]))
	{
		ML_LOG("Start or Goal is not walkable");
		return;
	}

	TArray<FIntPoint> AxialPath;
	if (!BuildPath_AxialBFS(StartAxial, GoalAxial, GridMap, AxialPath))
	{
		ML_LOG("No path found");
		return;
	}

	ML_LOG("Path found: %d tiles", AxialPath.Num());
	StartMoveAlongPath(AxialPath, GridMap);
}

bool AMyceliumPlayerController::BuildPath_AxialBFS(
	const FIntPoint& StartAxial,
	const FIntPoint& GoalAxial,
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

		for (const FIntPoint& Dir : AxialDirections)
		{
			const FIntPoint Next = Current + Dir;

			if (CameFrom.Contains(Next))
			{
				continue;
			}

			const AML_Tile* const* NextTilePtr = GridMap.Find(Next);
			if (!NextTilePtr)
			{
				continue;
			}

			if (!IsTileWalkable(*NextTilePtr))
			{
				continue;
			}

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

void AMyceliumPlayerController::StartMoveAlongPath(const TArray<FIntPoint>& AxialPath, const TMap<FIntPoint, AML_Tile*>& GridMap)
{
	CurrentPathWorld.Reset();
	CurrentPathIndex = 0;

	CurrentPathWorld.Reserve(AxialPath.Num());

	for (const FIntPoint& Axial : AxialPath)
	{
		if (AML_Tile* const* TilePtr = GridMap.Find(Axial))
		{
			if (IsValid(*TilePtr))
			{
				CurrentPathWorld.Add((*TilePtr)->GetActorLocation());
			}
		}
	}

	// Skip first point if already basically there
	if (APawn* P = GetPawn())
	{
		if (CurrentPathWorld.Num() > 0)
		{
			if (FVector::DistSquared2D(P->GetActorLocation(), CurrentPathWorld[0]) <= FMath::Square(AcceptanceRadius))
			{
				CurrentPathIndex = 1;
			}
		}
	}
}

void AMyceliumPlayerController::TickMoveAlongPath(float DeltaTime)
{
	if (CurrentPathWorld.Num() == 0 || CurrentPathIndex >= CurrentPathWorld.Num())
	{
		return;
	}

	AMyceliumCharacter* MyChar = Cast<AMyceliumCharacter>(GetPawn());
	if (!IsValid(MyChar))
	{
		return;
	}

	const FVector Loc = MyChar->GetActorLocation();

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
			{
				CurrentPathIndex++; // skip P1, go directly toward P2
			}
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
	MyChar->AddMovementInput(To, MoveSpeedScale);
}