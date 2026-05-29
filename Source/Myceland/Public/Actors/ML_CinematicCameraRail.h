#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ML_CinematicCameraRail.generated.h"

class UCineCameraComponent;
class USplineComponent;

UCLASS()
class MYCELAND_API AML_CinematicCameraRail : public AActor
{
	GENERATED_BODY()

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cinematic Camera Rail", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USplineComponent> Rail;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cinematic Camera Rail", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USplineComponent> LookAt;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cinematic Camera Rail", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UCineCameraComponent> Camera;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="Cinematic Camera Rail", meta=(AllowPrivateAccess="true", ClampMin="0.0", ClampMax="1.0"))
	float PreviewCompletion = 0.f;

	void SyncSplines(USplineComponent* Source, USplineComponent* Destination) const;
	void PlaceCamera(const FVector& CameraPosition, const FVector& LookAtPosition) const;

public:
	AML_CinematicCameraRail();

	virtual void Tick(float DeltaTime) override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual bool ShouldTickIfViewportsOnly() const override { return true; }

	UFUNCTION(BlueprintPure, Category="Cinematic Camera Rail")
	USplineComponent* GetRailFromCameraRail() const { return Rail; }

	UFUNCTION(BlueprintPure, Category="Cinematic Camera Rail")
	USplineComponent* GetLookAtFromCameraRail() const { return LookAt; }

	UFUNCTION(BlueprintPure, Category="Cinematic Camera Rail")
	UCineCameraComponent* GetCameraFromCameraRail() const { return Camera; }

	UFUNCTION(BlueprintCallable, Category="Cinematic Camera Rail")
	void SetPreviewCompletion(float NewCompletion);

	UFUNCTION(BlueprintPure, Category="Cinematic Camera Rail")
	float GetPreviewCompletion() const { return PreviewCompletion; }

	UFUNCTION(CallInEditor, Category="Cinematic Camera Rail", meta=(DisplayName="Sync Rail -> LookAt"))
	void SyncRailToLookAtSpline() const { SyncSplines(Rail, LookAt); }

	UFUNCTION(CallInEditor, Category="Cinematic Camera Rail", meta=(DisplayName="Sync LookAt -> Rail"))
	void SyncLookAtToRailSpline() const { SyncSplines(LookAt, Rail); }
};