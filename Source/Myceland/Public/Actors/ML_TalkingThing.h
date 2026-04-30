// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/ML_NarrativeData.h"
#include "GameFramework/Actor.h"
#include "ML_TalkingThing.generated.h"

UCLASS(Abstract, Blueprintable)
class MYCELAND_API AML_TalkingThing : public AActor
{
	GENERATED_BODY()
	
private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* MeshComponent;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	AML_TalkingThing();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative")
	ESpeakerTag SpeakerTag = ESpeakerTag::None;

	UPROPERTY(BlueprintReadOnly, Category = "Narrative")
	bool bIsTalking = false;

	UFUNCTION(BlueprintCallable, Category = "Narrative")
	void SetIsTalking(bool bTalking);

	ESpeakerTag GetSpeakerTag() const { return SpeakerTag; }
};