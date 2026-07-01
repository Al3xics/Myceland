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
	// GroundBase is a ground-level hexagon and serves BOTH roles:
	//   - cursor detection (Cursor channel = ECC_GameTraceChannel1)
	//   - physical floor/obstacle collision (the character walks on it and bumps into obstacles)
	// Detecting at ground level (not on an elevated proxy) avoids cursor parallax with the angled camera.
	// Object type stays WorldStatic — the old ClickableGround object type was removed with that channel.
	// REQUIRES the hexagon mesh to have SIMPLE collision (the trace uses bTraceComplex=false).
	GroundBase->SetCollisionResponseToAllChannels(ECR_Block);
	GroundBase->SetGenerateOverlapEvents(false);
}
	
void AML_TileBase::BeginPlay()
{
	Super::BeginPlay();
}


