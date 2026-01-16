#include "MycelandTile.h"
#include "Components/StaticMeshComponent.h"

AMycelandTile::AMycelandTile()
{
	PrimaryActorTick.bCanEverTick = false;

	// =========================
	// Sol (tuile)
	// =========================
	CaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CaseMesh"));
	RootComponent = CaseMesh;

	CaseMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CaseMesh->SetCollisionObjectType(ECC_WorldStatic);
	CaseMesh->SetCollisionResponseToAllChannels(ECR_Block);
	CaseMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	CaseMesh->SetGenerateOverlapEvents(false);

	// =========================
	// Blocker (mur invisible)
	// =========================
	Blocker = CreateDefaultSubobject<USphereComponent>(TEXT("Blocker"));
	Blocker->SetupAttachment(RootComponent);

	Blocker->SetCollisionObjectType(ECC_WorldStatic);
	Blocker->SetCollisionResponseToAllChannels(ECR_Ignore);
	Blocker->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Blocker->SetGenerateOverlapEvents(false);

	// Désactivé par défaut
	Blocker->SetHiddenInGame(true);
	Blocker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AMycelandTile::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Recalcule le rayon UNE FOIS que le mesh est bien connu
	SetBlockRadius();

	// Place la sphère à hauteur de capsule (≈ 90–100)
	Blocker->SetRelativeLocation(FVector(0.f, 0.f, 90.f));

	// Applique l’état bloqué
	SetBlocked(bBlocked);
}

void AMycelandTile::SetBlocked(bool bNewBlocked)
{
	bBlocked = bNewBlocked;

	if (!Blocker) return;

	Blocker->SetCollisionEnabled(
		bBlocked
		? ECollisionEnabled::QueryOnly
		: ECollisionEnabled::NoCollision
	);
}

void AMycelandTile::SetBlockRadius()
{
	if (!CaseMesh) return;

	UStaticMesh* Mesh = CaseMesh->GetStaticMesh();
	if (!Mesh) return;

	const FBoxSphereBounds Bounds = Mesh->GetBounds();
	const FVector Extent = Bounds.BoxExtent; // demi-tailles

	// Rayon suffisant pour couvrir la tuile sur X/Y
	const float Radius = FMath::Max(Extent.X, Extent.Y);

	// Alternative (si tu veux couvrir aussi les coins) :
	// const float Radius = FVector2D(Extent.X, Extent.Y).Size();

	Blocker->SetSphereRadius(Radius, true);
}
