// Copyright Myceland Team, All Rights Reserved.


#include "Collectible/ML_Collectible.h"

#include "Components/SphereComponent.h"
#include "Player/ML_PlayerController.h"

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

void AML_Collectible::AddEnergy(AML_PlayerController* MycelandController)
{
	if (!MycelandController) return;

	MycelandController->CurrentEnergy++;

	if (MycelandController->IsMoveInProgress())
	{
		MycelandController->NotifyCollectiblePickedOnAxial(GetOwningAxial());
	}
}