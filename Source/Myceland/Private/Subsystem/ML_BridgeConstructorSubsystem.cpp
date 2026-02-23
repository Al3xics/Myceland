// Copyright Myceland Team, All Rights Reserved.

#include "Subsystem/ML_BridgeConstructorSubsystem.h"

#include "Tiles/ML_BoardSpawner.h"
#include "Tiles/ML_Tile.h"
#include "Core/CoreData.h" // For EML_TileType

const FIntPoint UML_BridgeConstructorSubsystem::AxialDirections[6] =
{
	FIntPoint( 1,  0),
	FIntPoint( 1, -1),
	FIntPoint( 0, -1),
	FIntPoint(-1,  0),
	FIntPoint(-1,  1),
	FIntPoint( 0,  1)
};

bool UML_BridgeConstructorSubsystem::IsTileWalkable(const AML_Tile* Tile) const
{
	if (!IsValid(Tile))
	{
		return false;
	}

	// Your rule: only Dirt and Grass are non-blocking
	const EML_TileType Type = Tile->GetCurrentType();
	const bool bTypeWalkable = (Type == EML_TileType::Dirt || Type == EML_TileType::Grass);

	// Optional safety: if you also use bBlocked as "hard block", keep this.
	// If you truly want to ignore bBlocked and rely only on type, remove the && !Tile->IsBlocked().
	return bTypeWalkable && !Tile->IsBlocked();
}

bool UML_BridgeConstructorSubsystem::CheckPathToExit(const FIntPoint& StartTile, const FIntPoint& ExitTile, const AML_BoardSpawner* BoardSpawner) const
{
	if (!IsValid(BoardSpawner))
	{
		return false;
	}

	// Fast path
	if (StartTile == ExitTile)
	{
		// Still ensure the tile exists and is walkable (or decide to return true no matter what).
		const TMap<FIntPoint, AML_Tile*> Grid = BoardSpawner->GetGridMap();
		if (const AML_Tile* const* Found = Grid.Find(StartTile))
		{
			return IsTileWalkable(*Found);
		}
		return false;
	}

	// NOTE: GetGridMap() returns a COPY in your current code.
	// For large grids, consider adding a const ref getter in BoardSpawner later.
	const TMap<FIntPoint, AML_Tile*> Grid = BoardSpawner->GetGridMap();

	const AML_Tile* const* StartPtr = Grid.Find(StartTile);
	const AML_Tile* const* ExitPtr  = Grid.Find(ExitTile);

	if (!StartPtr || !ExitPtr)
	{
		return false; // start or exit doesn't exist in this grid
	}

	// If you want to allow reaching an Exit even if it's "blocked", you can special-case ExitTile here.
	if (!IsTileWalkable(*StartPtr))
	{
		return false;
	}
	if (!IsTileWalkable(*ExitPtr))
	{
		return false;
	}

	// BFS
	TSet<FIntPoint> Visited;
	Visited.Reserve(Grid.Num());

	TArray<FIntPoint> Queue;
	Queue.Reserve(Grid.Num());

	int32 Head = 0;

	Visited.Add(StartTile);
	Queue.Add(StartTile);

	while (Head < Queue.Num())
	{
		const FIntPoint Current = Queue[Head++];

		for (const FIntPoint& Dir : AxialDirections)
		{
			const FIntPoint Next = Current + Dir;

			if (Visited.Contains(Next))
			{
				continue;
			}

			const AML_Tile* const* NextTilePtr = Grid.Find(Next);
			if (!NextTilePtr)
			{
				continue; // outside grid / no tile
			}

			const AML_Tile* NextTile = *NextTilePtr;
			if (!IsTileWalkable(NextTile))
			{
				continue; // blocked by type (or bBlocked)
			}

			if (Next == ExitTile)
			{
				return true;
			}

			Visited.Add(Next);
			Queue.Add(Next);
		}
	}

	return false;
}