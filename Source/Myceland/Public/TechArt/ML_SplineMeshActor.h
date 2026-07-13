#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ML_SplineMeshActor.generated.h"

class USceneComponent;
class USplineComponent;
class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;

UENUM(BlueprintType)
enum class EMLMeshForwardAxis : uint8
{
	X,
	Y,
	Z
};

UCLASS(Blueprintable)
class MYCELAND_API AML_SplineMeshActor : public AActor
{
	GENERATED_BODY()

public:
	AML_SplineMeshActor();

	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Spline")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Spline")
	TObjectPtr<USplineComponent> Spline;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mesh")
	TObjectPtr<UStaticMesh> StaticMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mesh")
	TObjectPtr<UMaterialInterface> MaterialOverride;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mesh", meta=(ClampMin="0"))
	int32 MaterialIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mesh")
	EMLMeshForwardAxis MeshForwardAxis = EMLMeshForwardAxis::X;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mesh")
	FRotator RotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mesh")
	FVector MeshScale = FVector::OneVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mesh")
	FVector LocationOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mesh")
	bool bAlignMeshToSpline = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mesh")
	bool bUseSplinePointScale = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Generation", meta=(ClampMin="1"))
	int32 DefaultMeshCountPerSegment = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Generation")
	TArray<int32> MeshCountPerSegment;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Generation")
	bool bGenerateClosedLoopSegment = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Generation")
	bool bCenterMeshesInsideSegment = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Generation", meta=(ClampMin="0.0"))
	float StartPadding = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Generation", meta=(ClampMin="0.0"))
	float EndPadding = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Collision")
	bool bEnableCollision = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Collision", meta=(EditCondition="bEnableCollision"))
	FName CollisionProfile = TEXT("BlockAll");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rendering")
	bool bCastShadow = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rendering")
	bool bVisible = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rendering")
	TEnumAsByte<EComponentMobility::Type> MeshMobility = EComponentMobility::Movable;

	UFUNCTION(BlueprintCallable, CallInEditor, Category="Spline Mesh")
	void RebuildMeshes();

	UFUNCTION(BlueprintCallable, CallInEditor, Category="Spline Mesh")
	void ClearMeshes();

	UFUNCTION(BlueprintPure, Category="Spline Mesh")
	int32 GetGeneratedMeshCount() const;

protected:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> GeneratedMeshes;

	void GenerateSegment(int32 SegmentIndex, float StartDistance, float EndDistance);
	UStaticMeshComponent* CreateMeshComponent(int32 SegmentIndex, int32 MeshIndex);
	int32 GetMeshCountForSegment(int32 SegmentIndex) const;
	FQuat GetForwardAxisCorrection() const;
};