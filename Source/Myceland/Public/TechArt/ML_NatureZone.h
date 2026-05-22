#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ML_NatureZone.generated.h"

class UBoxComponent;
class UInstancedStaticMeshComponent;
class UStaticMesh;

USTRUCT(BlueprintType)
struct FML_NatureInstanceReference
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Myceland Nature")
	TObjectPtr<UInstancedStaticMeshComponent> Component = nullptr;

	UPROPERTY(BlueprintReadOnly, Category="Myceland Nature")
	int32 InstanceIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category="Myceland Nature")
	TObjectPtr<UStaticMesh> StaticMesh = nullptr;

	UPROPERTY(BlueprintReadOnly, Category="Myceland Nature")
	FTransform WorldTransform = FTransform::Identity;

	bool IsValid() const
	{
		return Component != nullptr && InstanceIndex != INDEX_NONE;
	}
};

USTRUCT(BlueprintType)
struct FML_NatureCachedInstance
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Myceland Nature")
	FML_NatureInstanceReference InstanceRef;

	UPROPERTY(BlueprintReadOnly, Category="Myceland Nature")
	FVector OriginalScale = FVector::OneVector;
};

UCLASS(Blueprintable)
class MYCELAND_API AML_NatureZone : public AActor
{
	GENERATED_BODY()

public:
	AML_NatureZone();

	virtual void Tick(float DeltaSeconds) override;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Myceland Nature")
	TObjectPtr<USceneComponent> SceneRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Myceland Nature")
	TObjectPtr<UBoxComponent> DetectionBox = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Myceland Nature")
	TArray<TObjectPtr<UStaticMesh>> TargetMeshes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Myceland Nature")
	bool bUsePreciseRotatedBoxCheck = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Myceland Nature|Growth")
	float HiddenScale = 0.01f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Myceland Nature|Growth")
	float GrowthDuration = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Myceland Nature|Growth")
	bool bUseSmoothStepGrowth = true;

	UPROPERTY(BlueprintReadOnly, Category="Myceland Nature|Growth")
	bool bIsGrowing = false;

	UPROPERTY(BlueprintReadOnly, Category="Myceland Nature|Growth")
	TArray<FML_NatureCachedInstance> CachedInstances;

	UFUNCTION(BlueprintCallable, Category="Myceland Nature")
	TArray<FML_NatureInstanceReference> GetFoliageInstancesInsideBox() const;

	UFUNCTION(BlueprintCallable, Category="Myceland Nature")
	bool SetFoliageInstanceScale(
		const FML_NatureInstanceReference& InstanceRef,
		FVector NewScale,
		bool bMarkRenderStateDirty = true
	);

	UFUNCTION(BlueprintCallable, Category="Myceland Nature")
	int32 SetFoliageInstancesScale(
		const TArray<FML_NatureInstanceReference>& InstanceRefs,
		FVector NewScale
	);

	UFUNCTION(BlueprintCallable, Category="Myceland Nature|Growth")
	int32 CacheFoliageInstancesInsideBox();

	UFUNCTION(BlueprintCallable, Category="Myceland Nature|Growth")
	int32 CacheAndHideFoliageInstancesInsideBox();

	UFUNCTION(BlueprintCallable, Category="Myceland Nature|Growth")
	void StartGrowCachedFoliage();

	UFUNCTION(BlueprintCallable, Category="Myceland Nature|Growth")
	int32 HideCachedFoliage();

	UFUNCTION(BlueprintCallable, Category="Myceland Nature|Growth")
	void ClearCachedFoliage();

private:
	float GrowthElapsed = 0.0f;

	bool ShouldAcceptMesh(const UStaticMesh* Mesh) const;
	bool IsWorldLocationInsideDetectionBox(const FVector& WorldLocation) const;
	void ApplyGrowthAlpha(float Alpha);
};