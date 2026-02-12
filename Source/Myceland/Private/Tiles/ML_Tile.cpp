// Copyright Myceland Team, All Rights Reserved.


#include "Tiles/ML_Tile.h"


AML_Tile::AML_Tile()
{
	PrimaryActorTick.bCanEverTick = false;
	
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;
	
	TileChildActor = CreateDefaultSubobject<UChildActorComponent>(TEXT("TileChildActor"));
	TileChildActor->SetupAttachment(RootComponent);

	HighlightTileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TileMesh"));
	HighlightTileMesh->SetupAttachment(RootComponent);
	HighlightTileMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	HighlightTileMesh->SetCollisionObjectType(ECC_WorldStatic);
	HighlightTileMesh->SetCollisionResponseToAllChannels(ECR_Block);
	HighlightTileMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	HighlightTileMesh->SetGenerateOverlapEvents(false);
	
	HexagonCollision = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HexagonCollision"));
	HexagonCollision->SetupAttachment(RootComponent);
	HexagonCollision->SetRelativeScale3D(FVector(0.9f, 0.9f, 0.9f));
	HexagonCollision->SetCollisionObjectType(ECC_WorldStatic);
	HexagonCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	HexagonCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	HexagonCollision->SetGenerateOverlapEvents(false);
	HexagonCollision->SetHiddenInGame(true);
	HexagonCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AML_Tile::SetBlocked(bool bNewBlocked)
{
	bBlocked = bNewBlocked;

	if (!HexagonCollision) return;

	HexagonCollision->SetCollisionEnabled(
		bBlocked
		? ECollisionEnabled::QueryOnly
		: ECollisionEnabled::NoCollision
	);
}
