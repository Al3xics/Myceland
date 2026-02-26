// Copyright Myceland Team, All Rights Reserved.


#include "Subsystem/ML_WinLoseSubsystem.h"

#include "Core/ML_CoreData.h"
#include "Tiles/ML_TileBase.h"
#include "Tiles/ML_Tile.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Player/ML_PlayerCharacter.h"
#include "Player/ML_PlayerController.h"

FML_GameResult UML_WinLoseSubsystem::CheckWinLose()
{
	if (CurrentBoardSpawner == nullptr)
	{
		CurrentBoardSpawner = FindBoardSpawner();
	}

	const bool bWin = AreAllGoalsConnectedByAllowedPaths(CurrentBoardSpawner, EML_TileType::Tree,
	                                                     {EML_TileType::Grass, EML_TileType::Water});
	//const bool bLose = CheckPlayerKilled(GetPlayerCurrentTile());
	FML_GameResult GameResult;

	if (bIsPlayerDead)
	{
		GameResult.Result = EML_WinLose::Lose;
		GameResult.bIsGameOver = true;
		OnLose.Broadcast();
		GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, TEXT("You Lost!"));
		return GameResult;
	}

	if (bWin)
	{
		GameResult.Result = EML_WinLose::Win;
		GameResult.bIsGameOver = false;
		OnWin.Broadcast();
		GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, TEXT("You Won!"));
	}

	return GameResult;
}

bool UML_WinLoseSubsystem::CheckPlayerKilled(AML_Tile* CurrentTileOn)
{
	if (bIsPlayerDead) return false;

	if (CurrentTileOn->GetCurrentType() == EML_TileType::Water || CurrentTileOn->GetCurrentType() ==
		EML_TileType::Parasite)
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

	if (!IsValid(Board)) return false;

	const TMap<FIntPoint, AML_Tile*> Grid = Board->GetGridMap();
	if (Grid.Num() == 0) return false;

	TSet<EML_TileType> AllowedSet;
	for (EML_TileType T : AllowedPathTypes) AllowedSet.Add(T);

	// Gather goals
	TArray<FIntPoint> GoalAxials;
	for (const auto& Pair : Grid)
	{
		if (Pair.Value && Pair.Value->GetCurrentType() == GoalType)
		{
			GoalAxials.Add(Pair.Key);
		}
	}

	if (GoalAxials.Num() <= 1)
	{
		// 0/1 goal = trivially connected. Keep PathTiles empty or add the goal if you prefer.
		return true;
	}

	auto CanTraverse = [&](AML_Tile* Tile) -> bool
	{
		if (!IsValid(Tile)) return false;
		const EML_TileType Type = Tile->GetCurrentType();
		return (Type == GoalType) || AllowedSet.Contains(Type);
	};

	static const FIntPoint Dirs[6] = {
		FIntPoint(1, 0),
		FIntPoint(1, -1),
		FIntPoint(0, -1),
		FIntPoint(-1, 0),
		FIntPoint(-1, 1),
		FIntPoint(0, 1)
	};

	const FIntPoint Start = GoalAxials[0];

	// BFS state
	TSet<FIntPoint> Visited;
	TQueue<FIntPoint> Queue;

	// Parent links to reconstruct paths (BFS tree)
	TMap<FIntPoint, FIntPoint> Parent; // child -> parent
	Parent.Reserve(Grid.Num());

	Visited.Add(Start);
	Queue.Enqueue(Start);

	// For goal checks
	TSet<FIntPoint> GoalSet(GoalAxials);
	TSet<FIntPoint> ReachedGoalSet;
	ReachedGoalSet.Reserve(GoalAxials.Num());

	while (!Queue.IsEmpty())
	{
		FIntPoint Current;
		Queue.Dequeue(Current);

		if (GoalSet.Contains(Current))
		{
			ReachedGoalSet.Add(Current);
		}

		for (const FIntPoint& Dir : Dirs)
		{
			const FIntPoint Next = Current + Dir;
			if (Visited.Contains(Next)) continue;

			AML_Tile* const* NextPtr = Grid.Find(Next);
			if (!NextPtr) continue;

			AML_Tile* NextTile = *NextPtr;
			if (!CanTraverse(NextTile)) continue;

			Visited.Add(Next);
			Parent.Add(Next, Current);
			Queue.Enqueue(Next);
		}
	}

	// If not all goals reached, fail and clear
	if (ReachedGoalSet.Num() != GoalAxials.Num())
	{
		PathTiles.Reset();
		return false;
	}

	// Reconstruct union of paths from each goal back to Start using Parent
	TSet<FIntPoint> PathAxials;
	PathAxials.Reserve(Grid.Num());

	PathAxials.Add(Start);

	for (const FIntPoint& GoalAxial : GoalAxials)
	{
		FIntPoint Node = GoalAxial;
		PathAxials.Add(Node);

		// Walk back to Start
		while (Node != Start)
		{
			const FIntPoint* P = Parent.Find(Node);
			if (!P)
			{
				// Shouldn't happen if connected, but safety
				PathTiles.Reset();
				return false;
			}
			Node = *P;
			PathAxials.Add(Node);
		}
	}

	// Convert axial set to tile pointers
	PathTiles.Reserve(PathAxials.Num());
	for (const FIntPoint& A : PathAxials)
	{
		if (AML_Tile* const* T = Grid.Find(A))
		{
			PathTiles.Add(*T);
		}
	}

	return true;
}


bool UML_WinLoseSubsystem::BuildConnectedGoalGroups(AML_BoardSpawner* Board,
                                                    EML_TileType GoalType,
                                                    const TSet<EML_TileType>& AllowedSet,
                                                    bool bDisallowBlocked,
                                                    TArray<FML_TileGroup>& OutGroups) const
{
	OutGroups.Reset();

	if (!IsValid(Board)) return false;

	const TMap<FIntPoint, AML_Tile*> Grid = Board->GetGridMap();
	if (Grid.Num() == 0) return false;

	auto CanTraverse = [&](AML_Tile* Tile) -> bool
	{
		if (!IsValid(Tile)) return false;
		if (bDisallowBlocked && Tile->IsBlocked()) return false;

		const EML_TileType Type = Tile->GetCurrentType();
		return (Type == GoalType) || AllowedSet.Contains(Type);
	};

	static const FIntPoint Dirs[6] = {
		FIntPoint(1, 0),
		FIntPoint(1, -1),
		FIntPoint(0, -1),
		FIntPoint(-1, 0),
		FIntPoint(-1, 1),
		FIntPoint(0, 1)
	};

	TSet<FIntPoint> Visited;
	Visited.Reserve(Grid.Num());

	for (const TPair<FIntPoint, AML_Tile*>& Pair : Grid)
	{
		const FIntPoint StartAxial = Pair.Key;

		if (Visited.Contains(StartAxial))
			continue;

		AML_Tile* StartTile = Pair.Value;

		// If not traversable, just mark visited and skip
		if (!CanTraverse(StartTile))
		{
			Visited.Add(StartAxial);
			continue;
		}

		// BFS for this connected component
		TQueue<FIntPoint> Queue;
		Queue.Enqueue(StartAxial);
		Visited.Add(StartAxial);

		FML_TileGroup Group;
		Group.Tiles.Reserve(64);
		Group.Goals.Reserve(8);

		while (!Queue.IsEmpty())
		{
			FIntPoint Current;
			Queue.Dequeue(Current);

			AML_Tile* const* CurPtr = Grid.Find(Current);
			if (!CurPtr || !IsValid(*CurPtr)) continue;

			AML_Tile* CurTile = *CurPtr;
			Group.Tiles.Add(CurTile);

			if (CurTile->GetCurrentType() == GoalType)
			{
				Group.Goals.Add(CurTile);
			}

			for (const FIntPoint& Dir : Dirs)
			{
				const FIntPoint Next = Current + Dir;
				if (Visited.Contains(Next)) continue;

				AML_Tile* const* NextPtr = Grid.Find(Next);
				if (!NextPtr)
				{
					Visited.Add(Next);
					continue;
				}

				AML_Tile* NextTile = *NextPtr;
				if (!CanTraverse(NextTile))
				{
					Visited.Add(Next);
					continue;
				}

				Visited.Add(Next);
				Queue.Enqueue(Next);
			}
		}

		OutGroups.Add(MoveTemp(Group));
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
	if (!IsValid(Board)) return false;

	const TMap<FIntPoint, AML_Tile*> Grid = Board->GetGridMap();
	if (Grid.Num() == 0) return false;

	// Build allowed set
	TSet<EML_TileType> AllowedSet;
	AllowedSet.Reserve(AllowedPathTypes.Num());
	for (EML_TileType T : AllowedPathTypes)
	{
		AllowedSet.Add(T);
	}

	auto CanTraverse = [&](AML_Tile* Tile) -> bool
	{
		if (!IsValid(Tile)) return false;
		if (bDisallowBlocked && Tile->IsBlocked()) return false;

		const EML_TileType Type = Tile->GetCurrentType();
		return (Type == GoalType) || AllowedSet.Contains(Type);
	};

	static const FIntPoint Dirs[6] = {
		FIntPoint( 1,  0),
		FIntPoint( 1, -1),
		FIntPoint( 0, -1),
		FIntPoint(-1,  0),
		FIntPoint(-1,  1),
		FIntPoint( 0,  1)
	};

	// Gather goals
	TArray<FIntPoint> GoalAxials;
	GoalAxials.Reserve(32);

	for (const auto& Pair : Grid)
	{
		if (Pair.Value && Pair.Value->GetCurrentType() == GoalType)
		{
			if (!bDisallowBlocked || !Pair.Value->IsBlocked())
			{
				GoalAxials.Add(Pair.Key);
			}
		}
	}

	// If you pass MinGoalsInGroup, for "path between 2 goals" it only makes sense as >= 2.
	const int32 RequiredGoalsPerPath = FMath::Max(2, MinGoalsInGroup);
	if (GoalAxials.Num() < RequiredGoalsPerPath) return false;

	// For faster membership testing
	TSet<FIntPoint> GoalSet;
	GoalSet.Reserve(GoalAxials.Num());
	for (const FIntPoint& A : GoalAxials) GoalSet.Add(A);

	// Helper to reconstruct path from Parent map (Target -> Start)
	auto ReconstructPath = [&](const FIntPoint& Start, const FIntPoint& Target, const TMap<FIntPoint, FIntPoint>& Parent, TArray<AML_Tile*>& OutTiles) -> bool
	{
		OutTiles.Reset();

		// Walk backwards Target -> Start
		TArray<FIntPoint> Axials;
		FIntPoint Node = Target;
		Axials.Add(Node);

		while (Node != Start)
		{
			const FIntPoint* P = Parent.Find(Node);
			if (!P) return false;
			Node = *P;
			Axials.Add(Node);
		}

		Algo::Reverse(Axials);

		OutTiles.Reserve(Axials.Num());
		for (const FIntPoint& Ax : Axials)
		{
			if (AML_Tile* const* T = Grid.Find(Ax))
			{
				OutTiles.Add(*T);
			}
		}

		return OutTiles.Num() > 0;
	};

	// Generate one shortest path group for each connected pair (i < j)
	for (int32 i = 0; i < GoalAxials.Num(); ++i)
	{
		const FIntPoint Start = GoalAxials[i];

		// BFS from this goal
		TQueue<FIntPoint> Queue;
		TSet<FIntPoint> Visited;
		TMap<FIntPoint, FIntPoint> Parent;

		Visited.Reserve(Grid.Num());
		Parent.Reserve(Grid.Num());

		Visited.Add(Start);
		Queue.Enqueue(Start);

		while (!Queue.IsEmpty())
		{
			FIntPoint Current;
			Queue.Dequeue(Current);

			for (const FIntPoint& Dir : Dirs)
			{
				const FIntPoint Next = Current + Dir;
				if (Visited.Contains(Next)) continue;

				AML_Tile* const* NextPtr = Grid.Find(Next);
				if (!NextPtr) continue;

				AML_Tile* NextTile = *NextPtr;
				if (!CanTraverse(NextTile)) continue;

				Visited.Add(Next);
				Parent.Add(Next, Current);
				Queue.Enqueue(Next);
			}
		}

		// For every other goal j > i, if reachable, add a distinct path group
		for (int32 j = i + 1; j < GoalAxials.Num(); ++j)
		{
			const FIntPoint Target = GoalAxials[j];
			if (Target == Start) continue;

			// If unreachable, Parent won't contain it (unless it's Start)
			if (!Parent.Contains(Target)) continue;

			TArray<AML_Tile*> Path;
			if (!ReconstructPath(Start, Target, Parent, Path)) continue;

			// Build one group = one path between 2 goals
			FML_TileGroup Group;
			Group.Tiles = MoveTemp(Path);

			AML_Tile* GoalA = Grid.FindRef(Start);
			AML_Tile* GoalB = Grid.FindRef(Target);

			if (IsValid(GoalA)) Group.Goals.Add(GoalA);
			if (IsValid(GoalB)) Group.Goals.Add(GoalB);

			// Enforce RequiredGoalsPerPath (usually 2)
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
	bool bHasConnectedGoals = FindConnectedGoalGroups(CurrentBoardSpawner,EML_TileType::Tree,{EML_TileType::Grass, EML_TileType::Water}, false, 2);
	return ConnectedGoalGroups;
}

AML_Tile* UML_WinLoseSubsystem::GetPlayerCurrentTile() const
{
	UWorld* World = GetWorld();
	if (!World) return nullptr;

	AML_PlayerCharacter* PlayerCharacter = Cast<AML_PlayerCharacter>(UGameplayStatics::GetPlayerCharacter(World, 0));
	if (!PlayerCharacter) return nullptr;

	AML_Tile* PlayerCurrentTile = PlayerCharacter->CurrentTileOn;
	if (!IsValid(PlayerCurrentTile)) return nullptr;
	return PlayerCurrentTile;
}

AML_BoardSpawner* UML_WinLoseSubsystem::FindBoardSpawner() const
{
	AML_Tile* ChildTile = GetPlayerCurrentTile();

	AActor* BoardActor = ChildTile->GetAttachParentActor();
	if (!BoardActor) return nullptr;

	AML_BoardSpawner* RetrivedBoardSpawner = Cast<AML_BoardSpawner>(BoardActor);
	if (!RetrivedBoardSpawner) return nullptr;

	UE_LOG(LogTemp, Log, TEXT("Board found: %s at %s"),
	       *RetrivedBoardSpawner->GetName(),
	       *RetrivedBoardSpawner->GetActorLocation().ToString());

	return RetrivedBoardSpawner;
}

TArray<AML_Tile*> UML_WinLoseSubsystem::GetConnectedPathStruct()
{
	return PathTiles;
}
