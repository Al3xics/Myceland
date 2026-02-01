#include "TileBase.h"


ATileBase::ATileBase()
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
	
void ATileBase::BeginPlay()
{
	Super::BeginPlay();
}

void ATileBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

