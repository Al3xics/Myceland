// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tiles/ML_TileBase.h"
#include "ML_TileWaterPath.generated.h"

/**
 * 
 */
UCLASS()
class MYCELAND_API AML_TileWaterPath : public AML_TileBase
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AML_TileWaterPath();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
};
