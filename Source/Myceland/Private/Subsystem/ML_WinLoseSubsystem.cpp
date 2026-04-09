// Copyright Myceland Team, All Rights Reserved.

#include "Subsystem/ML_WinLoseSubsystem.h"

#include "Algo/Reverse.h"
#include "Core/ML_CoreData.h"
#include "Kismet/GameplayStatics.h"
#include "Player/ML_PlayerCharacter.h"
#include "Tiles/ML_Tile.h"
#include "Tiles/ML_TileBase.h"
#include "Tiles/TileBase/ML_TileGrass.h"

const FIntPoint UML_WinLoseSubsystem::HexDirs[6] = {
	FIntPoint(1, 0),
	FIntPoint(1, -1),
	FIntPoint(0, -1),
	FIntPoint(-1, 0),
	FIntPoint(-1, 1),
	FIntPoint(0, 1)
};

FML_GameResult UML_WinLoseSubsystem::CheckWinLose()
{
	if (CurrentBoardSpawner == nullptr)
	{
		CurrentBoardSpawner = FindBoardSpawner();
	}

	FML_GameResult NoResult;
	if (CurrentBoardSpawner->bIsPuzzleSolved == true) return NoResult;
	
	const bool bWin = AreAllGoalsConnectedByAllowedPaths(
		CurrentBoardSpawner,
		EML_TileType::Tree,
		{EML_TileType::Grass, EML_TileType::Water});

	FML_GameResult GameResult;

	if (bIsPlayerDead)
	{
		GameResult.Result = EML_WinLose::Lose;
		GameResult.bIsGameOver = true;
		OnLose.Broadcast();
		GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, TEXT("You Lost!"));
		bIsPlayerDead = false;
		return GameResult;
	}

	if (bWin)
	{
		GameResult.Result = EML_WinLose::Win;
		GameResult.bIsGameOver = false;
		CurrentBoardSpawner->bIsPuzzleSolved = true;
		OnWin.Broadcast();

		ClearWinPath(
			CurrentBoardSpawner,
			GetPlayerCurrentTile(),
			CurrentBoardSpawner->ExitTile,
			{EML_TileType::Grass, EML_TileType::Water, EML_TileType::Dirt});

		GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, TEXT("You Won!"));
	}
	
	return GameResult;
}

bool UML_WinLoseSubsystem::CheckPlayerKilled(AML_Tile* CurrentTileOn)
{
	if (bIsPlayerDead || !IsValid(CurrentTileOn))
	{
		return false;
	}

	const EML_TileType TileType = CurrentTileOn->GetCurrentType();
	if (TileType == EML_TileType::Water || TileType == EML_TileType::Parasite)
	{
		GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, TEXT("You died"));
		bIsPlayerDead = true;
		OnDeath.Broadcast();
		return true;
	}

	return false;
}

bool UML_WinLoseSubsystem::AreAllGoalsConnectedByAllowedPaths(
	AML_BoardSpawner* Board,
	EML_TileType GoalType,
	const TArray<EML_TileType>& AllowedPathTypes)
{
	PathTiles.Reset();

	if (!IsValid(Board))
	{
		return false;
	}

	const TMap<FIntPoint, AML_Tile*> Grid = Board->GetGridMap();
	if (Grid.Num() == 0)
	{
		return false;
	}

	const TSet<EML_TileType> AllowedSet = BuildAllowedSet(AllowedPathTypes);
	const TArray<FIntPoint> GoalAxials = CollectGoalAxials(Grid, GoalType);

	if (GoalAxials.Num() <= 1)
	{
		return true;
	}

	auto CanTraverse = [&](AML_Tile* Tile) -> bool
	{
		if (!IsValid(Tile))
		{
			return false;
		}

		const EML_TileType TileType = Tile->GetCurrentType();
		return TileType == GoalType || AllowedSet.Contains(TileType);
	};

	const FIntPoint Start = GoalAxials[0];

	TSet<FIntPoint> Visited;
	TMap<FIntPoint, FIntPoint> Parent;
	RunBFS(Grid, Start, CanTraverse, Visited, Parent);

	TSet<FIntPoint> PathAxials;
	PathAxials.Reserve(Grid.Num());
	PathAxials.Add(Start);

	for (const FIntPoint& GoalAxial : GoalAxials)
	{
		if (!Visited.Contains(GoalAxial))
		{
			PathTiles.Reset();
			return false;
		}

		FIntPoint Node = GoalAxial;
		PathAxials.Add(Node);

		while (Node != Start)
		{
			const FIntPoint* Prev = Parent.Find(Node);
			if (!Prev)
			{
				PathTiles.Reset();
				return false;
			}

			Node = *Prev;
			PathAxials.Add(Node);
		}
	}

	PathTiles.Reset();
	PathTiles.Reserve(PathAxials.Num());

	for (const FIntPoint& Axial : PathAxials)
	{
		if (AML_Tile* const* TilePtr = Grid.Find(Axial))
		{
			PathTiles.Add(*TilePtr);
		}
	}

	return true;
}

bool UML_WinLoseSubsystem::FindConnectedGoalGroups(
	AML_BoardSpawner* Board,
	EML_TileType GoalType,
	const TArray<EML_TileType>& AllowedPathTypes,
	bool bDisallowBlocked,
	int32 MinGoalsInGroup)
{
	ConnectedGoalGroups.Reset();

	if (!IsValid(Board))
	{
		return false;
	}

	const TMap<FIntPoint, AML_Tile*> Grid = Board->GetGridMap();
	if (Grid.Num() == 0)
	{
		return false;
	}

	const TSet<EML_TileType> AllowedSet = BuildAllowedSet(AllowedPathTypes);
	const TArray<FIntPoint> GoalAxials = CollectGoalAxials(Grid, GoalType, bDisallowBlocked);

	const int32 RequiredGoalsPerPath = FMath::Max(2, MinGoalsInGroup);
	if (GoalAxials.Num() < RequiredGoalsPerPath)
	{
		return false;
	}

	auto CanTraverse = [&](AML_Tile* Tile) -> bool
	{
		if (!IsValid(Tile))
		{
			return false;
		}

		if (bDisallowBlocked && Tile->IsBlocked())
		{
			return false;
		}

		const EML_TileType TileType = Tile->GetCurrentType();
		return TileType == GoalType || AllowedSet.Contains(TileType);
	};

	for (int32 i = 0; i < GoalAxials.Num(); ++i)
	{
		const FIntPoint Start = GoalAxials[i];

		TSet<FIntPoint> Visited;
		TMap<FIntPoint, FIntPoint> Parent;
		RunBFS(Grid, Start, CanTraverse, Visited, Parent);

		for (int32 j = i + 1; j < GoalAxials.Num(); ++j)
		{
			const FIntPoint Target = GoalAxials[j];
			if (!Visited.Contains(Target))
			{
				continue;
			}

			TArray<FIntPoint> PathAxials;
			if (!BuildPathAxialsFromParent(Start, Target, Parent, PathAxials))
			{
				continue;
			}

			TArray<AML_Tile*> PathTilesLocal;
			if (!ConvertAxialsToTiles(Grid, PathAxials, PathTilesLocal))
			{
				continue;
			}

			FML_TileGroup Group;
			Group.Tiles = MoveTemp(PathTilesLocal);

			if (AML_Tile* GoalA = Grid.FindRef(Start); IsValid(GoalA))
			{
				Group.Goals.Add(GoalA);
			}

			if (AML_Tile* GoalB = Grid.FindRef(Target); IsValid(GoalB))
			{
				Group.Goals.Add(GoalB);
			}

			if (Group.Goals.Num() >= RequiredGoalsPerPath)
			{
				ConnectedGoalGroups.Add(MoveTemp(Group));
			}
		}
	}

	return ConnectedGoalGroups.Num() > 0;
}

TArray<FML_TileGroup> UML_WinLoseSubsystem::TriggerFindConnectedGoalCheck()
{
	FindConnectedGoalGroups(
		CurrentBoardSpawner,
		EML_TileType::Tree,
		{EML_TileType::Grass, EML_TileType::Water},
		false,
		2);

	return ConnectedGoalGroups;
}

AML_Tile* UML_WinLoseSubsystem::GetPlayerCurrentTile() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	AML_PlayerCharacter* PlayerCharacter = Cast<AML_PlayerCharacter>(
		UGameplayStatics::GetPlayerCharacter(World, 0));

	if (!PlayerCharacter)
	{
		return nullptr;
	}

	AML_Tile* PlayerCurrentTile = PlayerCharacter->CurrentTileOn;
	return IsValid(PlayerCurrentTile) ? PlayerCurrentTile : nullptr;
}

void UML_WinLoseSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	AML_PlayerCharacter* Player = Cast<AML_PlayerCharacter>(
		UGameplayStatics::GetPlayerCharacter(&InWorld, 0));

	if (Player)
	{
		BoundPlayer = Player;
		Player->OnBoardChanged.AddDynamic(this, &UML_WinLoseSubsystem::HandleBoardChanged);
	}
}

void UML_WinLoseSubsystem::HandleBoardChanged(const AML_Tile* NewTile)
{
	if (!IsValid(NewTile))
	{
		CurrentBoardSpawner = nullptr;
		return;
	}

	CurrentBoardSpawner = FindBoardSpawner();
}

AML_BoardSpawner* UML_WinLoseSubsystem::FindBoardSpawner() const
{
	AML_Tile* ChildTile = GetPlayerCurrentTile();
	if (!IsValid(ChildTile))
	{
		return nullptr;
	}

	AActor* BoardActor = ChildTile->GetAttachParentActor();
	if (!BoardActor)
	{
		return nullptr;
	}

	AML_BoardSpawner* RetrievedBoardSpawner = Cast<AML_BoardSpawner>(BoardActor);
	if (!RetrievedBoardSpawner)
	{
		return nullptr;
	}

	UE_LOG(LogTemp, Log, TEXT("Board found: %s at %s"),
		*RetrievedBoardSpawner->GetName(),
		*RetrievedBoardSpawner->GetActorLocation().ToString());

	return RetrievedBoardSpawner;
}

void UML_WinLoseSubsystem::ClearWinPath(
	const AML_BoardSpawner* Board,
	const AML_Tile* StartTile,
	const AML_Tile* GoalTile,
	const TArray<EML_TileType>& AllowedPathTypes) const
{
	if (!IsValid(Board) || !IsValid(StartTile) || !IsValid(GoalTile))
	{
		return;
	}

	const TMap<FIntPoint, AML_Tile*> Grid = Board->GetGridMap();
	if (Grid.Num() == 0)
	{
		return;
	}

	const TSet<EML_TileType> AllowedSet = BuildAllowedSet(AllowedPathTypes);

	const FIntPoint Start = StartTile->GetAxialCoord();
	const FIntPoint Goal = GoalTile->GetAxialCoord();

	auto CanTraverse = [&](AML_Tile* Tile) -> bool
	{
		if (!IsValid(Tile))
		{
			return false;
		}

		return Tile == StartTile
			|| Tile == GoalTile
			|| AllowedSet.Contains(Tile->GetCurrentType());
	};

	TSet<FIntPoint> Visited;
	TMap<FIntPoint, FIntPoint> Parent;
	RunBFS(Grid, Start, CanTraverse, Visited, Parent);

	if (Start != Goal && !Visited.Contains(Goal))
	{
		return;
	}

	TArray<FIntPoint> PathAxials;
	if (Start == Goal)
	{
		PathAxials.Add(Start);
	}
	else if (!BuildPathAxialsFromParent(Start, Goal, Parent, PathAxials))
	{
		return;
	}

	for (const FIntPoint& Axial : PathAxials)
	{
		AML_Tile* const* TilePtr = Grid.Find(Axial);
		if (!TilePtr || !IsValid(*TilePtr))
		{
			continue;
		}

		if ((*TilePtr)->GetCurrentType() == EML_TileType::Water)
		{
			(*TilePtr)->UpdateClassAtRuntime(EML_TileType::Grass, CurrentBoardSpawner->WaterChangeTile);
		}
	}
}

TSet<EML_TileType> UML_WinLoseSubsystem::BuildAllowedSet(
	const TArray<EML_TileType>& AllowedPathTypes) const
{
	TSet<EML_TileType> AllowedSet;
	AllowedSet.Reserve(AllowedPathTypes.Num());

	for (const EML_TileType Type : AllowedPathTypes)
	{
		AllowedSet.Add(Type);
	}

	return AllowedSet;
}

TArray<FIntPoint> UML_WinLoseSubsystem::CollectGoalAxials(
	const TMap<FIntPoint, AML_Tile*>& Grid,
	EML_TileType GoalType,
	bool bDisallowBlocked) const
{
	TArray<FIntPoint> GoalAxials;

	for (const auto& Pair : Grid)
	{
		AML_Tile* Tile = Pair.Value;
		if (!IsValid(Tile))
		{
			continue;
		}

		if (Tile->GetCurrentType() != GoalType)
		{
			continue;
		}

		if (bDisallowBlocked && Tile->IsBlocked())
		{
			continue;
		}

		GoalAxials.Add(Pair.Key);
	}

	return GoalAxials;
}

void UML_WinLoseSubsystem::RunBFS(
	const TMap<FIntPoint, AML_Tile*>& Grid,
	const FIntPoint& Start,
	TFunctionRef<bool(AML_Tile*)> CanTraverse,
	TSet<FIntPoint>& OutVisited,
	TMap<FIntPoint, FIntPoint>& OutParent) const
{
	OutVisited.Reset();
	OutParent.Reset();

	TQueue<FIntPoint> Queue;

	OutVisited.Reserve(Grid.Num());
	OutParent.Reserve(Grid.Num());

	OutVisited.Add(Start);
	Queue.Enqueue(Start);

	while (!Queue.IsEmpty())
	{
		FIntPoint Current;
		Queue.Dequeue(Current);

		for (const FIntPoint& Dir : HexDirs)
		{
			const FIntPoint Next = Current + Dir;
			if (OutVisited.Contains(Next))
			{
				continue;
			}

			AML_Tile* const* NextPtr = Grid.Find(Next);
			if (!NextPtr)
			{
				continue;
			}

			AML_Tile* NextTile = *NextPtr;
			if (!CanTraverse(NextTile))
			{
				continue;
			}

			OutVisited.Add(Next);
			OutParent.Add(Next, Current);
			Queue.Enqueue(Next);
		}
	}
}

bool UML_WinLoseSubsystem::BuildPathAxialsFromParent(
	const FIntPoint& Start,
	const FIntPoint& Target,
	const TMap<FIntPoint, FIntPoint>& Parent,
	TArray<FIntPoint>& OutAxials) const
{
	OutAxials.Reset();

	FIntPoint Node = Target;
	OutAxials.Add(Node);

	while (Node != Start)
	{
		const FIntPoint* Prev = Parent.Find(Node);
		if (!Prev)
		{
			return false;
		}

		Node = *Prev;
		OutAxials.Add(Node);
	}

	Algo::Reverse(OutAxials);
	return true;
}

bool UML_WinLoseSubsystem::ConvertAxialsToTiles(
	const TMap<FIntPoint, AML_Tile*>& Grid,
	const TArray<FIntPoint>& Axials,
	TArray<AML_Tile*>& OutTiles) const
{
	OutTiles.Reset();
	OutTiles.Reserve(Axials.Num());

	for (const FIntPoint& Axial : Axials)
	{
		if (AML_Tile* const* TilePtr = Grid.Find(Axial))
		{
			OutTiles.Add(*TilePtr);
		}
	}

	return OutTiles.Num() > 0;
}