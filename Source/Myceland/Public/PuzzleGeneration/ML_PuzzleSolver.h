#pragma once

#include "CoreMinimal.h"
#include "PuzzleGeneration/ML_PuzzleGenerationTypes.h"

class MYCELAND_API FML_PuzzleSolver
{
public:
	static FML_PuzzleSolveReport Solve(
		const FML_PuzzleState& InitialState,
		const FML_PuzzleGenerationSettings& Settings);

private:
	static FString BuildStateKey(const FML_PuzzleState& State, int32 RemainingEnergy);
};
