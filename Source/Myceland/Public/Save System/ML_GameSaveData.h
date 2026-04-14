// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/ML_CoreData.h"
#include "ML_GameSaveData.generated.h"

USTRUCT(BlueprintType)
struct FML_GameSaveData
{
	GENERATED_BODY()
	//In game advancement
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Level = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 CurrentPuzzle = 0;

	//Options to save
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Brightness = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MusicVolume = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString PlayerName = TEXT("Player");

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FIntPoint Resolution = FIntPoint(1920, 1080);

	//Player statistics
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int64 TotalPlayTimeSeconds = 0;
};

// One tile entry in a saved grid snapshot: axial coordinate + tile type.
USTRUCT(BlueprintType)
struct FML_TileSaveEntry
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadWrite)
	FIntPoint Axial = FIntPoint::ZeroValue;

	UPROPERTY(SaveGame, BlueprintReadWrite)
	EML_TileType TileType = EML_TileType::Dirt;
};

// All persistent data for one puzzle board, keyed by PuzzleID in the save object.
USTRUCT(BlueprintType)
struct FML_PuzzleSaveRecord
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadWrite)
	bool bIsSolved = false;

	// Board state as authored in the level (captured on first BeginPlay).
	UPROPERTY(SaveGame, BlueprintReadWrite)
	TArray<FML_TileSaveEntry> InitialGrid;

	// Board state at the moment the player won (empty until the puzzle is solved).
	UPROPERTY(SaveGame, BlueprintReadWrite)
	TArray<FML_TileSaveEntry> SolvedGrid;
};
