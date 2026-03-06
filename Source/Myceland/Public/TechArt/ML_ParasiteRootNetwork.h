#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SplineMeshComponent.h"
#include "ML_ParasiteRootNetwork.generated.h"

class AML_Tile;
class AML_TileParasite;
class USceneComponent;
class USplineComponent;
class UStaticMesh;
class UMaterialInterface;

USTRUCT()
struct FML_ParasiteTilePairKey
{
	GENERATED_BODY()

	TWeakObjectPtr<AML_Tile> TileA;
	TWeakObjectPtr<AML_Tile> TileB;

	FML_ParasiteTilePairKey() {}

	FML_ParasiteTilePairKey(AML_Tile* InA, AML_Tile* InB)
	{
		if (InA <= InB)
		{
			TileA = InA;
			TileB = InB;
		}
		else
		{
			TileA = InB;
			TileB = InA;
		}
	}

	bool operator==(const FML_ParasiteTilePairKey& Other) const
	{
		return TileA == Other.TileA && TileB == Other.TileB;
	}

	friend uint32 GetTypeHash(const FML_ParasiteTilePairKey& Key)
	{
		return HashCombine(GetTypeHash(Key.TileA), GetTypeHash(Key.TileB));
	}
};

USTRUCT()
struct FML_ParasiteConnectionRecord
{
	GENERATED_BODY()

	FGuid ConnectionId;

	TWeakObjectPtr<AML_Tile> TileA;
	TWeakObjectPtr<AML_Tile> TileB;

	TWeakObjectPtr<USceneComponent> ExtremityA;
	TWeakObjectPtr<USceneComponent> ExtremityB;

	UPROPERTY()
	TObjectPtr<USplineComponent> SplineComponent;

	UPROPERTY()
	TArray<TObjectPtr<USplineMeshComponent>> MeshSegments;

	int32 VisibleSegments = 0;
	int32 TotalSegments = 0;

	UStaticMesh* ChosenMesh = nullptr;
};

USTRUCT()
struct FML_ParasiteConnectionBuildRequest
{
	GENERATED_BODY()

	TWeakObjectPtr<AML_Tile> TileA;
	TWeakObjectPtr<AML_Tile> TileB;
	TWeakObjectPtr<USceneComponent> ExtremityA;
	TWeakObjectPtr<USceneComponent> ExtremityB;
};

UCLASS()
class MYCELAND_API AML_ParasiteRootNetwork : public AActor
{
	GENERATED_BODY()

public:
	AML_ParasiteRootNetwork();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:

	UPROPERTY(VisibleAnywhere)
	USceneComponent* SceneRoot;

	UPROPERTY(EditAnywhere)
	bool bBuildAtBeginPlay = true;

	UPROPERTY(EditAnywhere)
	bool bRebuildAllOnBeginPlay = true;

	UPROPERTY(EditAnywhere, meta = (ClampMin = "1", ClampMax = "3"))
	int32 MinConnectionsPerNeighbor = 1;

	UPROPERTY(EditAnywhere, meta = (ClampMin = "1", ClampMax = "3"))
	int32 MaxConnectionsPerNeighbor = 3;

	UPROPERTY(EditAnywhere)
	float DelayBetweenNewConnections = 0.08f;

	UPROPERTY(EditAnywhere)
	float DelayBetweenSegmentReveal = 0.03f;

	UPROPERTY(EditAnywhere)
	int32 SplinePointCount = 8;

	UPROPERTY(EditAnywhere)
	int32 MeshSegmentsPerConnection = 10;

	UPROPERTY(EditAnywhere)
	int32 OndulationCount = 3;

	UPROPERTY(EditAnywhere)
	float OndulationAmplitude = 60.f;

	UPROPERTY(EditAnywhere)
	UCurveFloat* AmplitudeAlongSplineCurve;

	UPROPERTY(EditAnywhere)
	float TangentStrength = 120.f;

	UPROPERTY(EditAnywhere)
	float RandomSeedOffset = 0.f;

	UPROPERTY(EditAnywhere)
	FVector WorldUpVector = FVector::UpVector;

	UPROPERTY(EditAnywhere)
	bool bUseRoll = false;

	UPROPERTY(EditAnywhere)
	float StartRollDegrees = 0.f;

	UPROPERTY(EditAnywhere)
	float EndRollDegrees = 0.f;

	UPROPERTY(EditAnywhere)
	TArray<UStaticMesh*> RootMeshes;

	UPROPERTY(EditAnywhere)
	UMaterialInterface* RootMaterial;

	UPROPERTY(EditAnywhere)
	FVector2D MeshForwardAxisScale = FVector2D(1, 1);

	UPROPERTY(EditAnywhere)
	TEnumAsByte<enum ESplineMeshAxis::Type> ForwardAxis = ESplineMeshAxis::X;

	void RebuildEntireNetwork();
	void ClearAllRoots();

	void RegisterParasiteActor(AActor* ParasiteActor);
	void UnregisterParasiteActor(AActor* ParasiteActor);

private:

	TSet<TWeakObjectPtr<USceneComponent>> UsedExtremities;
	TArray<FML_ParasiteConnectionBuildRequest> PendingBuildRequests;

	TMap<FGuid, FML_ParasiteConnectionRecord> ActiveConnections;
	TMap<TWeakObjectPtr<AML_Tile>, TArray<FGuid>> TileToConnections;

	FTimerHandle BuildQueueTimerHandle;
	FTimerHandle SegmentRevealTimerHandle;

private:

	AML_Tile* FindTileFromParasiteActor(AActor* ParasiteActor) const;
	AML_TileParasite* GetParasiteChildFromTile(AML_Tile* Tile) const;

	bool IsParasiteTile(AML_Tile* Tile) const;

	TArray<AML_Tile*> GetParasiteNeighbors(AML_Tile* Tile) const;
	TArray<USceneComponent*> GetAvailableExtremities(AML_Tile* Tile) const;

	void RebuildAroundTile(AML_Tile* ChangedTile);

	void RemoveConnectionsForTileSet(const TSet<AML_Tile*>& TilesToClear);
	void EnqueueConnectionsForTileSet(const TSet<AML_Tile*>& TilesToBuild);

	void TryBuildQueuedConnections();
	void BuildConnectionNow(const FML_ParasiteConnectionBuildRequest& Request);
	void RevealNextSegments();

	bool IsPairAlreadyConnected(AML_Tile* TileA, AML_Tile* TileB, USceneComponent* ExtremityA, USceneComponent* ExtremityB) const;

	int32 GetDesiredConnectionCountForPair(
		const TArray<USceneComponent*>& ExtremitiesA,
		const TArray<USceneComponent*>& ExtremitiesB
	) const;

	void GatherBestConnectionsForPair(
		AML_Tile* TileA,
		AML_Tile* TileB,
		const TArray<USceneComponent*>& ExtremitiesA,
		const TArray<USceneComponent*>& ExtremitiesB,
		TArray<FML_ParasiteConnectionBuildRequest>& OutRequests
	) const;

	UStaticMesh* ChooseRandomMesh() const;

	void MarkExtremityUsed(USceneComponent* Extremity);
	void MarkExtremityFree(USceneComponent* Extremity);

	void RegisterConnectionOnTile(AML_Tile* Tile, const FGuid& ConnectionId);
	void UnregisterConnectionOnTile(AML_Tile* Tile, const FGuid& ConnectionId);

	FVector ComputeSplinePoint(
		const FVector& Start,
		const FVector& End,
		const FVector& RightVector,
		float Alpha,
		float UniquePhase
	) const;

	float GetAmplitudeAlpha(float Alpha) const;
};