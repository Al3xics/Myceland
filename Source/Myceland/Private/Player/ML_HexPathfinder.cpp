// Copyright Myceland Team, All Rights Reserved.

#include "Player/ML_HexPathfinder.h"

#include "Algo/Reverse.h"
#include "Core/ML_CoreData.h"
#include "Tiles/ML_Tile.h"

bool UML_HexPathfinder::IsTileWalkable(const AML_Tile* Tile)
{
	if (!IsValid(Tile)) return false;

	const EML_TileType Type = Tile->GetCurrentType();
	return (Type == EML_TileType::Dirt || Type == EML_TileType::Grass) && !Tile->IsBlocked();
}

bool UML_HexPathfinder::BuildPath_AxialBFS(const FIntPoint& StartAxial, const FIntPoint& GoalAxial,
                                            const TMap<FIntPoint, AML_Tile*>& GridMap,
                                            TArray<FIntPoint>& OutAxialPath)
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

