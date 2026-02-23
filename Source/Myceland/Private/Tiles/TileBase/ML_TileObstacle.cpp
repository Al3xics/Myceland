// Copyright Myceland Team, All Rights Reserved.


#include "Tiles/TileBase/ML_TileObstacle.h"


// Sets default values
AML_TileObstacle::AML_TileObstacle()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void AML_TileObstacle::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AML_TileObstacle::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

