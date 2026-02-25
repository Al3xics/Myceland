// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tiles/ML_BoardSpawner.h"
#include "ML_WinLoseSubsystem.generated.h"

class AML_Tile;
class AML_PlayerCharacter;
struct FML_GameResult;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWin);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLose);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDeath);

UCLASS()
class MYCELAND_API UML_WinLoseSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category="Myceland WinLose")
	FOnWin OnWin;

	UPROPERTY(BlueprintAssignable, Category="Myceland WinLose")
	FOnLose OnLose;

	UPROPERTY(BlueprintAssignable, Category="Myceland WinLose")
	FOnLose OnDeath;
	
	UFUNCTION(BlueprintCallable, Category = "Myceland WinLose")
	FML_GameResult CheckWinLose();

	UFUNCTION(BlueprintCallable, Category = "Myceland WinLose")
	bool CheckPlayerKilled(AML_Tile* CurrentTileOn);

	UFUNCTION(BlueprintCallable, Category = "Myceland WinLose")
	bool AreAllGoalsConnectedByAllowedPaths(AML_BoardSpawner* Board,
										   EML_TileType GoalType,
										   const TArray<EML_TileType>& AllowedPathTypes) const;

	UFUNCTION(BlueprintCallable, Category = "Myceland WinLose")
	AML_Tile* GetPlayerCurrentTile() const;
	
	UPROPERTY(BlueprintReadOnly, Category = "Myceland WinLose")
	AML_BoardSpawner* CurrentBoardSpawner;

	UFUNCTION(BlueprintCallable, Category = "Myceland WinLose")
	AML_BoardSpawner* FindBoardSpawner() const;
	
	bool bIsPlayerDead;

private:
	TWeakObjectPtr<AML_PlayerCharacter> BoundPlayer;
};
