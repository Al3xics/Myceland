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
	if (!MycelandController || !MycelandCharacter || !MycelandCharacter->CurrentTileOn)
	{
		return;
	}

	// 1) Give energy to the player.
	MycelandController->CurrentEnergy++;

	// 2) Record the pickup only during a normal move (not during undo playback).
	if (MycelandController->IsMoveInProgress() && !MycelandController->IsUndoMovePlayback())
	{
		const FIntPoint PickedAxial = MycelandCharacter->CurrentTileOn->GetAxialCoord();
		MycelandController->NotifyCollectiblePickedOnAxial(PickedAxial);

		// Alternative if OwningAxial is 100% reliable:
		// MycelandController->NotifyCollectiblePickedOnAxial(GetOwningAxial());
	}

	// 3) World state: this tile no longer has a collectible.
	AML_Tile* Tile = MycelandCharacter->CurrentTileOn;
	Tile->CollectibleActor = nullptr;
	Tile->SetHasCollectible(false);

	// 4) Remove the collectible actor.
	Destroy();
}