// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/ML_CoreData.h"
#include "GameplayTagContainer.h"
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

// Ordered list of puzzle IDs solved within one level (oldest → most recent).
// Wrapped in a struct because UHT forbids TArray as a direct TMap value.
USTRUCT(BlueprintType)
struct FML_LevelSolveOrder
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadWrite)
	TArray<FName> PuzzleIDs;
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

// One row of the save-slot list, built by UML_SaveSubsystem::GetAllSaveSlots for the UI.
// Purely a read model: the authoritative data always lives in the UML_GameSave objects.
USTRUCT(BlueprintType)
struct FML_SaveSlotInfo
{
	GENERATED_BODY()

	// Technical identifier to pass back to ContinueFromSlot / DeleteSlot. Never shown to the
	// player. Normal slots use their file name ("Slot_3"); packaged demo saves are prefixed
	// with "demo:" so the two can never be confused, whatever their file is called.
	UPROPERTY(BlueprintReadOnly, Category="Myceland Save")
	FString SlotName;

	// Human-readable label for the UI (level name, or the label given to ExportActiveSlotAsDemo).
	UPROPERTY(BlueprintReadOnly, Category="Myceland Save")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category="Myceland Save")
	FDateTime LastSaveTime;

	// Level this slot resumes into. Feed it straight to UIManager::OpenLevelByTag.
	UPROPERTY(BlueprintReadOnly, Category="Myceland Save")
	FGameplayTag CurrentLevel;

	// True for the demo saves packaged in Content/DemoSaves. They can never be written to or
	// deleted: continuing one duplicates it into a fresh normal slot first.
	UPROPERTY(BlueprintReadOnly, Category="Myceland Save")
	bool bIsReadOnly = false;
};
