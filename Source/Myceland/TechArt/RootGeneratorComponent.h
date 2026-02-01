#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"              // FComponentReference
#include "Components/StaticMeshComponent.h"  // UStaticMeshComponent
#include "RootGeneratorComponent.generated.h"

class USplineComponent;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class MYCELAND_API URootGeneratorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URootGeneratorComponent();

	// Generates/updates RootSpline and returns it (nullptr if failed)
	UFUNCTION(BlueprintCallable, Category = "Root")
	USplineComponent* GenerateRoot();

	// Destroys the spline (your BP should delete spawned spline meshes too)
	UFUNCTION(BlueprintCallable, Category = "Root")
	void DestroyRoot();

	// Convenience toggle
	UFUNCTION(BlueprintCallable, Category = "Root")
	void SetRootEnabled(bool bEnabled);

	// Pick target static mesh component from BP viewport
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Root",
		meta = (UseComponentPicker, AllowedClasses = "StaticMeshComponent"))
	FComponentReference TargetMesh;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Root")
	TObjectPtr<UStaticMeshComponent> TargetMeshPtr = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Root")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	// Path settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Root|Path")
	float StepLengthCm = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Root|Path")
	float SurfaceOffsetCm = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Root|Path")
	float TraceHeightCm = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Root|Path")
	float MaxTurnDeg = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Root|Path")
	int32 MaxPoints = 120;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Root|Path")
	int32 MaxSegments = 256;

	// Debug draw the generated polyline (green). Only lasts a few seconds.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Root|Debug")
	bool bDebugDraw = true;

	// Expose points so BP can build meshes however it wants
	UPROPERTY(BlueprintReadOnly, Category = "Root")
	TArray<FVector> RootPoints;

	// Expose spline so BP can read it
	UPROPERTY(BlueprintReadOnly, Category = "Root")
	TObjectPtr<USplineComponent> RootSpline = nullptr;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void ResolveTargetMesh();

	bool BuildRootPathSimpleStartEnd();
	bool BuildOrUpdateSplineFromPoints();

	// destroy only spline component (keeps points until cleared)
	void CleanupSpline();

	static float PolylineLength(const TArray<FVector>& Pts);
};
