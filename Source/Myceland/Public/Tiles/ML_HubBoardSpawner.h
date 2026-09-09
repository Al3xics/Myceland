// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tiles/ML_BoardSpawner.h"
#include "ML_HubBoardSpawner.generated.h"

class AML_Tile;
class AML_TileBase;
class AML_TileDirt;
class AML_TileGrass;
class AML_TileParasite;
class AML_TileWater;
class AML_TileObstacle;
class AML_TileTree;
class UML_WinLoseSubsystem;
class UML_RollBackSubsystem;
class UML_WavePropagationSubsystem;


// ==================== Structs ====================

USTRUCT(BlueprintType)
struct FML_HubTileChange
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hub Tile Change")
	TObjectPtr<AML_Tile> TargetTile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hub Tile Change")
	EML_TileType TargetType = EML_TileType::Grass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hub Tile Change", meta=(EditCondition="TargetType==EML_TileType::Dirt", EditConditionHides))
	TSubclassOf<AML_TileDirt> DirtClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hub Tile Change", meta=(EditCondition="TargetType==EML_TileType::Grass", EditConditionHides))
	TSubclassOf<AML_TileGrass> GrassClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hub Tile Change", meta=(EditCondition="TargetType==EML_TileType::Parasite", EditConditionHides))
	TSubclassOf<AML_TileParasite> ParasiteClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hub Tile Change", meta=(EditCondition="TargetType==EML_TileType::Water||TargetType==EML_TileType::WaterPath", EditConditionHides))
	TSubclassOf<AML_TileWater> WaterClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hub Tile Change", meta=(EditCondition="TargetType==EML_TileType::Obstacle", EditConditionHides))
	TSubclassOf<AML_TileObstacle> ObstacleClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hub Tile Change", meta=(EditCondition="TargetType==EML_TileType::Tree", EditConditionHides))
	TSubclassOf<AML_TileTree> TreeClass;
};

USTRUCT(BlueprintType)
struct FML_HubPuzzleEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hub Puzzle Entry")
	TObjectPtr<AML_BoardSpawner> LinkedPuzzle;

	// Hub tiles to transform when LinkedPuzzle is won, applied in array order
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hub Puzzle Entry")
	TArray<FML_HubTileChange> TileChanges;

	// If true, tiles appear one by one in array order; if false, all at once
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hub Puzzle Entry")
	bool bSequentialSpawn = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hub Puzzle Entry", meta=(EditCondition="bSequentialSpawn", EditConditionHides, ClampMin="0.0"))
	float SequentialDelay = 0.5f;

	// Seconds to wait after OnBeforeHubTilesSpawn fires before the first tile appears.
	// Tune this to match the camera transition duration or something else if needed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hub Puzzle Entry", meta=(ClampMin="0.0"))
	float DelayBeforeFirstTile = 1.0f;

	// Seconds to wait after the last tile appears before OnHubTileChangesEnd fires.
	// Tune this to give the player time to see the final hub state before the camera blends back.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hub Puzzle Entry", meta=(ClampMin="0.0"))
	float DelayAfterLastTile = 1.0f;
};

// Internal — stores the tile state before a hub change, used for revert
USTRUCT()
struct FML_HubTileSnapshot
{
	GENERATED_BODY()

	UPROPERTY()
	EML_TileType OriginalType = EML_TileType::Dirt;

	UPROPERTY()
	TSubclassOf<AML_TileBase> OriginalClass;
};

USTRUCT()
struct FML_HubEntrySnapshot
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FML_HubTileSnapshot> Snapshots;
};


// ==================== Delegates ====================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBeforeHubTilesSpawn, int32, EntryIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHubTileChangesEnd, int32, EntryIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHubPuzzleTilesReverted, int32, EntryIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHubAllTilesPlaced);


// ==================== Class ====================

UCLASS()
class MYCELAND_API AML_HubBoardSpawner : public AML_BoardSpawner
{
	GENERATED_BODY()

private:
	// ==================== Internal State ====================

	TSet<int32> AppliedEntryIndices;

	UPROPERTY(Transient)
	TMap<int32, FML_HubEntrySnapshot> EntrySnapshots;

	// Captured at reset start so we can identify which entry to revert when the animation ends
	UPROPERTY(Transient)
	TObjectPtr<AML_BoardSpawner> PendingResetBoard;

	TArray<FTimerHandle> SpawnTimerHandles;

	UPROPERTY(Transient)
	TObjectPtr<UML_WinLoseSubsystem> WinLoseSubsystem;

	UPROPERTY(Transient)
	TObjectPtr<UML_RollBackSubsystem> RollBackSubsystem;

	UPROPERTY(Transient)
	TObjectPtr<UML_WavePropagationSubsystem> WavePropagationSubsystem;

	// ==================== Internal Methods ====================

	void BindDelegates();
	void UnbindDelegates();

	UFUNCTION()
	void HandleWin();

	UFUNCTION()
	void HandleResetAnimating(bool bIsResetAnimating);

	void ActivatePuzzleEntry(int32 EntryIndex);
	void ApplyAllTileChanges(int32 EntryIndex);
	void ApplySingleTileChange(int32 EntryIndex, int32 TileChangeIndex);
	void FinalizeTileChanges(int32 EntryIndex);
	void RevertPuzzleEntryTiles(int32 EntryIndex);
	void RehydrateAlreadySolvedPuzzles();
	void TakeEntrySnapshot(int32 EntryIndex);
	void CancelPendingSpawnTimers();
	int32 FindEntryIndexForBoard(const AML_BoardSpawner* Board) const;

	// Repaints the board from the persisted hub grid (which captures wave-propagation
	// results). Returns false if no snapshot exists so the caller can fall back to
	// replaying the per-entry TileChanges.
	bool RestoreSavedHubGrid();

	// Writes the current hub grid to the save. When bMarkSolved is true the hub is also
	// marked solved so it becomes the player's respawn anchor (only when fully revitalized).
	void PersistHubGrid(bool bMarkSolved);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// The hub owns its restore (see RehydrateAlreadySolvedPuzzles) so it can snapshot
	// pre-change tile state for reverts before repainting — opt out of the base auto-restore.
	virtual bool ShouldAutoRestoreSolvedGrid() const override { return false; }

	// The hub is never reset as one board: drop the per-entry bookkeeping and repaint
	// from whichever linked puzzles are still solved, rather than blanking to dirt.
	virtual void RestoreToInitialState() override;

public:
	AML_HubBoardSpawner();
UFUNCTION(BlueprintCallable, Category="Myceland Hub|Debug")
void ForceWin();
	// ==================== Myceland Hub ====================

	UPROPERTY(EditInstanceOnly, Category="Myceland Hub")
	TArray<FML_HubPuzzleEntry> PuzzleEntries;

	// ==================== Delegates ====================

	// Fires just before tiles for a puzzle entry begin spawning —
	// trigger a camera blend here (if needed); DelayBeforeFirstTile controls when tiles actually appear
	UPROPERTY(BlueprintAssignable, Category="Myceland Hub")
	FOnBeforeHubTilesSpawn OnBeforeHubTilesSpawn;

	// Fires when all tiles for a single puzzle entry have finished spawning — not when the hub itself is fully complete.
	UPROPERTY(BlueprintAssignable, Category="Myceland Hub")
	FOnHubTileChangesEnd OnHubTileChangesEnd;

	// Fires when hub tiles for a puzzle entry are reverted after that puzzle was reset
	UPROPERTY(BlueprintAssignable, Category="Myceland Hub")
	FOnHubPuzzleTilesReverted OnHubPuzzleTilesReverted;

	// Fires when every puzzle entry has had its tiles placed — the hub is fully revitalized.
	UPROPERTY(BlueprintAssignable, Category="Myceland Hub")
	FOnHubAllTilesPlaced OnHubAllTilesPlaced;
};
