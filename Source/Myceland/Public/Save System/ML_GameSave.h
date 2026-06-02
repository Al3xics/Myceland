// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Save System/ML_GameSaveData.h"
#include "ML_GameSave.generated.h"

UCLASS()
class MYCELAND_API UML_GameSave : public USaveGame
{
	GENERATED_BODY()

public:
	// General game settings (brightness, volume, resolution, …)
	UPROPERTY(SaveGame)
	FML_GameSaveData Settings;

	// One record per puzzle, keyed by the PuzzleID set on AML_BoardSpawner.
	UPROPERTY(SaveGame)
	TMap<FName, FML_PuzzleSaveRecord> PuzzleRecords;

	// PuzzleID of the most recently solved puzzle. Used to restore the player's
	// spawn position to that puzzle's exit tile on the next session load.
	UPROPERTY(SaveGame)
	FName LastSolvedPuzzleID;
};
