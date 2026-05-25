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

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

};
