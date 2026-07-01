// Copyright Myceland Team, All Rights Reserved.

#include "Audio/ML_AnimNotify_FootstepDirt.h"

#include "Audio/ML_FMODEvents.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Subsystem/ML_SoundSubsystem.h"

void UML_AnimNotify_FootstepDirt::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!IsValid(MeshComp))
	{
		return;
	}

	if (UML_SoundSubsystem* SoundSubsystem = UML_SoundSubsystem::Get(MeshComp))
	{
		const AActor* AnimatedActor = MeshComp->GetOwner();
		const FTransform SoundTransform = IsValid(AnimatedActor)
			? AnimatedActor->GetActorTransform()
			: MeshComp->GetComponentTransform();

		SoundSubsystem->StartSoundAtLocationByPath(MLFMODEvents::AvatarFootstepDirt, SoundTransform);
	}
}

FString UML_AnimNotify_FootstepDirt::GetNotifyName_Implementation() const
{
	return TEXT("ML Footstep Dirt");
}
