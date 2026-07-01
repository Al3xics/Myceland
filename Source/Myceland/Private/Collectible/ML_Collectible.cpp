// Copyright Myceland Team, All Rights Reserved.

#include "Collectible/ML_Collectible.h"

#include "Audio/ML_FMODEvents.h"
#include "Components/SphereComponent.h"
#include "Player/ML_PlayerCharacter.h"
#include "Player/ML_PlayerController.h"
#include "Subsystem/ML_SoundSubsystem.h"
#include "Tiles/ML_Tile.h"


AML_Collectible::AML_Collectible()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->SetupAttachment(RootComponent);
	Collision->InitSphereRadius(50.f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionResponseToAllChannels(ECR_Overlap);
	Collision->SetGenerateOverlapEvents(true);
}

void AML_Collectible::DestroyCollectible()
{
	BeforeDestroyCollectible(OwningTile);
	
	if (OwningTile && OwningTile->CollectibleActor == this)
		OwningTile->CollectibleActor = nullptr;
	
	Destroy();
}

void AML_Collectible::BeginPlay()
{
	Super::BeginPlay();
}

void AML_Collectible::AddEnergy(AML_PlayerController* MycelandController, AML_PlayerCharacter* MycelandCharacter)
{
	if (!MycelandController || !MycelandCharacter || !MycelandCharacter->CurrentTileOn)
	{
		return;
	}

	MycelandController->EnergyComponent->AddEnergy(+1);

	if (UML_SoundSubsystem* SoundSubsystem = UML_SoundSubsystem::Get(this))
	{
		SoundSubsystem->StartSound2DByPath(MLFMODEvents::EnergyCollect);
	}

	if (OwningTile)
	{
		// Record the pickup only during a normal move (not during undo playback).
		if (!MycelandController->IsUndoMovePlayback())
		{
			const FIntPoint PickedAxial = OwningTile->GetAxialCoord();
			MycelandController->NotifyCollectiblePickedOnAxial(PickedAxial);
		}

		OwningTile->CollectibleActor = nullptr;
		OwningTile->SetHasCollectible(false);
		OwningTile = nullptr;
	}
	DestroyCollectible();
}

// bool AML_Collectible::CheckIsOwningTile(AML_PlayerCharacter* MycelandCharacter)
// {
// 	return MycelandCharacter->CurrentTileOn == OwningTile;
// }
