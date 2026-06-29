// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ML_LakeFishManager.generated.h"

class AML_Tile;
class AML_BoardSpawner;
class USplineComponent;
class AML_TileWater;
enum class EML_TileType : uint8;

USTRUCT()
struct FML_LakeFishRuntime
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<AActor> Fish = nullptr;
	UPROPERTY()
	FVector CurrentTargetLocation = FVector::ZeroVector;
	UPROPERTY()
	float FishZOffsetRandom = 0.f;
	UPROPERTY()
	FVector LastMoveDirection = FVector::ForwardVector;
	UPROPERTY()
	bool bForward = true;
	
	UPROPERTY()
	TObjectPtr<USplineComponent> Spline = nullptr;

	UPROPERTY()
	float Distance = 0.f;

	UPROPERTY()
	float SpeedMultiplier = 1.f;

	UPROPERTY()
	float WavePhase = 0.f;
};

USTRUCT()
struct FML_LakeRuntime
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<TObjectPtr<AML_Tile>> Tiles;

	UPROPERTY()
	TArray<TObjectPtr<USplineComponent>> Splines;

	UPROPERTY()
	TArray<FML_LakeFishRuntime> Fish;
};

UCLASS(Blueprintable)
class MYCELAND_API AML_LakeFishManager : public AActor
{
	GENERATED_BODY()

public:
	AML_LakeFishManager();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Myceland Lake Fish")
	TSubclassOf<AActor> FishClass;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Myceland Lake Fish")
	float FishZOffsetRandomRange = 8.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Myceland Lake Fish", meta = (ClampMin = "0.0"))
	float FishRotationInterpSpeed = 4.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Myceland Lake Fish")
	float FishTargetAcceptanceRadius = 25.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Myceland Lake Fish", meta=(ClampMin="0.0"))
	float FishPerWaterTile = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Myceland Lake Fish", meta=(ClampMin="0"))
	int32 MinFishPerLake = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Myceland Lake Fish", meta=(ClampMin="0"))
	int32 MaxFishPerLake = 60;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Myceland Lake Fish", meta=(ClampMin="1.0"))
	float FishSpeed = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Myceland Lake Fish", meta=(ClampMin="0.0"))
	float FishSpeedRandomRange = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Myceland Lake Fish", meta=(ClampMin="0.0"))
	float UndulationAmplitude = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Myceland Lake Fish", meta=(ClampMin="0.0"))
	float UndulationFrequency = 0.035f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Myceland Lake Fish")
    float FishZOffset = 20.f;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Myceland Lake Fish")
    float SplineZOffset = 20.f;
	FVector PickRandomLakeTarget(const FML_LakeRuntime& Lake, const FML_LakeFishRuntime& FishData) const;
	void AssignNewFishTarget(FML_LakeFishRuntime& FishData, const FML_LakeRuntime& Lake);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Myceland Lake Fish")
	bool bRotateFishAlongSpline = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Myceland Lake Fish")
	bool bDebugDrawSplines = false;

	UFUNCTION(BlueprintCallable, Category="Myceland Lake Fish")
	void RebuildLakes();

private:
	UPROPERTY()
	TObjectPtr<USceneComponent> SceneRoot = nullptr;

	UPROPERTY()
	TArray<TObjectPtr<AML_BoardSpawner>> BoardSpawners;

	UPROPERTY()
	TArray<TObjectPtr<AML_Tile>> CachedTiles;

	UPROPERTY()
	TArray<FML_LakeRuntime> Lakes;

	UFUNCTION()
	void HandleTileTypeChanged(AML_Tile* Tile);

	void GatherBoardsAndTiles();
	void BindTileEvents();
	void CleanupInvalidCachedTiles();

	bool IsWaterTile(const AML_Tile* Tile) const;
	bool IsGrassTile(const AML_Tile* Tile) const;
	bool IsTileAliveWater(const AML_Tile* Tile) const;
	bool IsTreeTile(const AML_Tile* Tile) const;
	void FloodFillWaterLake(AML_Tile* StartTile, TSet<AML_Tile*>& Visited, TArray<AML_Tile*>& OutTiles) const;
	bool DoesLakeTouchGrass(const TArray<AML_Tile*>& LakeTiles) const;

	void BuildLakeSplines(FML_LakeRuntime& Lake);
	void ReconcileFish(FML_LakeRuntime& Lake, TArray<FML_LakeFishRuntime>& ReusableFish);
	int32 GetTargetFishCount(const FML_LakeRuntime& Lake) const;

	USplineComponent* CreateSplineForTilePair(AML_Tile* A, AML_Tile* B);
	USplineComponent* PickSpline(const FML_LakeRuntime& Lake) const;
	
	void PlaceFishOnSpline(FML_LakeFishRuntime& FishData, bool bRandomizeDistance);
	void InitializeLakeManager();
	void DestroyLake(FML_LakeRuntime& Lake);
	void DestroyFish(FML_LakeFishRuntime& FishData);
	void DestroySpline(USplineComponent* Spline);
	AML_TileWater* GetWaterActorFromTile(const AML_Tile* Tile) const;
	void AssignClosestSplineWithoutTeleport(const FML_LakeRuntime& Lake, FML_LakeFishRuntime& FishData);
	
	static FString MakeLakeKey(const TArray<AML_Tile*>& Tiles);
	static bool LakesOverlap(const FML_LakeRuntime& ExistingLake, const TArray<AML_Tile*>& NewTiles);
};
