// Copyright Myceland Team, All Rights Reserved.


#include "Tiles/ML_TileBase.h"


AML_TileBase::AML_TileBase()
{
	PrimaryActorTick.bCanEverTick = true;
	
	SceneRoot = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;
	
	GroundBase = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GroundBase"));
	GroundBase->SetupAttachment(SceneRoot);
	GroundBase->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GroundBase->SetCollisionObjectType(ECC_WorldStatic);
	GroundBase->SetCollisionResponseToAllChannels(ECR_Block);
	GroundBase->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	GroundBase->SetGenerateOverlapEvents(false);
}
	
void AML_TileBase::BeginPlay()
{
	Super::BeginPlay();
}

void AML_TileBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

