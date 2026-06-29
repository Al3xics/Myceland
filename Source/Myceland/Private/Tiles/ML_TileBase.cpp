// Copyright Myceland Team, All Rights Reserved.


#include "Tiles/ML_TileBase.h"


AML_TileBase::AML_TileBase()
{
	PrimaryActorTick.bCanEverTick = false;
	
	SceneRoot = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;
	
	GroundBase = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GroundBase"));
	GroundBase->SetupAttachment(SceneRoot);
	GroundBase->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GroundBase->SetCollisionObjectType(ECC_GameTraceChannel1);
	GroundBase->SetCollisionResponseToAllChannels(ECR_Block);
	GroundBase->SetGenerateOverlapEvents(false);
}
	
void AML_TileBase::BeginPlay()
{
	Super::BeginPlay();
}


