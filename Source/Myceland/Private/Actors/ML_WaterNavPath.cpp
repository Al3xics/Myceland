// Copyright Myceland Team, All Rights Reserved.

#include "Actors/ML_WaterNavPath.h"

#include "Components/StaticMeshComponent.h"

void AML_WaterNavPath::Despawn()
{
	// Reverse of the Blueprint Spawn event: hide every mesh and turn off its
	// collision so the bridge is fully inert until the puzzle is won again.
	TArray<UStaticMeshComponent*> Meshes;
	GetComponents(Meshes);
	for (UStaticMeshComponent* Mesh : Meshes)
	{
		if (!Mesh) continue;
		Mesh->SetVisibility(false, /*bPropagateToChildren=*/true);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}