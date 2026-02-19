// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ML_BoardSpawner.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Core/ML_CoreData.h"
#include "ML_Tile.generated.h"

class AML_TileTree;
class AML_TileObstacle;
class AML_TileWater;
class AML_TileParasite;
class AML_TileGrass;
class AML_TileDirt;
class AML_BoardSpawner;
enum class EML_TileType : uint8;

UCLASS(Blueprintable)
class MYCELAND_API AML_Tile : public AActor
{
	GENERATED_BODY()

private:
	UPROPERTY(EditInstanceOnly, Category="Myceland Tile")
	EML_TileType CurrentType = EML_TileType::Dirt;
	
	UPROPERTY(EditAnywhere, Category="Myceland Tile", meta=(EditCondition="CurrentType==EML_TileType::Dirt", EditConditionHides))
	TSubclassOf<AML_TileDirt> DirtClass;
	
	UPROPERTY(EditAnywhere, Category="Myceland Tile", meta=(EditCondition="CurrentType==EML_TileType::Grass", EditConditionHides))
	TSubclassOf<AML_TileGrass> GrassClass;
	
	UPROPERTY(EditAnywhere, Category="Myceland Tile", meta=(EditCondition="CurrentType==EML_TileType::Parasite", EditConditionHides))
	TSubclassOf<AML_TileParasite> ParasiteClass;
	
	UPROPERTY(EditAnywhere, Category="Myceland Tile", meta=(EditCondition="CurrentType==EML_TileType::Water", EditConditionHides))
	TSubclassOf<AML_TileWater> WaterClass;
	
	UPROPERTY(EditAnywhere, Category="Myceland Tile", meta=(EditCondition="CurrentType==EML_TileType::Obstacle", EditConditionHides))
	TSubclassOf<AML_TileObstacle> ObstacleClass;
	
	UPROPERTY(EditAnywhere, Category="Myceland Tile", meta=(EditCondition="CurrentType==EML_TileType::Tree", EditConditionHides))
	TSubclassOf<AML_TileTree> TreeClass;
	
	UPROPERTY(VisibleAnywhere, Category="Myceland Tile")
	FIntPoint AxialCoord = FIntPoint(0, 0);

	UPROPERTY(VisibleAnywhere, Category="Myceland Tile")
	bool bBlocked = false;
	
	UPROPERTY(VisibleAnywhere, Category="Myceland Tile")
	bool bHasCollectible = false;
	
	void SetBlocked(bool bNewBlocked);
	bool IsTileTypeBlocking(EML_TileType Type);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Myceland Tile")
	USceneComponent* SceneRoot;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Myceland Tile")
	UChildActorComponent* TileChildActor;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Myceland Tile")
	UStaticMeshComponent* HighlightTileMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Myceland Tile")
	UStaticMeshComponent* HexagonCollision;
	
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	void UpdateClassInEditor(EML_TileType NewTileType);
#endif

public:
	bool bConsumedGrass = false;
	
	AML_Tile();

	UFUNCTION(BlueprintCallable, Category="Myceland Tile|Getter & Setter")
	void SetAxialCoord(const FIntPoint& InAxial) { AxialCoord = InAxial; }

	UFUNCTION(BlueprintPure, Category="Myceland Tile|Getter & Setter")
	FIntPoint GetAxialCoord() const { return AxialCoord; }

	UFUNCTION(BlueprintCallable, Category="Myceland Tile|Getter & Setter")
	void SetCurrentType(const EML_TileType NewType) { CurrentType = NewType; }

	UFUNCTION(BlueprintPure, Category="Myceland Tile|Getter & Setter")
	EML_TileType GetCurrentType() const { return CurrentType; }

	UFUNCTION(BlueprintPure, Category="Myceland Tile|Getter & Setter")
	bool IsBlocked() const { return bBlocked; }

	UFUNCTION(BlueprintCallable, Category="Myceland Tile|Collectible")
	void SetHasCollectible(const bool bNewValue) { bHasCollectible = bNewValue; }
	
	UFUNCTION(BlueprintPure, Category="Myceland Tile|Collectible")
	bool HasCollectible() const { return bHasCollectible; }
	
	UFUNCTION(BlueprintPure, Category="Myceland Tile|Getter & Setter")
	AML_BoardSpawner* GetBoardSpawnerFromTile() const { return Cast<AML_BoardSpawner>(GetOwner()); }

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category="Myceland Tile|Feedback")
	void Glow();

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category="Myceland Tile|Feedback")
	void StopGlowing();
	
	UFUNCTION(BlueprintImplementableEvent, Category="Myceland Tile|Feedback")
	void OnTileTypeChanged(EML_TileType OldType, EML_TileType NewType);
	
	
	UFUNCTION()
	void UpdateClassAtRuntime(const EML_TileType NewTileType, TSubclassOf<AML_TileBase> NewClass);
	
	UFUNCTION()
	void Initialize(UML_BiomeTileSet* InBiomeTileSet);
	
	UFUNCTION()
	UChildActorComponent* GetTileChildActor() const { return TileChildActor; }
};
