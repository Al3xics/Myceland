// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/ML_CoreData.h"
#include "GameFramework/Actor.h"
#include "ML_BoardSpawner.generated.h"

class UML_BiomeTileSet;
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

public:
	AML_BoardSpawner();

private:
	// ==================== Myceland Runtime ====================
	
	UPROPERTY(EditInstanceOnly, Category="Myceland Runtime")
	UML_BiomeTileSet* BiomeTileSet;
	
	
	UPROPERTY(Transient)
	TArray<TObjectPtr<AML_Tile>> SpawnedTiles;
	
	TMap<FIntPoint, TObjectPtr<AML_Tile>> GridMap;
	
	// Generators
	void SpawnHexagonRadius();
	void SpawnRectangleWH();

	// Conversions
	FVector AxialToWorld(int32 Q, int32 R) const;
	FIntPoint WorldToAxial(const FVector& WorldLocation) const;
	FIntPoint OffsetToAxial(int32 Col, int32 Row) const;

protected:
	virtual void Destroyed() override;
	virtual void BeginPlay() override;

public:
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
	
	
	
	UFUNCTION(CallInEditor, Category="Myceland Hex Grid", meta=(DisplayName="Update Current Grid"))
	void UpdateCurrentGrid();

	UFUNCTION(CallInEditor, Category="Myceland Hex Grid", meta=(DisplayName="Clear & Rebuild Grid"))
	void RebuildGrid();
	
	UFUNCTION(CallInEditor, Category="Myceland Hex Grid", meta=(DisplayName="Clear Grid"))
	void ClearTiles();
	
	UFUNCTION(BlueprintCallable, Category="Myceland Hex Grid")
	TArray<AML_Tile*> GetNeighbors(AML_Tile* CenterTile);
	
	UFUNCTION(BlueprintPure, Category="Myceland Runtime")
	TMap<FIntPoint, AML_Tile*> GetGridMap() const;
	
	UFUNCTION(BlueprintPure, Category="Myceland Runtime")
	TArray<AML_Tile*> GetGridTiles();
	
	UFUNCTION(BlueprintPure, Category="Myceland Runtime")
	UML_BiomeTileSet* GetBiomeTileSet() const { return BiomeTileSet; }
};
