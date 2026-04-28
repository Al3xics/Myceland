#include "PuzzleGeneration/ML_PuzzleSimulator.h"
#include "Core/ML_CoreData.h"
#include "Core/ML_TileTypeTraits.h"

bool FML_PuzzleSimulator::SimulatePlantAction(FML_PuzzleState& State, const FIntPoint& PlantAxial)
{
	FML_PuzzleCell* Cell = State.FindCellMutable(PlantAxial);
	if (!Cell || Cell->Type != EML_TileType::Dirt)
	{
		return false;
	}

	bool bHadChanges = true;
	while (bHadChanges)
	{
		bHadChanges = false;
		bHadChanges |= SimulateGrassWave(State, PlantAxial);
		bHadChanges |= SimulateWaterWave(State);
		bHadChanges |= SimulateParasiteWave(State);
	}

	return true;
}

bool FML_PuzzleSimulator::IsWinningState(const FML_PuzzleState& State)
{
	TArray<FIntPoint> TreeAxials;

	for (const FML_PuzzleCell& Cell : State.Cells)
	{
		if (Cell.Type == EML_TileType::Tree)
		{
			TreeAxials.Add(Cell.Axial);
		}
	}

	if (TreeAxials.Num() <= 1)
	{
		return true;
	}

	TSet<FIntPoint> TreeSet(TreeAxials);
	TSet<FIntPoint> Visited;
	TQueue<FIntPoint> Queue;

	Visited.Add(TreeAxials[0]);
	Queue.Enqueue(TreeAxials[0]);

	while (!Queue.IsEmpty())
	{
		FIntPoint Current;
		Queue.Dequeue(Current);

		TArray<FIntPoint> Neighbors;
		GetNeighbors(State, Current, Neighbors);

		for (const FIntPoint& NeighborAxial : Neighbors)
		{
			if (Visited.Contains(NeighborAxial))
			{
				continue;
			}

			const FML_PuzzleCell* NeighborCell = State.FindCell(NeighborAxial);
			if (!NeighborCell || !(UML_TileTypeTraits::IsTreeType(NeighborCell->Type) || UML_TileTypeTraits::IsWinPathTile(NeighborCell->Type)))
			{
				continue;
			}

			Visited.Add(NeighborAxial);
			Queue.Enqueue(NeighborAxial);
		}
	}

	for (const FIntPoint& TreeAxial : TreeAxials)
	{
		if (!Visited.Contains(TreeAxial))
		{
			return false;
		}
	}

	return true;
}

void FML_PuzzleSimulator::GetNeighbors(
	const FML_PuzzleState& State,
	const FIntPoint& Axial,
	TArray<FIntPoint>& OutNeighbors)
{
	OutNeighbors.Reset();
	OutNeighbors.Reserve(6);

	for (const FIntPoint& Direction : Directions)
	{
		const FIntPoint NeighborAxial = Axial + Direction;
		if (State.FindCell(NeighborAxial))
		{
			OutNeighbors.Add(NeighborAxial);
		}
	}
}

bool FML_PuzzleSimulator::SimulateGrassWave(FML_PuzzleState& State, const FIntPoint& OriginAxial)
{
	const FML_PuzzleCell* OriginCell = State.FindCell(OriginAxial);
	if (!OriginCell)
	{
		return false;
	}

	TSet<FIntPoint> Scheduled;
	TSet<FIntPoint> WaterConnected;
	TArray<FIntPoint> GrassSources;
	bool bChanged = false;

	if (OriginCell->Type == EML_TileType::Dirt)
	{
		FML_PuzzleCell* MutableOriginCell = State.FindCellMutable(OriginAxial);
		if (!MutableOriginCell)
		{
			return false;
		}

		MutableOriginCell->Type = EML_TileType::Grass;
		bChanged = true;
		GrassSources.Add(OriginAxial);
		Scheduled.Add(OriginAxial);

		ExpandWaterNetwork(State, OriginAxial, WaterConnected);
	}
	else
	{
		for (const FML_PuzzleCell& Cell : State.Cells)
		{
			if (Cell.Type == EML_TileType::Grass)
			{
				GrassSources.Add(Cell.Axial);
				Scheduled.Add(Cell.Axial);
			}
		}

		if (GrassSources.Num() == 0)
		{
			return false;
		}

		for (const FIntPoint& GrassSource : GrassSources)
		{
			ExpandWaterNetwork(State, GrassSource, WaterConnected);
		}
	}

	TQueue<FIntPoint> PropagationQueue;

	for (const FIntPoint& WaterAxial : WaterConnected)
	{
		TArray<FIntPoint> Neighbors;
		GetNeighbors(State, WaterAxial, Neighbors);

		for (const FIntPoint& NeighborAxial : Neighbors)
		{
			if (Scheduled.Contains(NeighborAxial))
			{
				continue;
			}

			const FML_PuzzleCell* NeighborCell = State.FindCell(NeighborAxial);
			if (!NeighborCell || !UML_TileTypeTraits::CanGrassPropagateTo(NeighborCell->Type))
			{
				continue;
			}

			Scheduled.Add(NeighborAxial);
			PropagationQueue.Enqueue(NeighborAxial);
		}
	}

	while (!PropagationQueue.IsEmpty())
	{
		FIntPoint CurrentAxial;
		PropagationQueue.Dequeue(CurrentAxial);

		FML_PuzzleCell* CurrentCell = State.FindCellMutable(CurrentAxial);
		if (!CurrentCell)
		{
			continue;
		}

		if (CurrentCell->Type == EML_TileType::Dirt)
		{
			CurrentCell->Type = EML_TileType::Grass;
			bChanged = true;
			ExpandWaterNetwork(State, CurrentAxial, WaterConnected);
		}

		TArray<FIntPoint> Neighbors;
		GetNeighbors(State, CurrentAxial, Neighbors);

		for (const FIntPoint& NeighborAxial : Neighbors)
		{
			if (Scheduled.Contains(NeighborAxial))
			{
				continue;
			}

			const FML_PuzzleCell* NeighborCell = State.FindCell(NeighborAxial);
			if (!NeighborCell || !UML_TileTypeTraits::CanGrassPropagateTo(NeighborCell->Type))
			{
				continue;
			}

			TArray<FIntPoint> AroundNeighbors;
			GetNeighbors(State, NeighborAxial, AroundNeighbors);

			bool bTouchesWater = false;
			for (const FIntPoint& AroundAxial : AroundNeighbors)
			{
				if (WaterConnected.Contains(AroundAxial))
				{
					bTouchesWater = true;
					break;
				}
			}

			if (!bTouchesWater)
			{
				continue;
			}

			Scheduled.Add(NeighborAxial);
			PropagationQueue.Enqueue(NeighborAxial);
		}
	}

	return bChanged;
}

bool FML_PuzzleSimulator::SimulateWaterWave(FML_PuzzleState& State)
{
	TQueue<FIntPoint> Queue;
	TSet<FIntPoint> Visited;

	for (const FML_PuzzleCell& Cell : State.Cells)
	{
		if (Cell.Type == EML_TileType::Water)
		{
			Queue.Enqueue(Cell.Axial);
			Visited.Add(Cell.Axial);
		}
	}

	bool bChanged = false;

	while (!Queue.IsEmpty())
	{
		FIntPoint CurrentAxial;
		Queue.Dequeue(CurrentAxial);

		TArray<FIntPoint> Neighbors;
		GetNeighbors(State, CurrentAxial, Neighbors);

		for (const FIntPoint& NeighborAxial : Neighbors)
		{
			if (Visited.Contains(NeighborAxial))
			{
				continue;
			}

			Visited.Add(NeighborAxial);

			FML_PuzzleCell* NeighborCell = State.FindCellMutable(NeighborAxial);
			if (!NeighborCell || NeighborCell->Type != EML_TileType::Parasite)
			{
				continue;
			}

			NeighborCell->Type = EML_TileType::Water;
			bChanged = true;
			Queue.Enqueue(NeighborAxial);
		}
	}

	return bChanged;
}

bool FML_PuzzleSimulator::SimulateParasiteWave(FML_PuzzleState& State)
{
	TQueue<FIntPoint> Queue;
	TSet<FIntPoint> Visited;

	for (const FML_PuzzleCell& Cell : State.Cells)
	{
		if (Cell.Type == EML_TileType::Parasite)
		{
			Queue.Enqueue(Cell.Axial);
			Visited.Add(Cell.Axial);
		}
	}

	bool bChanged = false;

	while (!Queue.IsEmpty())
	{
		FIntPoint CurrentAxial;
		Queue.Dequeue(CurrentAxial);

		TArray<FIntPoint> Neighbors;
		GetNeighbors(State, CurrentAxial, Neighbors);

		for (const FIntPoint& NeighborAxial : Neighbors)
		{
			if (Visited.Contains(NeighborAxial))
			{
				continue;
			}

			Visited.Add(NeighborAxial);

			FML_PuzzleCell* NeighborCell = State.FindCellMutable(NeighborAxial);
			if (!NeighborCell || NeighborCell->Type != EML_TileType::Grass)
			{
				continue;
			}

			NeighborCell->Type = EML_TileType::Parasite;
			bChanged = true;
			Queue.Enqueue(NeighborAxial);
		}
	}

	return bChanged;
}

void FML_PuzzleSimulator::ExpandWaterNetwork(
	const FML_PuzzleState& State,
	const FIntPoint& FromAxial,
	TSet<FIntPoint>& WaterConnected)
{
	TQueue<FIntPoint> Queue;

	TArray<FIntPoint> Neighbors;
	GetNeighbors(State, FromAxial, Neighbors);

	for (const FIntPoint& NeighborAxial : Neighbors)
	{
		const FML_PuzzleCell* NeighborCell = State.FindCell(NeighborAxial);
		if (NeighborCell && NeighborCell->Type == EML_TileType::Water && !WaterConnected.Contains(NeighborAxial))
		{
			WaterConnected.Add(NeighborAxial);
			Queue.Enqueue(NeighborAxial);
		}
	}

	while (!Queue.IsEmpty())
	{
		FIntPoint CurrentAxial;
		Queue.Dequeue(CurrentAxial);

		TArray<FIntPoint> WaterNeighbors;
		GetNeighbors(State, CurrentAxial, WaterNeighbors);

		for (const FIntPoint& NeighborAxial : WaterNeighbors)
		{
			const FML_PuzzleCell* NeighborCell = State.FindCell(NeighborAxial);
			if (!NeighborCell || NeighborCell->Type != EML_TileType::Water || WaterConnected.Contains(NeighborAxial))
			{
				continue;
			}

			WaterConnected.Add(NeighborAxial);
			Queue.Enqueue(NeighborAxial);
		}
	}
}
