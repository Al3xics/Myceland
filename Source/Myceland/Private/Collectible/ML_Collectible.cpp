// Copyright Myceland Team, All Rights Reserved.


#include "Collectible/ML_Collectible.h"

#include "Components/SphereComponent.h"
#include "Player/ML_PlayerController.h"


AML_Collectible::AML_Collectible()
{
	PrimaryActorTick.bCanEverTick = true;
	
	// Root pour gérer la rotation/position
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	// Sphere collider pour overlap
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
	MycelandController->CurrentEnergy++;
}

