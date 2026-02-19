// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tiles/ML_TileBase.h"
#include "ML_TileWater.generated.h"

UCLASS()
class MYCELAND_API AML_TileWater : public AML_TileBase
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AML_TileWater();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};
