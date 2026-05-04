// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ML_CameraRail.generated.h"

class UCameraComponent;
class AML_PlayerCharacter;
class USplineComponent;

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

	UPROPERTY(EditAnywhere, Category="Camera Rail", meta=(ClampMin=0.f, ClampMax=1.f))
	float PreviewCompletion = 0.f;

	UPROPERTY(EditAnywhere, Category="Camera Rail")
	bool bEnableCameraLag = true;

	// Higher = faster catch-up, lower = more lag. 0 = frozen.
	UPROPERTY(EditAnywhere, Category="Camera Rail", meta=(ClampMin=0.f, ClampMax=20.f, EditCondition="bEnableCameraLag"))
	float CameraLagSpeed = 3.f;

	float CurrentCompletion = 0.f;

	void SyncSplines(USplineComponent* Source, USplineComponent* Destination) const;
	void PlaceCamera(const FVector& CameraPosition, const FVector& LookAtPosition) const;

protected:
	virtual void BeginPlay() override;

public:
	AML_CameraRail();
	virtual void Tick(float DeltaTime) override;
	virtual void OnConstruction(const FTransform& Transform) override;
	
	UFUNCTION(CallInEditor, Category="Camera Rail", meta = (DisplayName = "Sync Rail -> LookAt", ToolTip = "Copies Rail's spline points structure to LookAt. LookAt keeps its own world offset. WARNING: overwrites LookAt points permanently."))
	void SyncRailToLookAtSpline() const { SyncSplines(Rail, LookAt); }

	UFUNCTION(CallInEditor, Category="Camera Rail", meta = (DisplayName = "Sync LookAt -> Rail", ToolTip = "Copies LookAt's spline points structure to Rail. Rail keeps its own world offset. WARNING: overwrites Rail points permanently."))
	void SyncLookAtToRailSpline() const { SyncSplines(LookAt, Rail); }
};
