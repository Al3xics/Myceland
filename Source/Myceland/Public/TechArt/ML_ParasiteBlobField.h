// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ML_ParasiteBlobField.generated.h"

class AML_Tile;
class AML_BoardSpawner;
class UMaterialInterface;
class UProceduralMeshComponent;
class USceneComponent;

struct FML_ParasiteBridgeKey
{
	AML_Tile* A = nullptr;
	AML_Tile* B = nullptr;

	FML_ParasiteBridgeKey() = default;

	FML_ParasiteBridgeKey(AML_Tile* InA, AML_Tile* InB)
	{
		if (InA < InB)
		{
			A = InA;
			B = InB;
		}
		else
		{
			A = InB;
			B = InA;
		}
	}

	bool operator==(const FML_ParasiteBridgeKey& Other) const
	{
		return A == Other.A && B == Other.B;
	}
};

FORCEINLINE uint32 GetTypeHash(const FML_ParasiteBridgeKey& Key)
{
	return HashCombine(::GetTypeHash(Key.A), ::GetTypeHash(Key.B));
}

UCLASS()
class MYCELAND_API AML_ParasiteBlobField : public AActor
{
	GENERATED_BODY()

public:
	AML_ParasiteBlobField();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Parasite Blob")
	USceneComponent* SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Parasite Blob")
	UProceduralMeshComponent* ProceduralMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parasite Blob|Material")
	UMaterialInterface* BlobMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parasite Blob|General")
	bool bGenerateInEditor = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parasite Blob|General")
	bool bAnimateAtRuntime = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parasite Blob|General", meta = (ClampMin = "0.01"))
	float ScanInterval = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parasite Blob|General", meta = (ClampMin = "0"))
	int32 NoiseSeed = 1337;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parasite Blob|General")
	float ZOffset = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parasite Blob|Animation", meta = (ClampMin = "0.01"))
	float BlobGrowSpeed = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parasite Blob|Animation", meta = (ClampMin = "0.01"))
	float BlobShrinkSpeed = 2.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parasite Blob|Animation", meta = (ClampMin = "0.01"))
	float BridgeGrowSpeed = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parasite Blob|Animation", meta = (ClampMin = "0.01"))
	float BridgeShrinkSpeed = 2.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parasite Blob|Blob Shape", meta = (ClampMin = "1.0"))
	float BlobRadius = 78.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parasite Blob|Blob Shape", meta = (ClampMin = "0.0"))
	float CenterHeight = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parasite Blob|Blob Shape", meta = (ClampMin = "0.0"))
	float InnerRingHeight = 15.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parasite Blob|Blob Shape", meta = (ClampMin = "0.05", ClampMax = "0.95"))
	float InnerRingRadiusFactor = 0.42f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parasite Blob|Blob Shape", meta = (ClampMin = "0.05", ClampMax = "0.95"))
	float MidInsetFactor = 0.66f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parasite Blob|Blob Shape", meta = (ClampMin = "0.0"))
	float ConnectedArmExtraLength = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parasite Blob|Blob Shape", meta = (ClampMin = "0.0"))
	float UnconnectedArmExtraLength = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parasite Blob|Blob Shape", meta = (ClampMin = "0.0"))
	float EdgeDrop = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parasite Blob|Blob Shape", meta = (ClampMin = "0.0"))
	float RandomRadiusJitter = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parasite Blob|Blob Shape", meta = (ClampMin = "0.0"))
	float RandomHeightJitter = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parasite Blob|Blob Shape", meta = (ClampMin = "0.0"))
	float EdgeUndulationAmplitude = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parasite Blob|Blob Shape", meta = (ClampMin = "0.0"))
	float EdgeUndulationSecondary = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parasite Blob|Blob Shape", meta = (ClampMin = "0.0"))
	float DeadSpreadFactor = 1.18f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parasite Blob|Bridge", meta = (ClampMin = "2"))
	int32 BridgeSegments = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parasite Blob|Bridge", meta = (ClampMin = "0.0"))
	float BridgeMidWidth = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parasite Blob|Bridge", meta = (ClampMin = "0.0"))
	float BridgeEndWidth = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parasite Blob|Bridge", meta = (ClampMin = "0.0"))
	float BridgeArchHeight = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parasite Blob|Bridge", meta = (ClampMin = "0.0"))
	float BridgeWaveAmplitude = 9.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parasite Blob|Bridge", meta = (ClampMin = "0.0"))
	float BridgeWidthNoiseAmplitude = 0.20f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parasite Blob|Bridge", meta = (ClampMin = "0.0"))
	float BridgeEndLift = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parasite Blob|Bridge", meta = (ClampMin = "0.0"))
	float UVScale = 100.f;

	UFUNCTION(CallInEditor, Category = "Parasite Blob")
	void RebuildParasiteBlobs();

	UFUNCTION(CallInEditor, Category = "Parasite Blob")
	void ClearBlobMesh();

private:
	struct FTileAnimState
	{
		TWeakObjectPtr<AML_Tile> Tile;
		float Alpha = 0.f;
		float TargetAlpha = 0.f;
	};

	struct FBridgeAnimState
	{
		FML_ParasiteBridgeKey Key;
		float Alpha = 0.f;
		float TargetAlpha = 0.f;
	};

	struct FTileBlobData
	{
		TWeakObjectPtr<AML_Tile> Tile;
		TWeakObjectPtr<AML_BoardSpawner> Board;
		FVector CenterWS = FVector::ZeroVector;
		FVector TipWS[6];
	};

private:
	TMap<AML_Tile*, FTileAnimState> TileStates;
	TMap<FML_ParasiteBridgeKey, FBridgeAnimState> BridgeStates;

	float ScanAccumulator = 0.f;
	bool bInitializedRuntime = false;

private:
	void RefreshTargetsFromWorld();
	bool UpdateAnimation(float DeltaSeconds);
	void SnapToCurrentTargets();
	void RebuildMeshFromState();

	void AddBlobForTile(
		AML_Tile* Tile,
		float TileAlpha,
		TArray<FVector>& Vertices,
		TArray<int32>& Triangles,
		TArray<FVector2D>& UVs,
		FTileBlobData& OutBlobData
	) const;

	void AddBridgeBetweenTiles(
		const FTileBlobData& A,
		AML_Tile* TileA,
		int32 SideA,
		const FTileBlobData& B,
		AML_Tile* TileB,
		int32 SideB,
		float BridgeAlpha,
		TArray<FVector>& Vertices,
		TArray<int32>& Triangles,
		TArray<FVector2D>& UVs
	) const;

	int32 FindSideIndexToNeighbor(AML_Tile* FromTile, AML_Tile* ToTile) const;
	FVector GetDirectionVectorForSide(AML_Tile* Tile, int32 SideIndex) const;
	float GetBridgeAlpha(AML_Tile* TileA, AML_Tile* TileB) const;

	FML_ParasiteBridgeKey MakeBridgeKey(AML_Tile* A, AML_Tile* B) const;
	bool ShouldCreateBridge(AML_Tile* A, AML_Tile* B) const;
	bool IsParasiteTile(AML_Tile* Tile) const;

	float StableRand01(int32 X, int32 Y, int32 Salt) const;
	float MoveAlphaTowards(float Current, float Target, float Speed, float DeltaSeconds) const;
	FVector WorldToLocalPosition(const FVector& WorldPos) const;
	int32 AddVertex(TArray<FVector>& Vertices, TArray<FVector2D>& UVs, const FVector& Position, const FVector2D& UV) const;
	void AddTriangle(TArray<int32>& Triangles, int32 A, int32 B, int32 C) const;
};