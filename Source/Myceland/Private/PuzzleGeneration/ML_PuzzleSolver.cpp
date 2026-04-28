#include "PuzzleGeneration/ML_PuzzleSolver.h"
#include "PuzzleGeneration/ML_PuzzleSimulator.h"

struct FML_PuzzleSearchNode
{
	FML_PuzzleState State;
	TArray<FIntPoint> Actions;
	int32 RemainingEnergy = 0;
};

FML_PuzzleSolveReport FML_PuzzleSolver::Solve(
	const FML_PuzzleState& InitialState,
	const FML_PuzzleGenerationSettings& Settings)
{
	FML_PuzzleSolveReport Report;

	TQueue<FML_PuzzleSearchNode> Queue;
	TSet<FString> Visited;

	FML_PuzzleSearchNode Root;
	Root.State = InitialState;
	Root.RemainingEnergy = FMath::Min(InitialState.Energy, Settings.MaxSearchDepth);

	Queue.Enqueue(Root);
	Visited.Add(BuildStateKey(Root.State, Root.RemainingEnergy));

	while (!Queue.IsEmpty())
	{
		FML_PuzzleSearchNode Node;
		Queue.Dequeue(Node);

		if (FML_PuzzleSimulator::IsWinningState(Node.State))
		{
			Report.bSolvable = true;
			Report.SolutionCount++;

			if (Report.ShortestSolutionLength == 0 || Node.Actions.Num() < Report.ShortestSolutionLength)
			{
				Report.ShortestSolutionLength = Node.Actions.Num();
				Report.ShortestSolution.PlantActions = Node.Actions;
			}

			if (Report.SolutionCount >= Settings.MaxSolutionCount)
			{
				return Report;
			}

			continue;
		}

		if (Node.RemainingEnergy <= 0)
		{
			continue;
		}

		for (const FML_PuzzleCell& Cell : Node.State.Cells)
		{
			if (Cell.Type != EML_TileType::Dirt)
			{
				continue;
			}

			FML_PuzzleSearchNode Next;
			Next.State = Node.State;
			Next.Actions = Node.Actions;
			Next.Actions.Add(Cell.Axial);
			Next.RemainingEnergy = Node.RemainingEnergy - 1;

			if (!FML_PuzzleSimulator::SimulatePlantAction(Next.State, Cell.Axial))
			{
				continue;
			}

			const FString Key = BuildStateKey(Next.State, Next.RemainingEnergy);
			if (Visited.Contains(Key))
			{
				continue;
			}

			Visited.Add(Key);
			Queue.Enqueue(Next);
		}
	}

	return Report;
}

FString FML_PuzzleSolver::BuildStateKey(const FML_PuzzleState& State, int32 RemainingEnergy)
{
	TArray<FML_PuzzleCell> SortedCells = State.Cells;
	SortedCells.Sort([](const FML_PuzzleCell& A, const FML_PuzzleCell& B)
	{
		if (A.Axial.X != B.Axial.X)
		{
			return A.Axial.X < B.Axial.X;
		}

		return A.Axial.Y < B.Axial.Y;
	});

	FString Key = FString::Printf(TEXT("E%d|"), RemainingEnergy);

	for (const FML_PuzzleCell& Cell : SortedCells)
	{
		Key += FString::Printf(
			TEXT("%d,%d:%d;"),
			Cell.Axial.X,
			Cell.Axial.Y,
			static_cast<int32>(Cell.Type));
	}

	return Key;
}
