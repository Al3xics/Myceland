#pragma once

#include "CoreMinimal.h"
#include "Core/ML_CoreData.h"
#include "ML_PuzzleGenerationTypes.generated.h"

UENUM(BlueprintType)
enum class EML_PuzzleDifficulty : uint8
{
	Easy,
	Medium,
	Hard
};

USTRUCT(BlueprintType)
struct FML_PuzzleGenerationSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EML_PuzzleDifficulty Difficulty = EML_PuzzleDifficulty::Easy;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Seed = 12345;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1"))
	int32 MaxSearchDepth = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0"))
	int32 MinSolutionCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1"))
	int32 MaxSolutionCount = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1"))
	int32 MinShortestSolutionLength = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1"))
	int32 MaxShortestSolutionLength = 3;
};

USTRUCT(BlueprintType)
struct FML_PuzzleCell
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FIntPoint Axial = FIntPoint::ZeroValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EML_TileType Type = EML_TileType::Dirt;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bHasCollectible = false;
};

USTRUCT(BlueprintType)
struct FML_PuzzleState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FML_PuzzleCell> Cells;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FIntPoint EntryAxial = FIntPoint::ZeroValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FIntPoint ExitAxial = FIntPoint::ZeroValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Energy = 1;

	bool TryGetCell(const FIntPoint& Axial, FML_PuzzleCell& OutCell) const
	{
		for (const FML_PuzzleCell& Cell : Cells)
		{
			if (Cell.Axial == Axial)
			{
				OutCell = Cell;
				return true;
			}
		}
		return false;
	}

	FML_PuzzleCell* FindCellMutable(const FIntPoint& Axial)
	{
		for (FML_PuzzleCell& Cell : Cells)
		{
			if (Cell.Axial == Axial)
			{
				return &Cell;
			}
		}
		return nullptr;
	}

	const FML_PuzzleCell* FindCell(const FIntPoint& Axial) const
	{
		for (const FML_PuzzleCell& Cell : Cells)
		{
			if (Cell.Axial == Axial)
			{
				return &Cell;
			}
		}
		return nullptr;
	}
};

USTRUCT(BlueprintType)
struct FML_PuzzleSolution
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FIntPoint> PlantActions;
};

USTRUCT(BlueprintType)
struct FML_PuzzleSolveReport
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bSolvable = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 SolutionCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ShortestSolutionLength = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FML_PuzzleSolution ShortestSolution;
};
