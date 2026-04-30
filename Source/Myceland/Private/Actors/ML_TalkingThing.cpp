// Copyright Myceland Team, All Rights Reserved.


#include "Actors/ML_TalkingThing.h"

#include "Subsystem/ML_NarrativeSubsystem.h"


AML_TalkingThing::AML_TalkingThing()
{
	PrimaryActorTick.bCanEverTick = false;

	MeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("MeshComponent"));
	RootComponent = MeshComponent;
}

void AML_TalkingThing::BeginPlay()
{
	Super::BeginPlay();

	if (UML_NarrativeSubsystem* SubSys = UML_NarrativeSubsystem::Get(this))
		SubSys->RegisterTalkingThing(this);
}

void AML_TalkingThing::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UML_NarrativeSubsystem* SubSys = UML_NarrativeSubsystem::Get(this))
		SubSys->UnregisterTalkingThing(this);

	Super::EndPlay(EndPlayReason);
}

void AML_TalkingThing::SetIsTalking(bool bTalking)
{
	bIsTalking = bTalking;
}