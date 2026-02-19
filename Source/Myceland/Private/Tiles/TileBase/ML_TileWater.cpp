// Copyright Myceland Team, All Rights Reserved.


#include "Tiles/TileBase/ML_TileWater.h"


// Sets default values
AML_TileWater::AML_TileWater()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void AML_TileWater::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AML_TileWater::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

