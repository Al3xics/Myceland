// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tiles/ML_TileBase.h"
#include "ML_TileWater.generated.h"

UENUM(BlueprintType)
enum class EML_WaterState : uint8
{
    Dead  UMETA(DisplayName="Dead"),
    Alive UMETA(DisplayName="Alive")
};

UCLASS()
class MYCELAND_API AML_TileWater : public AML_TileBase
{
    GENERATED_BODY()

public:
    // Sets default values for this actor's properties
    AML_TileWater();

    UPROPERTY(BlueprintReadOnly, Category="Myceland Water")
    EML_WaterState WaterState = EML_WaterState::Dead;

    UFUNCTION(BlueprintCallable, Category="Myceland Water")
    void SetWaterState(EML_WaterState NewState);

    UFUNCTION(BlueprintImplementableEvent, Category="Myceland Water")
    void OnWaterStateChanged(EML_WaterState NewState);

    // Dynamic material instance reference (created in BP, same as before)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Myceland Water|Material")
    UMaterialInstanceDynamic* WaterMaterialRef = nullptr;

    // --- Designer-facing tuning, editable directly in the BP Details panel, no graph needed ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Myceland Water|Colors")
    FLinearColor AliveCoastLineColor = FLinearColor(1.f, 0.001f, 0.323f, 1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Myceland Water|Colors")
    FLinearColor DeadCoastLineColor = FLinearColor(0.04f, 1.f, 0.f, 1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Myceland Water|Timing", meta=(ClampMin="0.01"))
    float TransitionDuration = 1.0f;

    // Replaces the Lerp(1,100) that fed the "CoastLine" scalar parameter
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Myceland Water|Timing")
    FVector2D CoastLineLerpRange = FVector2D(1.f, 100.f);

    // Replaces the raw (un-lerped) Timeline track that fed the "DeadToAlive" scalar parameter
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Myceland Water|Timing")
    FVector2D DeadToAliveLerpRange = FVector2D(0.f, 1.f);

protected:
    // Called when the game starts or when spawned
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

private:
    float ElapsedTime = 0.f;
    float CurrentAlpha = 0.f;
    float TargetAlpha = 0.f;
    bool bIsTransitioning = false;

    void StartTransition(bool bToAlive);
    void UpdateMaterialParameters(float Alpha);
};