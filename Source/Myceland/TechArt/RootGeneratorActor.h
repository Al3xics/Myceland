#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/DecalComponent.h"
#include "RootGeneratorActor.generated.h"

UENUM(BlueprintType)
enum class EDecalAlongAxis : uint8
{
	// Decal local Y axis follows spline tangent
	AlongY UMETA(DisplayName = "Along Y"),
	// Decal local Z axis follows spline tangent
	AlongZ UMETA(DisplayName = "Along Z")
};

/* ===================== TILE TYPE PAIR ===================== */

USTRUCT(BlueprintType)
struct FTileTypePair
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString TypeA;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString TypeB;
};

/* ===================== EDGE KEY ===================== */

struct FTileEdgeKey
{
	int32 AId = 0;
	int32 BId = 0;
	int32 Variant = 0;

	FTileEdgeKey() = default;

	FTileEdgeKey(const UObject* A, const UObject* B, int32 InVariant = 0)
	{
		const int32 IdA = A ? A->GetUniqueID() : 0;
		const int32 IdB = B ? B->GetUniqueID() : 0;
		AId = FMath::Min(IdA, IdB);
		BId = FMath::Max(IdA, IdB);
		Variant = InVariant;
	}

	bool operator==(const FTileEdgeKey& R) const
	{
		return AId == R.AId && BId == R.BId && Variant == R.Variant;
	}
};

FORCEINLINE uint32 GetTypeHash(const FTileEdgeKey& K)
{
	return HashCombine(
		HashCombine(GetTypeHash(K.AId), GetTypeHash(K.BId)),
		GetTypeHash(K.Variant)
	);
}

/* ===================== EDGE RUNTIME ===================== */

struct FEdgeRuntime
{
	TObjectPtr<USplineComponent> Spline = nullptr;

	// Mesh mode: USplineMeshComponent; Decal mode: UDecalComponent.
	// Store raw pointers to avoid TObjectPtr implicit conversion issues.
	TArray<USceneComponent*> Segments;

	TArray<TObjectPtr<USplineComponent>> BranchSplines;

	TWeakObjectPtr<AActor> TileA;
	TWeakObjectPtr<AActor> TileB;

	FTimerHandle GrowTimer;
	FTimerHandle UngrowTimer;

	int32 GrowIndex = 0;
	int32 UngrowIndex = 0;
	bool bDestroying = false;
};

/* ===================== ACTOR ===================== */

UCLASS(Blueprintable)
class MYCELAND_API ARootGeneratorActor : public AActor
{
	GENERATED_BODY()

public:
	ARootGeneratorActor();

	/* ---------- BP reflection ---------- */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|BP")
	FName TileTypeVarName = TEXT("TileType");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|BP")
	FName NeighborsVarName = TEXT("Neighbors");

	/* ---------- Branch spacing helpers ---------- */

	UPROPERTY(EditAnywhere, Category = "Roots|Branches")
	float BranchClearance = 15.f;

	UPROPERTY(EditAnywhere, Category = "Roots|Branches")
	bool bBranchOffsetsOneSided = true;

	/* ---------- Tile type filtering ---------- */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Filter")
	TArray<FString> ConnectTypes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Filter")
	bool bAllowCrossTypeConnections = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Filter")
	TArray<FTileTypePair> CrossTypeWhitelist;

	/* ---------- Density ---------- */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Density", meta = (ClampMin = "0.0"))
	float RootDensity = 1.0f;

	/* ---------- Render mode ---------- */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Render")
	bool bUseDecals = false;

	/* ---------- Mesh ---------- */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Mesh", meta = (EditCondition = "!bUseDecals", EditConditionHides))
	TObjectPtr<UStaticMesh> SegmentMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Mesh", meta = (EditCondition = "!bUseDecals", EditConditionHides))
	TObjectPtr<UMaterialInterface> SegmentMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Mesh", meta = (EditCondition = "!bUseDecals", EditConditionHides))
	TEnumAsByte<ESplineMeshAxis::Type> ForwardAxis = ESplineMeshAxis::X;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Mesh", meta = (EditCondition = "!bUseDecals", EditConditionHides))
	FVector MeshOffsetLocal = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Mesh", meta = (ClampMin = "10.0", EditCondition = "!bUseDecals", EditConditionHides))
	float SegmentLength = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Mesh", meta = (ClampMin = "0.001", EditCondition = "!bUseDecals", EditConditionHides))
	float MeshScaleXY = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Mesh", meta = (EditCondition = "!bUseDecals", EditConditionHides))
	float BranchMeshScaleMul = 0.85f;

	/* ---------- Decals ---------- */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Decals", meta = (EditCondition = "bUseDecals", EditConditionHides))
	float DecalLineForwardOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Decals", meta = (EditCondition = "bUseDecals", EditConditionHides))
	float DecalLineRightOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Decals", meta = (EditCondition = "bUseDecals", EditConditionHides))
	float DecalEndForwardOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Decals", meta = (EditCondition = "bUseDecals", EditConditionHides))
	float DecalEndRightOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Decals", meta = (EditCondition = "bUseDecals", EditConditionHides, ClampMin = "1.0"))
	float DecalTileLength = 120.0f; // match your decal "length" in world units

	// Which decal axis should run along the spline tangent.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Decals", meta = (EditCondition = "bUseDecals", EditConditionHides))
	EDecalAlongAxis DecalAlongAxis = EDecalAlongAxis::AlongY;

	// Extra roll tweak (degrees) around projection axis for quick manual alignment.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Decals", meta = (EditCondition = "bUseDecals", EditConditionHides))
	float DecalRollOffsetDeg = 0.0f;

	// If true, use 3D tangent. If false, flatten to XY (recommended for ground cracks).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Decals", meta = (EditCondition = "bUseDecals", EditConditionHides))
	bool bDecalFlattenTangentToXY = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Decals", meta = (EditCondition = "bUseDecals", EditConditionHides))
	TArray<TObjectPtr<UMaterialInterface>> LineDecalMaterials;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Decals", meta = (EditCondition = "bUseDecals", EditConditionHides))
	TArray<TObjectPtr<UMaterialInterface>> EndDecalMaterials;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Decals", meta = (EditCondition = "bUseDecals", EditConditionHides))
	FVector DecalSize = FVector(32.f, 120.f, 18.f);

	// If > 0 spacing along spline. If <= 0 spacing = DecalSize.Y
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Decals", meta = (EditCondition = "bUseDecals", EditConditionHides))
	float DecalSegmentLength = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Decals", meta = (EditCondition = "bUseDecals", EditConditionHides))
	float DecalZOffset = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Decals", meta = (EditCondition = "bUseDecals", EditConditionHides))
	int32 DecalSortOrder = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Decals", meta = (EditCondition = "bUseDecals", EditConditionHides))
	float DecalFadeScreenSize = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Decals", meta = (EditCondition = "bUseDecals", EditConditionHides))
	bool bSetDecalColor = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Decals", meta = (EditCondition = "bUseDecals && bSetDecalColor", EditConditionHides))
	FName DecalColorParamName = TEXT("Color");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Decals", meta = (EditCondition = "bUseDecals && bSetDecalColor", EditConditionHides))
	FLinearColor DecalColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Decals", meta = (EditCondition = "bUseDecals", EditConditionHides))
	float BranchDecalScaleMul = 0.85f;

	/* ---------- Endpoint offset ---------- */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Endpoints")
	bool bEnableEndpointOffset = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Endpoints", meta = (ClampMin = "0.0"))
	float EndpointOffsetRadius = 25.0f;

	/* ---------- Variation ---------- */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Variation", meta = (ClampMin = "0.0"))
	float Variation = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Variation")
	bool bDeterministicVariation = true;

	/* ---------- Spline shape ---------- */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Spline", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SplineCurviness = 0.0f;

	/* ---------- Animation ---------- */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Anim")
	float GrowStepSeconds = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Anim")
	float UngrowStepSeconds = 0.05f;

	/* ---------- Crack ---------- */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Crack", meta = (ClampMin = "2"))
	int32 PointsPerEdge = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Crack")
	float CrackWidth = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Crack")
	float CrackOndulations = 1.2f;

	/* ---------- Branch ---------- */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Branch")
	bool bEnableBranches = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Branch")
	float BranchDensityPer1000 = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Branch")
	int32 BranchCountJitter = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Branch", meta = (ClampMin = "0.0", ClampMax = "0.49"))
	float BranchStartMargin = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Branch")
	float BranchLengthMin = 220.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Branch")
	float BranchLengthMax = 520.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Branch")
	float BranchAngleMinDeg = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Branch")
	float BranchAngleMaxDeg = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Branch")
	int32 BranchPointsPerEdge = 7;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Branch")
	float BranchWidthScale = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|Branch")
	float BranchOndulationsScale = 1.25f;

	/* ---------- 2D ---------- */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|2D")
	bool bForce2D = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|2D")
	bool bUseStartZAsFixedZ = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots|2D")
	float FixedZ = 0.0f;

	/* ---------- API ---------- */

	UFUNCTION(BlueprintCallable, Category = "Roots")
	void GenerateRoots();

	UFUNCTION(BlueprintCallable, Category = "Roots")
	void NotifyTileChanged(AActor* Tile);

	UFUNCTION(BlueprintCallable, Category = "Roots")
	void ClearAll(bool bAnimated = true);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/* ---------- State ---------- */

	TMap<FTileEdgeKey, FEdgeRuntime> EdgeMap;
	TSet<TWeakObjectPtr<AActor>> ActiveTiles;

	/* ---------- Tile helpers ---------- */

	bool GetTileEnumValue(AActor* Tile, UEnum*& OutEnum, int64& OutValue) const;
	bool GetTileTypeName(AActor* Tile, FString& OutTypeName) const;
	bool IsConnectType(const FString& TypeName) const;
	bool CanConnectTypes(const FString& AType, const FString& BType) const;
	bool GetNeighbors(AActor* Tile, TArray<AActor*>& OutNeighbors) const;

	/* ---------- Tile membership ---------- */

	void AddTile(AActor* Tile);
	void RemoveTile(AActor* Tile, bool bAnimated);

	/* ---------- Edge ops ---------- */

	void AddEdgesForTile(AActor* Tile);
	void RemoveEdgesForTile(AActor* Tile, bool bAnimated);

	void CreateEdge(AActor* A, AActor* B, int32 Variant);
	void DestroyEdge(const FTileEdgeKey& Key, bool bAnimated);

	/* ---------- Timers ---------- */

	void TickGrow(FTileEdgeKey Key);
	void TickUngrow(FTileEdgeKey Key);

	/* ---------- Helpers ---------- */

	static FVector FlattenXY(const FVector& V, float Z);
	static FVector2D SafeNormal2D(const FVector2D& V);

	FVector MakeEndpointOffsetXY(const AActor* A, const AActor* B, bool bForA) const;

	// IMPORTANT: 3 params everywhere.
	FRandomStream MakeEdgeRng(const AActor* A, const AActor* B, uint32 Salt = 0) const;


	float VaryFloat(float Base, float Var, FRandomStream& RS, float MinClamp = 0.0f) const;
	int32 VaryInt(int32 Base, float Var, FRandomStream& RS, int32 MinClamp = 0) const;

	void BuildCrackPoints2D(
		const FVector& Start,
		const FVector& End,
		float Z,
		int32 NumPoints,
		float Width,
		float Ondulations,
		float Phase,
		TArray<FVector>& OutPoints,
		const FVector2D* LateralDirOverride = nullptr,
		bool bOneSided = false,
		float MinLateral = 0.0f
	) const;

	void FillSplineFromWorldPoints(USplineComponent* Spline, const TArray<FVector>& WorldPoints) const;
	void ApplyCurvinessToSpline(USplineComponent* Spline) const;

	int32 ComputeBranchCount(float MainSplineLen) const;

	void BuildSplineSegmentsHidden(FEdgeRuntime& Edge);
	void RebuildDecalsForEdgesTouching(AActor* Tile);
	int32 GetTileRootDegree(AActor* Tile) const;
	bool IsTileExtremity(AActor* Tile) const { return GetTileRootDegree(Tile) == 1; }
	bool IsTrueEndTile(AActor* Tile) const;
	UMaterialInterface* PickDecalMaterial(bool bIsLast, FRandomStream& RS) const;
	FRotator MakeGroundDecalRotationFromTangent(const FVector& WorldTangent) const;
	float GetDecalStepLength(float LocalScaleMul) const;
};
