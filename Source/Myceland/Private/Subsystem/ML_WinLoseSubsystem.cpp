// Copyright Myceland Team, All Rights Reserved.


#include "Subsystem/ML_WinLoseSubsystem.h"

#include "Core/ML_CoreData.h"
#include "Tiles/ML_TileBase.h"
#include "Tiles/ML_Tile.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Player/ML_PlayerCharacter.h"

FML_GameResult UML_WinLoseSubsystem::CheckWinLose()
{
	if (CurrentBoardSpawner == nullptr)
	{
		CurrentBoardSpawner = FindBoardSpawner();
	}

	const bool bWin = AreAllGoalsConnectedByAllowedPaths(CurrentBoardSpawner, EML_TileType::Tree,
	                                                     {EML_TileType::Grass, EML_TileType::Water});
	const bool bLose = CheckPlayerKilled(GetPlayerCurrentTile());
	FML_GameResult GameResult;

	if (bWin)
	{
		GameResult.Result = EML_WinLose::Win;
		GameResult.bIsGameOver = false;
		OnWin.Broadcast();
		GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, TEXT("You Won!"));	

	}
	if (bLose)
	{
		GameResult.Result = EML_WinLose::Lose;
		GameResult.bIsGameOver = true;
		OnLose.Broadcast();
		GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, TEXT("You Lost!"));	
	}
	
	return GameResult;
}

bool UML_WinLoseSubsystem::CheckPlayerKilled(AML_Tile* CurrentTileOn)
{
	return false;
}

bool UML_WinLoseSubsystem::AreAllGoalsConnectedByAllowedPaths(AML_BoardSpawner* Board,
                                                              EML_TileType GoalType,
                                                              const TArray<EML_TileType>& AllowedPathTypes) const
{
	if (!IsValid(Board)) return false;

	const TMap<FIntPoint, AML_Tile*> Grid = Board->GetGridMap();
	if (Grid.Num() == 0) return false;

	// Build a fast lookup set of allowed path types
	TSet<EML_TileType> AllowedSet;
	AllowedSet.Reserve(AllowedPathTypes.Num());
	for (EML_TileType T : AllowedPathTypes)
	{
		AllowedSet.Add(T);
	}

	// Gather all goal tiles
	TArray<FIntPoint> GoalAxials;
	GoalAxials.Reserve(16);

	for (const TPair<FIntPoint, AML_Tile*>& Pair : Grid)
	{
		AML_Tile* Tile = Pair.Value;
		if (!IsValid(Tile)) continue;

		if (Tile->GetCurrentType() == GoalType)
		{
			GoalAxials.Add(Pair.Key);
		}
	}

	auto CanTraverse = [&](AML_Tile* Tile) -> bool
	{
		if (!IsValid(Tile)) return false;

		const EML_TileType Type = Tile->GetCurrentType();
		return (Type == GoalType) || AllowedSet.Contains(Type);
	};

	// BFS from first goal
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

	TQueue<FIntPoint> Queue;

	const FIntPoint Start = GoalAxials[0];
	Queue.Enqueue(Start);
	Visited.Add(Start);

	int32 ReachedGoals = 0;

	// Optional: quick membership for goal counting
	TSet<FIntPoint> GoalSet;
	GoalSet.Reserve(GoalAxials.Num());
	for (const FIntPoint& A : GoalAxials) GoalSet.Add(A);

	while (!Queue.IsEmpty())
	{
		FIntPoint Current;
		Queue.Dequeue(Current);

		// Count goal tiles reached
		if (GoalSet.Contains(Current))
		{
			++ReachedGoals;
			// early out if all reached
			if (ReachedGoals == GoalAxials.Num())
			{
				return true;
			}
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
			Queue.Enqueue(Next);
		}
	}

	return false;
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
