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
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCheckPaths);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnConnectedGoalPathTile, AML_Tile*, Tile);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDisconnectedGoalPathTile, const TArray<AML_Tile*>&, Tiles);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnConnectedGoalPathComplete);

UCLASS()
class MYCELAND_API UML_WinLoseSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Myceland WinLose")
	FOnWin OnWin;

	UPROPERTY(BlueprintAssignable, Category = "Myceland WinLose")
	FOnLose OnLose;

	UPROPERTY(BlueprintAssignable, Category = "Myceland WinLose")
	FOnDeath OnDeath;

	UPROPERTY(BlueprintAssignable, Category = "Myceland WinLose")
	FOnCheckPaths OnCheckPaths;

	UPROPERTY(BlueprintAssignable, Category = "Myceland WinLose")
	FOnConnectedGoalPathTile OnConnectedGoalPathTile;

	UPROPERTY(BlueprintAssignable, Category = "Myceland WinLose")
	FOnDisconnectedGoalPathTile OnDisconnectedGoalPathTile;

	UPROPERTY(BlueprintAssignable, Category = "Myceland WinLose")
	FOnConnectedGoalPathComplete OnConnectedGoalPathComplete;

	UFUNCTION(BlueprintCallable, Category = "Myceland WinLose")
	FML_GameResult CheckWinLose();

	UFUNCTION(BlueprintCallable, Category = "Myceland WinLose")
	bool CheckPlayerKilled(AML_Tile* CurrentTileOn);

	UFUNCTION(BlueprintCallable, Category = "Myceland WinLose")
	bool AreAllGoalsConnectedByAllowedPaths(
		AML_BoardSpawner* Board,
		EML_TileType GoalType,
		const TArray<EML_TileType>& AllowedPathTypes);

	UFUNCTION(BlueprintCallable, Category = "Myceland WinLose")
	bool FindConnectedGoalGroups(
		AML_BoardSpawner* Board,
		EML_TileType GoalType,
		const TArray<EML_TileType>& AllowedPathTypes,
		bool bDisallowBlocked,
		int32 MinGoalsInGroup);

	UFUNCTION(BlueprintCallable, Category = "Myceland WinLose")
	void TriggerFindConnectedGoalCheck();

	UFUNCTION(BlueprintCallable, Category = "Myceland WinLose")
	void ResetConnectedGoalPathState();

	UFUNCTION(BlueprintCallable, Category = "Myceland WinLose")
	void RemoveTileFromConnectedGoalPath(AML_Tile* Tile);

	UFUNCTION(BlueprintCallable, Category = "Myceland WinLose")
	AML_Tile* GetPlayerCurrentTile() const;

	UFUNCTION(BlueprintCallable, Category = "Myceland WinLose")
	AML_BoardSpawner* FindBoardSpawner() const;

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	UFUNCTION(BlueprintCallable, Category = "Myceland WinLose")
	void ClearWinPath(
		const AML_BoardSpawner* Board,
		const AML_Tile* StartTile,
		const AML_Tile* GoalTile,
		const TArray<EML_TileType>& AllowedPathTypes) const;

	UPROPERTY(BlueprintReadOnly, Category = "Myceland WinLose")
	AML_BoardSpawner* CurrentBoardSpawner = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Myceland WinLose")
	TArray<AML_Tile*> PathTiles;

	UPROPERTY(BlueprintReadOnly, Category = "Myceland WinLose")
	TArray<FML_TileGroup> ConnectedGoalGroups;

	UPROPERTY(BlueprintReadOnly, Category = "Myceland WinLose")
	bool bIsPlayerDead = false;

private:
	UFUNCTION()
	void HandleBoardChanged(const AML_Tile* NewTile);

	UFUNCTION()
	void HandleUndoAnimating(bool bIsAnimating);

	UFUNCTION()
	void HandleResetAnimating(bool bIsAnimating);

UFUNCTION()
	void BroadcastNextConnectedGoalPathTile();

	UPROPERTY()
	TSet<AML_Tile*> PreviousConnectedPathTiles;

	UPROPERTY()
	TArray<AML_Tile*> PendingConnectedGoalPathQueue;

	FTimerHandle ConnectedGoalPathTimerHandle;

	bool bPendingClearWinPath = false;

	static const FIntPoint HexDirs[6];

	TSet<EML_TileType> BuildAllowedSet(const TArray<EML_TileType>& AllowedPathTypes) const;

	TArray<FIntPoint> CollectGoalAxials(
		const TMap<FIntPoint, AML_Tile*>& Grid,
		EML_TileType GoalType,
		bool bDisallowBlocked = false) const;

	void RunBFS(
		const TMap<FIntPoint, AML_Tile*>& Grid,
		const FIntPoint& Start,
		TFunctionRef<bool(AML_Tile*)> CanTraverse,
		TSet<FIntPoint>& OutVisited,
		TMap<FIntPoint, FIntPoint>& OutParent) const;

	bool BuildPathAxialsFromParent(
		const FIntPoint& Start,
		const FIntPoint& Target,
		const TMap<FIntPoint, FIntPoint>& Parent,
		TArray<FIntPoint>& OutAxials) const;

	bool ConvertAxialsToTiles(
		const TMap<FIntPoint, AML_Tile*>& Grid,
		const TArray<FIntPoint>& Axials,
		TArray<AML_Tile*>& OutTiles) const;

private:
	TWeakObjectPtr<AML_PlayerCharacter> BoundPlayer;
};