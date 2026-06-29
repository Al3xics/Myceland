// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/ML_DialogueSpeaker.h"
#include "ML_TalkingThing.generated.h"

UCLASS(Abstract, Blueprintable)
class MYCELAND_API AML_TalkingThing : public AActor, public IML_DialogueSpeaker
{
	GENERATED_BODY()
	
private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* MeshComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UFMODAudioComponent* AudioComponent;
	
	bool bIsTalking = false;

protected:
	virtual void BeginPlay() override;

public:
	AML_TalkingThing();

	// ~Begin IML_DialogueSpeaker Implementation
	virtual void SetIsTalking(const bool bTalking) override { this->bIsTalking = bTalking; }
	
	UFUNCTION(BlueprintCallable, Category="Dialogue")
	virtual bool IsTalking() const override { return bIsTalking; }
	
	UFUNCTION(BlueprintCallable, Category="Dialogue")
	virtual UFMODAudioComponent* GetAudioComponent() const override { return AudioComponent; }
	// ~End IML_DialogueSpeaker Implementation
};