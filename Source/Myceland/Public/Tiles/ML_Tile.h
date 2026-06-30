// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ML_BoardSpawner.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Core/ML_CoreData.h"
#include "ML_Tile.generated.h"

class AML_TileTree;
class AML_TileBase;
class AML_TileObstacle;
class AML_TileWater;
class AML_TileParasite;
class AML_TileGrass;
class AML_TileDirt;
class AML_TileWaterPath;
class AML_BoardSpawner;
enum class EML_TileType : uint8;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTileChangedNative, AML_Tile*, Tile);

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

	UPROPERTY(EditAnywhere, Category="Myceland Tile", meta=(EditCondition="CurrentType==EML_TileType::WaterPath", EditConditionHides))
	TSubclassOf<AML_TileWaterPath> WaterPathClass;
	
	UPROPERTY(VisibleAnywhere, Category="Myceland Tile")
	FIntPoint AxialCoord = FIntPoint(0, 0);

	UPROPERTY(VisibleAnywhere, Category="Myceland Tile")
	bool bBlocked = false;
	
	UPROPERTY(VisibleAnywhere, Category="Myceland Tile")
	bool bHasCollectible = false;
	
	UPROPERTY(VisibleAnywhere, Category="Myceland Tile")
	bool bIsBorderTile = false;
	
	void SetBlocked(bool bNewBlocked);

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
	UPROPERTY(BlueprintAssignable, Category="Tile")
    FOnTileChangedNative OnTileChangedNative;
    
    UFUNCTION(BlueprintCallable, Category="Tile")
    void NotifyTileChangedNative();
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

	UFUNCTION(BlueprintCallable, Category="Myceland Tile|Getter & Setter")
	void SetBorderTile(const bool Value) { bIsBorderTile = Value; }

	UFUNCTION(BlueprintPure, Category="Myceland Tile|Getter & Setter")
	bool IsBorderTile() const { return bIsBorderTile; }

	UFUNCTION(BlueprintCallable, Category="Myceland Tile|Collectible")
	void SetHasCollectible(const bool bNewValue) { bHasCollectible = bNewValue; }
	
	UFUNCTION(BlueprintPure, Category="Myceland Tile|Collectible")
	bool HasCollectible() const { return bHasCollectible; }
	
	UFUNCTION(BlueprintPure, Category="Myceland Tile|Getter & Setter")
	AML_BoardSpawner* GetBoardSpawnerFromTile() const { return Cast<AML_BoardSpawner>(GetOwner()); }

	// bIsWalkable is false when hovering a non-walkable tile type (water, obstacle, parasite, tree).
	// The Blueprint uses it to pick a different "blocked" glow color.
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category="Myceland Tile|Feedback")
	void GlowCursorHovered(bool bIsPlayerTile, bool bIsWalkable);

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category="Myceland Tile|Feedback")
	void StopGlowingCursorUnhovered();

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category="Myceland Tile|Feedback")
	void GlowPathWalk();

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category="Myceland Tile|Feedback")
	void StopGlowingPathWalk();
	
	UFUNCTION(BlueprintImplementableEvent, Category="Myceland Tile|Feedback")
	void OnTileTypeChanged(EML_TileType OldType, EML_TileType NewType);

	UFUNCTION(BlueprintImplementableEvent, Category="Myceland Tile|Feedback")
	void OnWaveTouched();
	
	
	UFUNCTION()
	void UpdateClassAtRuntime(const EML_TileType NewTileType, TSubclassOf<AML_TileBase> NewClass);
	
	UFUNCTION()
	void Initialize(UML_BiomeTileSet* InBiomeTileSet);
	
	UFUNCTION()
	UChildActorComponent* GetTileChildActor() const { return TileChildActor; }

	UFUNCTION()
	void UpdateClassAtRuntime_Silent(EML_TileType NewTileType, TSubclassOf<AML_TileBase> NewClass);

	// Destroys any AML_TileBase actor attached to this tile that is NOT the child actor
	// currently tracked by TileChildActor. Guards against orphaned child actors left behind
	// by editor duplication / reattachment, which otherwise keep showing the previous type.
	void DestroyStaleChildActors();

	UPROPERTY(Transient)
	TWeakObjectPtr<AML_Collectible> CollectibleActor;
};
