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

UCLASS()
class MYCELAND_API AML_BoardSpawner : public AActor
{
	GENERATED_BODY()

private:
	// ==================== Myceland Runtime ====================
	
	UPROPERTY(EditInstanceOnly, Category="Myceland Runtime")
	int32 EnergyForPuzzle = 1;
	
	UPROPERTY(EditInstanceOnly, Category="Myceland Runtime")
	UML_BiomeTileSet* BiomeTileSet;
	
	UPROPERTY(EditInstanceOnly, Category="Myceland Runtime")
	ACameraActor* AssociatedCamera;
	
	UPROPERTY(EditInstanceOnly, Category="Myceland Runtime")
	AActor* AssociatedObstacle;
	
	UPROPERTY(EditInstanceOnly, Category="Myceland Runtime")
	TArray<AActor*> AssociatedNatureZones;
	
	UPROPERTY(EditInstanceOnly, Category="Myceland Runtime")
	TArray<AActor*> AssociatedWaterPaths;
	
	UPROPERTY(EditInstanceOnly, Category="Myceland Runtime")
	ULevelSequence* AssociatedWinCinematic;
	
	UPROPERTY(EditInstanceOnly, Category="Myceland Runtime")
	TObjectPtr<AML_CinematicCameraRail> AssociatedStartCinematicRail = nullptr;
	
	UPROPERTY(Transient)
	TArray<TObjectPtr<AML_Tile>> SpawnedTiles;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AML_Tile>> TreeTiles;

	TMap<FIntPoint, TObjectPtr<AML_Tile>> GridMap;

	mutable TMap<FIntPoint, AML_Tile*> GridMapCache;
	mutable bool bGridMapCacheBuilt = false;
	
	// Generators
	void UpdateCurrentGrid(bool bAllowSpawn = true);
	void SpawnHexagonRadius();
	void SpawnRectangleWH();
	void RefreshTileCaches();

	// Conversions
	FVector AxialToWorld(int32 Q, int32 R) const;
	FIntPoint OffsetToAxial(int32 Col, int32 Row) const;
	
	FML_PuzzleState BuildPuzzleStateFromCurrentGrid() const;

protected:
	virtual void Destroyed() override;
	virtual void BeginPlay() override;

public:
	AML_BoardSpawner();
	FIntPoint WorldToAxial(const FVector& WorldLocation) const;
	
	
	// ==================== Myceland Hex Grid ====================
	
	UPROPERTY(EditAnywhere, Category="Myceland Hex Grid")
	TSubclassOf<AML_Tile> TileClass;

	UPROPERTY(EditAnywhere, Category="Myceland Hex Grid")
	EML_HexGridLayout GridLayout = EML_HexGridLayout::HexagonRadius;

	UPROPERTY(EditAnywhere, Category="Myceland Hex Grid", meta=(ClampMin="0", EditCondition="GridLayout==EML_HexGridLayout::HexagonRadius", EditConditionHides))
	int32 Radius = 2;

	UPROPERTY(EditAnywhere, Category="Myceland Hex Grid|Rectangle", meta=(ClampMin="1", EditCondition="GridLayout==EML_HexGridLayout::RectangleWH", EditConditionHides))
	int32 GridWidth = 5;

	UPROPERTY(EditAnywhere, Category="Myceland Hex Grid|Rectangle", meta=(ClampMin="1", EditCondition="GridLayout==EML_HexGridLayout::RectangleWH", EditConditionHides))
	int32 GridHeight = 5;

	UPROPERTY(EditAnywhere, Category="Myceland Hex Grid")
	EML_HexOrientation Orientation = EML_HexOrientation::FlatTop;

	UPROPERTY(EditAnywhere, Category="Myceland Hex Grid", meta=(EditCondition="GridLayout==EML_HexGridLayout::RectangleWH", EditConditionHides))
	EML_HexOffsetLayout OffsetLayout = EML_HexOffsetLayout::OddR;

	UPROPERTY(EditAnywhere, Category="Myceland Hex Grid", meta=(ClampMin="1.0"))
	float TileSize = 100.f;

	UPROPERTY(EditAnywhere, Category="Myceland Hex Grid")
	FRotator TileRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, Category="Myceland Hex Grid", meta=(ClampMin="0.01"))
	FVector TileScale = FVector(2.f, 2.f, 2.f);
	
	UPROPERTY(EditAnywhere, Category="Myceland Hex Grid", meta=(Categories="Puzzle"))
	FGameplayTag PuzzleID;

	UPROPERTY(EditAnywhere, Category="Myceland Hex Grid")
	TArray<AML_CameraRail*> AssociatedCameraRails;
	
	UPROPERTY(EditAnywhere, Category="Myceland Hex Grid")
	TArray<FML_WaterPath> WaterPaths;

	UPROPERTY(EditAnywhere, Category="Myceland Hex Grid")
	TSubclassOf<AML_TileBase> WaterChangeTile;
	
	UPROPERTY(BlueprintReadWrite)
    bool bIsPuzzleSolved;
	
	UFUNCTION(CallInEditor, Category="Myceland Hex Grid", meta=(DisplayName="Update Current Grid"))
	void UpdateCurrentGridEditor();

	UFUNCTION(CallInEditor, Category="Myceland Hex Grid", meta=(DisplayName="Clear & Rebuild Grid"))
	void RebuildGrid();
	
	UFUNCTION(CallInEditor, Category="Myceland Hex Grid", meta=(DisplayName="Clear Grid"))
	void ClearTiles();
	
	UFUNCTION(CallInEditor, Category="Myceland Hex Grid", meta=(DisplayName="Destroy Stale Tiles"))
	void DestroyStaleTiles() const;
	
	UFUNCTION(BlueprintCallable, Category="Myceland Hex Grid")
	TArray<AML_Tile*> GetNeighbors(AML_Tile* CenterTile);
	
	UFUNCTION(BlueprintPure, Category="Myceland Runtime")
	TMap<FIntPoint, AML_Tile*> GetGridMap() const;

	const TMap<FIntPoint, AML_Tile*>& GetGridMapRef() const;
	
	UFUNCTION(BlueprintPure, Category="Myceland Runtime")
	TArray<AML_Tile*> GetGridTiles();

	UFUNCTION(BlueprintPure, Category="Myceland Runtime")
	TArray<AML_Tile*> GetTreeTiles() const;

	UFUNCTION(BlueprintPure, Category="Myceland Hex Grid")
	AML_Tile* FindClosestWalkableBorderTile(const FVector& WorldLocation) const;

	UFUNCTION(BlueprintPure, Category="Myceland Hex Grid")
	AML_Tile* FindClosestWaterPathTile(const AML_Tile* Tile);

	UFUNCTION(BlueprintPure, Category="Myceland Hex Grid")
	AML_CameraRail* GetClosestCameraRail(const FVector& WorldLocation) const;
	
	UFUNCTION(BlueprintPure, Category="Myceland Runtime")
	int32 GetEnergyForPuzzle() const { return EnergyForPuzzle; }
	
	UFUNCTION(BlueprintPure, Category="Myceland Runtime")
	UML_BiomeTileSet* GetBiomeTileSet() const { return BiomeTileSet; }
	
	UFUNCTION(BlueprintPure, Category="Myceland Runtime")
	ACameraActor* GetAssociatedCamera() const { return AssociatedCamera; }
	
	UFUNCTION(BlueprintPure, Category="Myceland Runtime")
	AActor* GetAssociatedObstacle() const { return AssociatedObstacle; }
	
	UFUNCTION(BlueprintPure, Category="Myceland Runtime")
	ULevelSequence* GetAssociatedWinCinematic() const { return AssociatedWinCinematic; }
	
	UFUNCTION(BlueprintPure, Category="Myceland Runtime")
	AML_CinematicCameraRail* GetAssociatedStartCinematicRail() const { return AssociatedStartCinematicRail; }
	
	UFUNCTION(BlueprintPure, Category="Myceland Runtime")
	TArray<AActor*> GetAssociatedNatureZones() const { return AssociatedNatureZones; } 
	
	UFUNCTION(BlueprintPure, Category="Myceland Runtime")
	TArray<AActor*> GetAssociatedWaterPaths() const { return AssociatedWaterPaths; } 
	
	// ==================== Myceland Hex Procedural Grid ====================
	
	UPROPERTY(EditAnywhere, Category="Myceland Puzzle Generator")
	FML_PuzzleGenerationSettings PuzzleGenerationSettings;

	UFUNCTION(CallInEditor, Category="Myceland Puzzle Generator")
	void AnalyzeCurrentPuzzle();
	
	
	// Restores this board's tiles to their authored initial state and re-enables the
	// obstacle. Does NOT write to the save — call ReplayPuzzle (or ResetPuzzle on the
	// subsystem) to commit the change. Used directly for cascade resets.
	UFUNCTION(BlueprintCallable, Category="Myceland Hex Grid")
	void RestoreToInitialState();

private:

	// Resets this puzzle (and every puzzle solved after it) back to its initial state
	// and updates the save. Intended for editor and Blueprint use.
	UFUNCTION(CallInEditor, BlueprintCallable, Category="Myceland Hex Grid", meta=(DisplayName="Replay Puzzle (Reset to Initial State)"))
	void ReplayPuzzle();

	// ==================== Save / Load Helpers ====================

	// Returns a snapshot of the current GridMap as a flat array of tile entries.
	TArray<FML_TileSaveEntry> SnapshotGrid() const;

	// Fetches the SaveSubsystem from the owning GameInstance. Returns null if unavailable.
	UML_SaveSubsystem* GetSaveSubsystem() const;

	// Returns a stable level key (map name with PIE prefix stripped) used to scope
	// solve orders and cascade resets to the current level.
	FName GetLevelKey() const;

	// Bound to UML_WinLoseSubsystem::OnWin; captures the solved grid and saves to disk.
	UFUNCTION()
	void HandlePuzzleWon();
};
