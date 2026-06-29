#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SplineFilledMeshActor.generated.h"

class USplineComponent;
class UProceduralMeshComponent;
class UMaterialInterface;

UCLASS()
class MYCELAND_API ASplineFilledMeshActor : public AActor
{
	GENERATED_BODY()

public:
	ASplineFilledMeshActor();

protected:
	virtual void OnConstruction(const FTransform& Transform) override;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	USplineComponent* Spline;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UProceduralMeshComponent* Mesh;

	UPROPERTY(EditAnywhere, Category = "Mesh")
	UMaterialInterface* Material;

	UPROPERTY(EditAnywhere, Category = "Mesh", meta = (ClampMin = "8"))
	int32 SplineSamples = 96;

	UPROPERTY(EditAnywhere, Category = "Mesh", meta = (ClampMin = "1"))
	int32 FillRings = 12;

	UPROPERTY(EditAnywhere, Category = "Mesh", meta = (ClampMin = "1"))
	int32 BevelSegments = 6;

	UPROPERTY(EditAnywhere, Category = "Mesh", meta = (ClampMin = "1"))
	float Thickness = 40.f;

	UPROPERTY(EditAnywhere, Category = "Mesh", meta = (ClampMin = "0"))
	float BevelRadius = 20.f;

	UPROPERTY(EditAnywhere, Category = "Mesh")
	float ZOffset = 0.f;

	UPROPERTY(EditAnywhere, Category = "Mesh")
	float UVScale = 0.01f;

	UPROPERTY(EditAnywhere, Category = "Mesh")
	bool bCreateCollision = true;

	void BuildMesh();
};