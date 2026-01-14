#include "MycelandTile.h"
#include "Components/StaticMeshComponent.h"

AMycelandTile::AMycelandTile()
{
	PrimaryActorTick.bCanEverTick = false;

	// Root
	CaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CaseMesh"));
	RootComponent = CaseMesh;

	// Collision
	CaseMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CaseMesh->SetCollisionObjectType(ECC_WorldStatic);
	CaseMesh->SetCollisionResponseToAllChannels(ECR_Block);
	CaseMesh->SetGenerateOverlapEvents(true);

	// Interaction (clic / hover)
	CaseMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}
