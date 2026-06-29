// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ML_CameraRail.generated.h"

class AML_PlayerController;
class UBoxComponent;
class AML_CameraRail;
class UCameraComponent;
class AML_PlayerCharacter;
class USplineComponent;

USTRUCT(BlueprintType)
struct FCameraRailTransition
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transition")
	float BlendTime = 2.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transition")
	TEnumAsByte<EViewTargetBlendFunction> BlendFunc = VTBlend_Linear;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transition")
	TObjectPtr<AML_CameraRail> NextRail = nullptr;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transition")
	TObjectPtr<UBoxComponent> TriggerBox = nullptr;
};

UCLASS()
class MYCELAND_API AML_CameraRail : public AActor
{
	GENERATED_BODY()

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera Rail", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USplineComponent> Rail;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera Rail", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USplineComponent> LookAt;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera Rail", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> Camera;
	
	UPROPERTY()
	AML_PlayerCharacter* Player = nullptr;
	
	UPROPERTY()
	AML_PlayerController* PlayerController = nullptr;

	UPROPERTY(EditAnywhere, Category="Camera Rail", meta=(ClampMin=0.f, ClampMax=1.f))
	float PreviewCompletion = 0.f;

	UPROPERTY(EditAnywhere, Category="Camera Rail")
	bool bEnableCameraLag = true;

	// Higher = faster catch-up, lower = more lag. 0 = frozen.
	UPROPERTY(EditAnywhere, Category="Camera Rail", meta=(ClampMin=0.f, ClampMax=20.f, EditCondition="bEnableCameraLag"))
	float CameraLagSpeed = 3.f;
	
	UPROPERTY(EditAnywhere, Category="Camera Rail", EditFixedSize)
	TArray<FCameraRailTransition> Transitions;

	float CurrentCompletion = 0.f;

	void SyncSplines(USplineComponent* Source, USplineComponent* Destination) const;
	void PlaceCamera(const FVector& CameraPosition, const FVector& LookAtPosition) const;

protected:
	virtual void BeginPlay() override;
	
	UFUNCTION()
	void OnTriggerEnter(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	

public:
	AML_CameraRail();
	virtual void Tick(float DeltaTime) override;
	virtual void OnConstruction(const FTransform& Transform) override;
	
	UFUNCTION(BlueprintPure, Category="Camera Rail")
	USplineComponent* GetRailFromCameraRail() const { return Rail; }

	UFUNCTION(BlueprintPure, Category="Camera Rail")
	USplineComponent* GetLookAtFromCameraRail() const { return LookAt; }

	UFUNCTION(BlueprintPure, Category="Camera Rail")
	UCameraComponent* GetCameraFromCameraRail() const { return Camera; }
	
	UFUNCTION(CallInEditor, Category="Camera Rail", meta = (DisplayName = "Sync Rail -> LookAt", ToolTip = "Copies Rail's spline points structure to LookAt. LookAt keeps its own world offset. WARNING: overwrites LookAt points permanently."))
	void SyncRailToLookAtSpline() const { SyncSplines(Rail, LookAt); }

	UFUNCTION(CallInEditor, Category="Camera Rail", meta = (DisplayName = "Sync LookAt -> Rail", ToolTip = "Copies LookAt's spline points structure to Rail. Rail keeps its own world offset. WARNING: overwrites Rail points permanently."))
	void SyncLookAtToRailSpline() const { SyncSplines(LookAt, Rail); }
	
#if WITH_EDITOR
	UFUNCTION(CallInEditor, Category="Camera Rail", meta = (DisplayName = "Add Transition"))
	void AddTransition();

	UFUNCTION(CallInEditor, Category="Camera Rail", meta = (DisplayName = "Remove Last Transition"))
	void RemoveLastTransition();
#endif
};
