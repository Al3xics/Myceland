// Copyright Myceland Team, All Rights Reserved.


#include "Tiles/TileBase/ML_TileWater.h"


// Sets default values
AML_TileWater::AML_TileWater()
{
	PrimaryActorTick.bCanEverTick = false;
}

// Called when the game starts or when spawned
void AML_TileWater::BeginPlay()
{
	Super::BeginPlay();
	
}

void AML_TileWater::SetWaterState(EML_WaterState NewState)
{
	if (WaterState == NewState)
	{
		return;
	}

	WaterState = NewState;
	OnWaterStateChanged(NewState);
}

