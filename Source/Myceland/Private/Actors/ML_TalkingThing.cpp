// Copyright Myceland Team, All Rights Reserved.


#include "Actors/ML_TalkingThing.h"
#include "FMODAudioComponent.h"


AML_TalkingThing::AML_TalkingThing()
{
	PrimaryActorTick.bCanEverTick = false;

	MeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("MeshComponent"));
	RootComponent = MeshComponent;
	
	AudioComponent = CreateDefaultSubobject<UFMODAudioComponent>(TEXT("FMODAudioComponent"));
	AudioComponent->SetupAttachment(MeshComponent);
}

void AML_TalkingThing::BeginPlay()
{
	Super::BeginPlay();
}