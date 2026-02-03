// RootSplineGeneratorComponent.h

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/SplineComponent.h"
#include "RootSplineGeneratorComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class MYCELAND_API URootSplineGeneratorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URootSplineGeneratorComponent();

	// =======================
	// Main root settings
	// =======================

	// Total spline points INCLUDING endpoints
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Root|Main", meta = (ClampMin = "2"))
	int32 MainNumPoints = 16;

	// Max sideways offset in Unreal units
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Root|Main", meta = (ClampMin = "0.0"))
	float MainWidth = 100.0f;

	// Number of wave cycles along the root
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Root|Main", meta = (ClampMin = "0.0"))
	float MainOndulations = 2.0f;

	// =======================
	// Extrusion settings
	// =======================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Root|Extrusions", meta = (ClampMin = "0"))
	int32 ExtrudingRootsNumbers = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Root|Extrusions", meta = (ClampMin = "0.0"))
	float ExtrudingRootsLength = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Root|Extrusions", meta = (ClampMin = "2"))
	int32 ExtrudeNumPoints = 12;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Root|Extrusions", meta = (ClampMin = "0.0"))
	float ExtrudeWidth = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Root|Extrusions", meta = (ClampMin = "0.0"))
	float ExtrudeOndulations = 2.0f;

	// =======================
	// 2D mode
	// =======================

	// If true: EVERYTHING forced to XY plane with FixedZ
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Root|2D")
	bool bForce2D = true;

	// If bForce2D, choose how to pick Z plane
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Root|2D")
	bool bUseStartZAsFixedZ = true;

	// Used when bUseStartZAsFixedZ = false
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Root|2D", meta = (EditCondition = "bForce2D && !bUseStartZAsFixedZ"))
	float FixedZ = 0.0f;

	// =======================
	// Random
	// =======================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Root|Random")
	bool bDeterministic = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Root|Random", meta = (EditCondition = "bDeterministic"))
	int32 Seed = 12345;

	// Generates a main spline between Start and End, plus N extruded splines
	// Outputs:
	//   OutMainSpline: created main spline component
	//   OutExtrusionSplines: created extrusion splines
	UFUNCTION(BlueprintCallable, Category = "Root")
	void GenerateRoot(
		const FVector& Start,
		const FVector& End,
		USplineComponent*& OutMainSpline,
		TArray<USplineComponent*>& OutExtrusionSplines
	);

	UFUNCTION(BlueprintCallable, Category = "Root")
	void ClearGenerated();

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<USplineComponent> MainSpline;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USplineComponent>> ExtrusionSplines;

	FRandomStream Rand;

	USplineComponent* CreateSplineComponent(const FName& Name, USceneComponent* AttachParent);

	// Strict 2D helpers
	static FVector FlattenXY(const FVector& V, float Z);
	static FVector2D SafeNormal2D(const FVector2D& V);
	static FVector MakePerp2D_FromDir2D(const FVector2D& DirUnit2D); // (x,y) -> (-y,x)

	static void BuildWavyPoints2D(
		const FVector& Start,
		const FVector& End,
		float ZPlane,
		int32 NumPoints,
		float Width,
		float Ondulations,
		float Phase,
		TArray<FVector>& OutPoints
	);

	void FillSplineFromPoints(USplineComponent* Spline, const TArray<FVector>& Points);
};
