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

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UInstancedStaticMeshComponent> Component = nullptr;

	UPROPERTY(BlueprintReadOnly)
	int32 InstanceIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UStaticMesh> StaticMesh = nullptr;

	UPROPERTY(BlueprintReadOnly)
	FTransform WorldTransform;

	bool IsValid() const
	{
		return Component.Get() != nullptr && InstanceIndex != INDEX_NONE;
	}
};

USTRUCT(BlueprintType)
struct FML_NatureCachedInstance
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FML_NatureInstanceReference InstanceRef;

	UPROPERTY(BlueprintReadOnly)
	FVector OriginalScale = FVector::OneVector;

	UPROPERTY(BlueprintReadOnly)
	float GrowthDelay = 0.0f;
};
struct FML_GrowthAlphaSample
{
	float Time = 0.0f;
	float Alpha = 0.0f;
};
UCLASS()
class MYCELAND_API AML_NatureZone : public AActor
{
	GENERATED_BODY()

public:
	AML_NatureZone();

	virtual void Tick(float DeltaSeconds) override;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Nature Zone|Growth")
	float MinGrowthDelay = 0.01f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Nature Zone|Growth")
	float MaxGrowthDelay = 0.1f;

	TArray<FML_GrowthAlphaSample> GrowthAlphaHistory;

	float GrowthStartTime = -1.0f;

	float GetDelayedGrowthAlpha(float CurrentTime, float Delay) const;
	UFUNCTION(BlueprintCallable, Category="Nature Zone|Foliage")
	TArray<FML_NatureInstanceReference> GetFoliageInstancesInsideBox() const;

	UFUNCTION(BlueprintCallable, Category="Nature Zone|Foliage")
	TArray<FML_NatureInstanceReference> GetMaterialTargetFoliageInstancesInsideBox() const;

	UFUNCTION(BlueprintCallable, Category="Nature Zone|Foliage")
	bool SetFoliageInstanceScale(const FML_NatureInstanceReference& InstanceRef, FVector NewScale, bool bMarkRenderStateDirty = true);

	UFUNCTION(BlueprintCallable, Category="Nature Zone|Foliage")
	int32 SetFoliageInstancesScale(const TArray<FML_NatureInstanceReference>& InstanceRefs, FVector NewScale);

	UFUNCTION(BlueprintCallable, Category="Nature Zone|Foliage")
	int32 CacheFoliageInstancesInsideBox();

	UFUNCTION(BlueprintCallable, Category="Nature Zone|Foliage")
	int32 CacheAndHideFoliageInstancesInsideBox();

	UFUNCTION(BlueprintCallable, Category="Nature Zone|Foliage")
	int32 HideCachedFoliage();

	UFUNCTION(BlueprintCallable, Category="Nature Zone|Foliage")
	void StartGrowCachedFoliage(float Alpha);

	UFUNCTION(BlueprintCallable, Category="Nature Zone|Foliage")
	void ClearCachedFoliage();

	UFUNCTION(BlueprintCallable, Category="Nature Zone|Material")
	int32 SetMaterialTargetCustomDataInsideBox(int32 CustomDataIndex, float Value);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Nature Zone")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Nature Zone")
	TObjectPtr<UBoxComponent> DetectionBox;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Nature Zone|Foliage")
	TArray<TObjectPtr<UStaticMesh>> TargetMeshes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Nature Zone|Material")
	TArray<TObjectPtr<UStaticMesh>> MaterialTargetMeshes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Nature Zone|Growth")
	float GrowthDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Nature Zone|Growth")
	bool bUseSmoothStepGrowth = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Nature Zone|Growth")
	float HiddenScale = 0.01f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Nature Zone|Detection")
	bool bUsePreciseRotatedBoxCheck = true;

	UPROPERTY(BlueprintReadOnly, Category="Nature Zone|Foliage")
	TArray<FML_NatureCachedInstance> CachedInstances;

	bool bIsGrowing = false;
	float GrowthElapsed = 0.0f;

	void ApplyGrowthAlpha(float Alpha);

	bool ShouldAcceptMesh(const UStaticMesh* Mesh) const;
	bool ShouldAcceptMaterialMesh(const UStaticMesh* Mesh) const;

	bool IsWorldLocationInsideDetectionBox(const FVector& WorldLocation) const;
};