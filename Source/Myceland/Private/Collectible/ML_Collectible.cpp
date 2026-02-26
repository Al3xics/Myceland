// Copyright Myceland Team, All Rights Reserved.


#include "Collectible/ML_Collectible.h"

#include "Components/SphereComponent.h"
#include "Player/ML_PlayerController.h"
#include "Player/ML_PlayerCharacter.h"
#include "Tiles/ML_Tile.h"

AML_Collectible::AML_Collectible()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->SetupAttachment(RootComponent);
	Collision->InitSphereRadius(50.f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionResponseToAllChannels(ECR_Overlap);
	Collision->SetGenerateOverlapEvents(true);
}

void AML_Collectible::BeginPlay()
{
	Super::BeginPlay();
}

void AML_Collectible::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AML_Collectible::AddEnergy(AML_PlayerController* MycelandController, AML_PlayerCharacter* MycelandCharacter)
{
	if (!MycelandController || !MycelandCharacter || !MycelandCharacter->CurrentTileOn) return;

	// 1) énergie
	MycelandController->CurrentEnergy++;

	// 2) record du pickup si on est dans un move "normal"
	// (IMPORTANT: éviter de polluer pendant l'undo playback)
	if (MycelandController->IsMoveInProgress() && !MycelandController->IsUndoMovePlayback())
	{
		// soit via l'axial de la tile sous le joueur au moment exact
		const FIntPoint PickedAxial = MycelandCharacter->CurrentTileOn->GetAxialCoord();
		MycelandController->NotifyCollectiblePickedOnAxial(PickedAxial);

		// (option alternative) si ton OwningAxial est fiable :
		// MycelandController->NotifyCollectiblePickedOnAxial(GetOwningAxial());
	}

	// 3) monde : plus de collectible sur cette tile
	MycelandCharacter->CurrentTileOn->SetHasCollectible(false);

	// 4) destroy actor
	Destroy();
}