// RootGeneratorActor.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "RootGeneratorActor.generated.h"

// Plain C++ key
struct FTileEdgeKey
{
	int32 AId = 0;
	int32 BId = 0;

	FTileEdgeKey() = default;

	FTileEdgeKey(const UObject* A, const UObject* B)
	{
		const int32 IdA = A ? A->GetUniqueID() : 0;
		const int32 IdB = B ? B->GetUniqueID() : 0;
		AId = FMath::Min(IdA, IdB);
		BId = FMath::Max(IdA, IdB);
	}

	bool operator==(const FTileEdgeKey& R) const
	{
		return AId == R.AId && BId == R.BId;
	}
};

FORCEINLINE uint32 GetTypeHash(const FTileEdgeKey& K)
{
	return HashCombine(GetTypeHash(K.AId), GetTypeHash(K.BId));
}

// Plain C++ runtime
struct FEdgeRuntime
{
	TObjectPtr<USplineComponent> Spline = nullptr;
	TArray<TObjectPtr<USplineMeshComponent>> Meshes;

	TArray<TObjectPtr<USplineComponent>> BranchSplines;

	TWeakObjectPtr<AActor> TileA;
	TWeakObjectPtr<AActor> TileB;

	FTimerHandle GrowTimer;
	FTimerHandle UngrowTimer;

	int32 GrowIndex = 0;
	int32 UngrowIndex = 0;
	bool bDestroying = false;
};

UCLASS(Blueprintable)
class MYCELAND_API ARootGeneratorActor : public AActor
{
	GENERATED_BODY()

public:
	ARootGeneratorActor();

	// BP var names on BP_Tile
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|BP")
	FName TileTypeVarName = TEXT("TileType");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|BP")
	FName NeighborsVarName = TEXT("Neighbors");

	// BP enum entry name you see, ex: Dirt
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Filter")
	FString TargetTileTypeDisplayName = TEXT("Dirt");

	// Mesh
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Mesh")
	TObjectPtr<UStaticMesh> SegmentMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Mesh")
	TObjectPtr<UMaterialInterface> SegmentMaterial = nullptr;

	// Which axis is "along the spline" for your mesh
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Mesh")
	TEnumAsByte<ESplineMeshAxis::Type> ForwardAxis = ESplineMeshAxis::X;

	// Offset applied to each spline mesh in its LOCAL space
	// X = along spline, Y = right, Z = up
	// Use this to fix "mesh shifted to left" due to pivot.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Endpoints")
	bool bEnableEndpointOffset = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Endpoints", meta = (ClampMin = "0.0"))
	float EndpointOffsetRadius = 25.0f; // units in XY
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Variation", meta = (ClampMin = "0.0"))
	float Variation = 0.0f; // 0 = no deviation. In same units as the value (width units, degrees, etc)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Variation")
	bool bDeterministicVariation = true; // stable per edge (recommended)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Mesh")
	FVector MeshOffsetLocal = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Mesh", meta = (ClampMin = "10.0"))
	float SegmentLength = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Mesh", meta = (ClampMin = "0.001"))
	float MeshScaleXY = 0.15f;

	// Anim
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Anim", meta = (ClampMin = "0.0"))
	float GrowStepSeconds = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Anim", meta = (ClampMin = "0.0"))
	float UngrowStepSeconds = 0.05f;

	// Crack spline shape
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Crack", meta = (ClampMin = "2"))
	int32 PointsPerEdge = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Crack", meta = (ClampMin = "0.0"))
	float CrackWidth = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Crack", meta = (ClampMin = "0.0"))
	float CrackOndulations = 1.2f;

	// Branching
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Branch")
	bool bEnableBranches = true;

	// Branches per 1000 units of main spline length
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Branch", meta = (ClampMin = "0.0"))
	float BranchDensityPer1000 = 1.6f;

	// Adds +- jitter to computed branch count
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Branch", meta = (ClampMin = "0"))
	int32 BranchCountJitter = 1;

	// Avoid spawning too close to ends, in normalized 0..1 along spline
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Branch", meta = (ClampMin = "0.0", ClampMax = "0.49"))
	float BranchStartMargin = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Branch", meta = (ClampMin = "0.0"))
	float BranchLengthMin = 220.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Branch", meta = (ClampMin = "0.0"))
	float BranchLengthMax = 520.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Branch", meta = (ClampMin = "0.0"))
	float BranchAngleMinDeg = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Branch", meta = (ClampMin = "0.0"))
	float BranchAngleMaxDeg = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Branch", meta = (ClampMin = "2"))
	int32 BranchPointsPerEdge = 7;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Branch", meta = (ClampMin = "0.0"))
	float BranchWidthScale = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Branch", meta = (ClampMin = "0.0"))
	float BranchOndulationsScale = 1.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Branch", meta = (ClampMin = "0.0"))
	float BranchMeshScaleMul = 0.85f;

	// 2D plane
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|2D")
	bool bForce2D = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|2D")
	bool bUseStartZAsFixedZ = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|2D")
	float FixedZ = 0.0f;

	UFUNCTION(BlueprintCallable, Category = "Roots")
	void GenerateRoots();

	UFUNCTION(BlueprintCallable, Category = "Roots")
	void NotifyTileChanged(AActor* Tile);

	UFUNCTION(BlueprintCallable, Category = "Roots")
	void ClearAll(bool bAnimated = true);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	TMap<FTileEdgeKey, FEdgeRuntime> EdgeMap;
	TSet<TWeakObjectPtr<AActor>> ActiveTiles;

	// Reflection
	bool GetTileEnumValue(AActor* Tile, UEnum*& OutEnum, int64& OutValue) const;
	bool IsTargetTile(AActor* Tile) const;
	bool GetNeighbors(AActor* Tile, TArray<AActor*>& OutNeighbors) const;

	// Tile membership
	void AddTile(AActor* Tile);
	void RemoveTile(AActor* Tile, bool bAnimated);

	// Edge ops
	void AddEdgesForTile(AActor* Tile);
	void RemoveEdgesForTile(AActor* Tile, bool bAnimated);

	void CreateEdge(AActor* A, AActor* B);
	void DestroyEdge(const FTileEdgeKey& Key, bool bAnimated);
	FVector MakeEndpointOffsetXY(const AActor* A, const AActor* B, bool bForA) const;
	// Timers
	void TickGrow(FTileEdgeKey Key);
	void TickUngrow(FTileEdgeKey Key);

	// Spline helpers
	static FVector FlattenXY(const FVector& V, float Z);
	static FVector2D SafeNormal2D(const FVector2D& V);

	void BuildCrackPoints2D(
		const FVector& Start,
		const FVector& End,
		float Z,
		int32 NumPoints,
		float Width,
		float Ondulations,
		float Phase,
		TArray<FVector>& OutPoints) const;

	void FillSplineFromWorldPoints(USplineComponent* Spline, const TArray<FVector>& WorldPoints) const;

	// Branch helpers
	int32 ComputeBranchCount(float MainSplineLen) const;

	// Mesh building
	void BuildSplineMeshesHidden(FEdgeRuntime& Edge);
};
