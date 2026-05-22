// Copyright Myceland Team, All Rights Reserved.


#include "Actors/ML_TalkingThing.h"


AML_TalkingThing::AML_TalkingThing()
{
	PrimaryActorTick.bCanEverTick = false;

	MeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("MeshComponent"));
	RootComponent = MeshComponent;
}

void AML_TalkingThing::BeginPlay()
{
	Super::BeginPlay();
}