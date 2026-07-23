// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/ML_CoreData.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Save System/ML_GameSaveData.h"
#include "PuzzleGeneration/ML_PuzzleGenerationTypes.h"
#include "LevelSequence.h"
#include "CineCameraActor.h"
#include "Actors/ML_CinematicCameraRail.h"
#include "ML_BoardSpawner.generated.h"

class UML_BiomeTileSet;
class UML_SaveSubsystem;
class AML_CameraRail;
class AML_Collectible;
class AML_TileBase;
class AML_TileWater;
class AML_TileParasite;
class AML_TileGrass;
class AML_Tile;

// Fixed-capacity neighbor list (6 hex directions) stored inline on the stack:
// filling it never allocates heap memory, unlike a regular TArray.
using FML_TileNeighbors = TArray<AML_Tile*, TInlineAllocator<6>>;

// All categories below are prefixed "ML- " so typing "ML" in the Details panel search box
// filters the panel down to this class's own categories. The Details panel sorts categories by
// declaration order, never by name, so the prefix is for searching only — don't expect it to sort.
UCLASS()
class MYCELAND_API AML_BoardSpawner : public AActor
{
	GENERATED_BODY()

private:
	// ==================== ML- Runtime ====================

	UPROPERTY(EditInstanceOnly, Category="ML- Runtime")
	int32 EnergyForPuzzle = 1;

	UPROPERTY(EditInstanceOnly, Category="ML- Runtime")
	UML_BiomeTileSet* BiomeTileSet;

	UPROPERTY(EditInstanceOnly, Category="ML- Runtime")
	ACameraActor* AssociatedCamera;

	UPROPERTY(EditInstanceOnly, Category="ML- Runtime")
	AActor* AssociatedObstacle;

	UPROPERTY(EditInstanceOnly, Category="ML- Runtime")
	TArray<AActor*> AssociatedNatureZones;

	UPROPERTY(EditInstanceOnly, Category="ML- Runtime")
	TArray<AActor*> AssociatedWaterPaths;

	UPROPERTY(EditInstanceOnly, Category="ML- Runtime")
	ULevelSequence* AssociatedWinCinematic;

	UPROPERTY(EditInstanceOnly, Category="ML- Runtime")
	TObjectPtr<AML_CinematicCameraRail> AssociatedStartCinematicRail = nullptr;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AML_Tile>> SpawnedTiles;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AML_Tile>> TreeTiles;

	TMap<FIntPoint, TObjectPtr<AML_Tile>> GridMap;

	mutable TMap<FIntPoint, AML_Tile*> GridMapCache;
	mutable bool bGridMapCacheBuilt = false;

	// ========== Board switches (per-instance, runtime-toggleable) ==========
	// Kept private so all runtime changes go through the setters (SetGlowEnabled clears any
	// active glow on OFF). EditAnywhere/BlueprintReadOnly still expose them via reflection.

	// When false, this board's tiles never glow (cursor hover + path preview).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ML- Board Switches", meta=(AllowPrivateAccess="true"))
	bool bGlowEnabled = true;

	// When false, the player never enters this board's tile-by-tile mode: clicking it stays in
	// free movement, and there is no exit-hold. The board movement system is fully disabled for it.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ML- Board Switches", meta=(AllowPrivateAccess="true"))
	bool bBoardTransitionEnabled = true;

	// Generators
	void UpdateCurrentGrid(bool bAllowSpawn = true);
	void SpawnHexagonRadius();
	void SpawnRectangleWH();
	void RefreshTileCaches();

	// Conversions
	FVector AxialToWorld(int32 Q, int32 R) const;
	FIntPoint OffsetToAxial(int32 Col, int32 Row) const;

	FML_PuzzleState BuildPuzzleStateFromCurrentGrid() const;

	// Blueprint-only version (UHT can't expose the inline-allocator array above, so this
	// one copies into a regular TArray). Private on purpose: Blueprint ignores C++ access
	// specifiers so BP graphs can still call it, while C++ code is forced to use the
	// allocation-free overload above.
	UFUNCTION(BlueprintCallable, Category="ML- Hex Grid")
	TArray<AML_Tile*> GetNeighbors(AML_Tile* CenterTile) const;

protected:
	virtual void Destroyed() override;
	virtual void BeginPlay() override;

#if WITH_EDITOR
	// Editing bGlowEnabled/bBoardTransitionEnabled in the Details panel writes the field directly,
	// bypassing the setters. Re-run their side-effects here so editor toggles behave like the setters.
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

public:
	AML_BoardSpawner();
	FIntPoint WorldToAxial(const FVector& WorldLocation) const;


	// ==================== ML- Hex Grid ====================

	UPROPERTY(EditAnywhere, Category="ML- Hex Grid")
	TSubclassOf<AML_Tile> TileClass;

	UPROPERTY(EditAnywhere, Category="ML- Hex Grid")
	EML_HexGridLayout GridLayout = EML_HexGridLayout::HexagonRadius;

	UPROPERTY(EditAnywhere, Category="ML- Hex Grid", meta=(ClampMin="0", EditCondition="GridLayout==EML_HexGridLayout::HexagonRadius", EditConditionHides))
	int32 Radius = 2;

	UPROPERTY(EditAnywhere, Category="ML- Hex Grid|Rectangle", meta=(ClampMin="1", EditCondition="GridLayout==EML_HexGridLayout::RectangleWH", EditConditionHides))
	int32 GridWidth = 5;

	UPROPERTY(EditAnywhere, Category="ML- Hex Grid|Rectangle", meta=(ClampMin="1", EditCondition="GridLayout==EML_HexGridLayout::RectangleWH", EditConditionHides))
	int32 GridHeight = 5;

	UPROPERTY(EditAnywhere, Category="ML- Hex Grid")
	EML_HexOrientation Orientation = EML_HexOrientation::FlatTop;

	UPROPERTY(EditAnywhere, Category="ML- Hex Grid", meta=(EditCondition="GridLayout==EML_HexGridLayout::RectangleWH", EditConditionHides))
	EML_HexOffsetLayout OffsetLayout = EML_HexOffsetLayout::OddR;

	UPROPERTY(EditAnywhere, Category="ML- Hex Grid", meta=(ClampMin="1.0"))
	float TileSize = 100.f;

	UPROPERTY(EditAnywhere, Category="ML- Hex Grid")
	FRotator TileRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, Category="ML- Hex Grid", meta=(ClampMin="0.01"))
	FVector TileScale = FVector(2.f, 2.f, 2.f);
	
	UPROPERTY(EditAnywhere, Category="ML- Hex Grid", meta=(Categories="Puzzle"))
	FGameplayTag PuzzleID;

	UPROPERTY(EditAnywhere, Category="ML- Hex Grid")
	TArray<AML_CameraRail*> AssociatedCameraRails;

	UPROPERTY(EditAnywhere, Category="ML- Hex Grid")
	TArray<FML_WaterPath> WaterPaths;

	UPROPERTY(EditAnywhere, Category="ML- Hex Grid")
	TSubclassOf<AML_TileBase> WaterChangeTile;

	UPROPERTY(BlueprintReadWrite)
    bool bIsPuzzleSolved;

	UFUNCTION(CallInEditor, Category="ML- Hex Grid", meta=(DisplayName="Update Current Grid"))
	void UpdateCurrentGridEditor();

	UFUNCTION(CallInEditor, Category="ML- Hex Grid", meta=(DisplayName="Clear & Rebuild Grid"))
	void RebuildGrid();

	UFUNCTION(CallInEditor, Category="ML- Hex Grid", meta=(DisplayName="Clear Grid"))
	void ClearTiles();

	UFUNCTION(CallInEditor, Category="ML- Hex Grid", meta=(DisplayName="Destroy Stale Tiles"))
	void DestroyStaleTiles() const;

	// Fills the caller-provided inline buffer (no heap allocation, reusable across calls).
	// Always outputs 6 entries, nullptr where no neighbor exists.
	void GetNeighbors(const AML_Tile* CenterTile, FML_TileNeighbors& OutNeighbors) const;

	UFUNCTION(BlueprintPure, Category="ML- Runtime")
	TMap<FIntPoint, AML_Tile*> GetGridMap() const;

	const TMap<FIntPoint, AML_Tile*>& GetGridMapRef() const;

	UFUNCTION(BlueprintPure, Category="ML- Runtime")
	TArray<AML_Tile*> GetGridTiles();

	UFUNCTION(BlueprintPure, Category="ML- Runtime")
	TArray<AML_Tile*> GetTreeTiles() const;

	UFUNCTION(BlueprintPure, Category="ML- Hex Grid")
	AML_Tile* FindClosestWalkableBorderTile(const FVector& WorldLocation) const;

	UFUNCTION(BlueprintPure, Category="ML- Hex Grid")
	AML_CameraRail* GetClosestCameraRail(const FVector& WorldLocation) const;

	UFUNCTION(BlueprintPure, Category="ML- Runtime")
	int32 GetEnergyForPuzzle() const { return EnergyForPuzzle; }

	UFUNCTION(BlueprintPure, Category="ML- Runtime")
	UML_BiomeTileSet* GetBiomeTileSet() const { return BiomeTileSet; }

	UFUNCTION(BlueprintPure, Category="ML- Runtime")
	ACameraActor* GetAssociatedCamera() const { return AssociatedCamera; }

	UFUNCTION(BlueprintPure, Category="ML- Runtime")
	AActor* GetAssociatedObstacle() const { return AssociatedObstacle; }

	UFUNCTION(BlueprintPure, Category="ML- Runtime")
	ULevelSequence* GetAssociatedWinCinematic() const { return AssociatedWinCinematic; }

	UFUNCTION(BlueprintPure, Category="ML- Runtime")
	AML_CinematicCameraRail* GetAssociatedStartCinematicRail() const { return AssociatedStartCinematicRail; }

	UFUNCTION(BlueprintPure, Category="ML- Runtime")
	TArray<AActor*> GetAssociatedNatureZones() const { return AssociatedNatureZones; }

	UFUNCTION(BlueprintPure, Category="ML- Runtime")
	TArray<AActor*> GetAssociatedWaterPaths() const { return AssociatedWaterPaths; }

	// ==================== ML- Board Switches ====================
	// Per-board ON/OFF switches (backing fields are private — see above).

	UFUNCTION(BlueprintPure, Category="ML- Board Switches")
	bool IsGlowEnabled() const { return bGlowEnabled; }

	UFUNCTION(BlueprintPure, Category="ML- Board Switches")
	bool IsBoardTransitionEnabled() const { return bBoardTransitionEnabled; }

	/** Enables/disables this board's glow. Clears any active glow immediately when turning OFF. */
	UFUNCTION(BlueprintCallable, Category="ML- Board Switches")
	void SetGlowEnabled(bool bEnabled);

	/** Enables/disables this board's entry/exit (tile-by-tile) system. Ejects the player to free movement when turning OFF while they are inside. */
	UFUNCTION(BlueprintCallable, Category="ML- Board Switches")
	void SetBoardTransitionEnabled(bool bEnabled);

	// ==================== ML- Board Exits (gamepad) ====================

	// Gamepad board exits: each entry maps a set of border tiles to a plane/actor to highlight.
	// This is pure per-board configuration — the availability broadcast lives on the player controller
	// (a single subscription point), so this board only needs to answer "which plane for this tile?".
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="ML- Board Exits")
	TArray<FML_BoardExit> BoardExits;

	// Returns the first exit whose tiles contain PlayerTile, or nullptr. Used both to know which plane
	// to highlight when the player stands on an exit tile, and to resolve the exit target on confirm.
	const FML_BoardExit* FindExitForTile(const AML_Tile* PlayerTile) const;

	// ==================== ML- Puzzle Generator ====================

	UPROPERTY(EditAnywhere, Category="ML- Puzzle Generator")
	FML_PuzzleGenerationSettings PuzzleGenerationSettings;

	UFUNCTION(CallInEditor, Category="ML- Puzzle Generator")
	void AnalyzeCurrentPuzzle();
	
	
	// Restores this board's tiles to their authored initial state and re-enables the
	// obstacle. Does NOT write to the save — call ReplayPuzzle (or ResetPuzzle on the
	// subsystem) to commit the change. Used directly for cascade resets.
	UFUNCTION(BlueprintCallable, Category="Myceland Hex Grid")
	virtual void RestoreToInitialState();

protected:

	// ==================== Save / Load Helpers ====================

	// If false, BeginPlay skips the automatic solved-grid restore so a subclass can
	// own its own restore path (e.g. the hub, which must snapshot pre-change tile
	// state for reverts before repainting). Default: true.
	virtual bool ShouldAutoRestoreSolvedGrid() const { return true; }

	// Returns a snapshot of the current GridMap as a flat array of tile entries.
	TArray<FML_TileSaveEntry> SnapshotGrid() const;

	// Fetches the SaveSubsystem from the owning GameInstance. Returns null if unavailable.
	UML_SaveSubsystem* GetSaveSubsystem() const;

	// Returns a stable level key (map name with PIE prefix stripped) used to scope
	// solve orders and cascade resets to the current level.
	FName GetLevelKey() const;

private:

	// Resets this puzzle (and every puzzle solved after it) back to its initial state
	// and updates the save. Intended for editor and Blueprint use.
	UFUNCTION(CallInEditor, BlueprintCallable, Category="Myceland Hex Grid", meta=(DisplayName="Replay Puzzle (Reset to Initial State)"))
	void ReplayPuzzle();

	// Bound to UML_WinLoseSubsystem::OnWin; captures the solved grid and saves to disk.
	UFUNCTION()
	void HandlePuzzleWon();
};
