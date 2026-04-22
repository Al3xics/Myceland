#pragma once

#include "CoreMinimal.h"
#include "PuzzleGeneration/ML_PuzzleGenerationTypes.h"

class MYCELAND_API FML_PuzzleSimulator
{
public:
	static bool SimulatePlantAction(FML_PuzzleState& State, const FIntPoint& PlantAxial);
	static bool IsWinningState(const FML_PuzzleState& State);
	static void GetNeighbors(const FML_PuzzleState& State, const FIntPoint& Axial, TArray<FIntPoint>& OutNeighbors);

private:
	static bool SimulateGrassWave(FML_PuzzleState& State, const FIntPoint& OriginAxial);
	static bool SimulateWaterWave(FML_PuzzleState& State);
	static bool SimulateParasiteWave(FML_PuzzleState& State);
	static void ExpandWaterNetwork(const FML_PuzzleState& State, const FIntPoint& FromAxial, TSet<FIntPoint>& WaterConnected);
	static bool IsDirtLike(EML_TileType Type);
	static bool IsConnectionType(EML_TileType Type);
};
