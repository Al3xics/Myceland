// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tiles/ML_BoardSpawner.h"
#include "ML_WinLoseSubsystem.generated.h"

class UML_MycelandDeveloperSettings;
class AML_Tile;
class AML_PlayerCharacter;
struct FML_GameResult;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWin);
// Fires after BOTH ClearWinPath passes have run and every winning-path tile has
// settled to WaterPath. Use this (not OnWin) when you need the final board state,
// e.g. snapshotting the solved grid for the save system.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWinPathSettled);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLose);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDeath);
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

	// Broadcast one tick after OnWin, once the deferred Entry→Exit ClearWinPath
	// pass has finished, so listeners see the fully-settled winning path.
	UPROPERTY(BlueprintAssignable, Category = "Myceland WinLose")
	FOnWinPathSettled OnWinPathSettled;

	UPROPERTY(BlueprintAssignable, Category = "Myceland WinLose")
	FOnLose OnLose;

	UPROPERTY(BlueprintAssignable, Category = "Myceland WinLose")
	FOnDeath OnDeath;

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
	bool FindConnectedGoalGroups(
		AML_BoardSpawner* Board,
		EML_TileType GoalType,
		const TArray<EML_TileType>& AllowedPathTypes,
		bool bDisallowBlocked,
		int32 MinGoalsInGroup);

	UFUNCTION(BlueprintCallable, Category = "Myceland WinLose")
	void TriggerFindConnectedGoalCheck();

	/**
	 * Plays the tile-link animation for Board's connected goal groups without
	 * touching PreviousConnectedPathTiles or CurrentBoardSpawner.
	 *
	 * Use this instead of TriggerFindConnectedGoalCheck when the board is NOT
	 * the player's current active board (e.g. the HubBoardSpawner showing
	 * solved-puzzle connections).  Tiles are tracked in PersistentAnimatedTiles
	 * so re-entering the hub never re-animates already-shown connections.
	 * Unlike the regular flow, these tiles are never broadcast as disconnected.
	 *
	 * When bImmediate is true the tiles are broadcast synchronously instead of staggered
	 * over a timer. This is used on load: it avoids the shared queue/timer so a board
	 * change (which calls ResetConnectedGoalPathState) can't cancel a partial reveal.
	 */
	UFUNCTION(BlueprintCallable, Category = "Myceland WinLose")
	void TriggerConnectedGoalAnimationForBoard(AML_BoardSpawner* Board, bool bImmediate = false);

	UFUNCTION(BlueprintCallable, Category = "Myceland WinLose")
	void ResetConnectedGoalPathState();

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
	TArray<FML_TileGroup> ConnectedGoalGroups;

	UPROPERTY(BlueprintReadOnly, Category = "Myceland WinLose")
	bool bIsPlayerDead = false;

private:
	UFUNCTION()
	void HandleBoardChanged(const AML_Tile* OldTile, const AML_Tile* NewTile);

	UFUNCTION()
	void HandleUndoAnimating(bool bIsAnimating);

	UFUNCTION()
	void HandleResetAnimating(bool bIsAnimating);

	UFUNCTION()
	void BroadcastNextConnectedGoalPathTile();

	/**
	 * Broadcasts OnWin, clears the pending flag, and runs both ClearWinPath
	 * passes (player→exit now, entry→exit deferred one tick).
	 * Called from BroadcastNextConnectedGoalPathTile when the queue drains,
	 * and as a fallback from CheckWinLose when no new tiles need to be animated.
	 */
	void FireWinSequence();

	UPROPERTY()
	TSet<AML_Tile*> PreviousConnectedPathTiles;

	/**
	 * Tiles already animated by TriggerConnectedGoalAnimationForBoard (hub connections).
	 * Never cleared by board-change events so hub visuals remain after the player leaves.
	 * Prevents re-animation if the same hub entry is re-activated.
	 */
	UPROPERTY()
	TSet<AML_Tile*> PersistentAnimatedTiles;

	UPROPERTY()
	TArray<AML_Tile*> PendingConnectedGoalPathQueue;

	FTimerHandle ConnectedGoalPathTimerHandle;
	FTimerHandle ConnectedWinTimerHandle;

	UPROPERTY()
	const UML_MycelandDeveloperSettings* DevSettings = nullptr;

	int32 QueueReadIndex = 0;
	bool bPendingClearWinPath = false;

	static const FIntPoint HexDirs[6];

	TSet<EML_TileType> CachedGoalPathAllowedSet;

	TSet<EML_TileType> BuildAllowedSet(const TArray<EML_TileType>& AllowedPathTypes) const;

	bool AreAllGoalsConnected(
		AML_BoardSpawner* Board,
		EML_TileType GoalType,
		const TSet<EML_TileType>& AllowedSet);

	bool FindConnectedGoalGroups(
		AML_BoardSpawner* Board,
		EML_TileType GoalType,
		const TSet<EML_TileType>& AllowedSet,
		bool bDisallowBlocked,
		int32 MinGoalsInGroup);

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

	void RunZeroOneBFS(
		const TMap<FIntPoint, AML_Tile*>& Grid,
		const FIntPoint& Start,
		TFunctionRef<bool(AML_Tile*)> CanTraverse,
		TFunctionRef<int32(AML_Tile*)> GetCost,
		TSet<FIntPoint>& OutVisited,
		TMap<FIntPoint, FIntPoint>& OutParent,
		TMap<FIntPoint, int32>& OutDist) const;

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