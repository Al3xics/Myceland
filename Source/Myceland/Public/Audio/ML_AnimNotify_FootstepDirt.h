// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "ML_AnimNotify_FootstepDirt.generated.h"

/** Plays the FMOD dirt footstep event at the animated actor's location. */
UCLASS(meta=(DisplayName="ML Footstep Dirt"))
class MYCELAND_API UML_AnimNotify_FootstepDirt : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;
};
