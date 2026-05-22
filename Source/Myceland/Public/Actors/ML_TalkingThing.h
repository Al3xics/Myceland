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
	
	bool bIsTalking = false;

protected:
	virtual void BeginPlay() override;

public:
	AML_TalkingThing();

	// ~Begin IML_DialogueSpeaker Implementation
	virtual void SetIsTalking(const bool bTalking) override
	{
		this->bIsTalking = bTalking;
		
		if (bTalking)
		{
			FVector Start = GetActorLocation();
			FVector End = Start + FVector(0.f, 0.f, 500.f);
        
			FHitResult Hit;
			FCollisionQueryParams Params;
			Params.AddIgnoredActor(this);
        
			bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
        
			// Debug draw
			DrawDebugLine(
				GetWorld(),
				Start,
				bHit ? Hit.Location : End,
				FColor::Red,
				false,
				2.f,
				0,
				8.f
			);
        
			if (bHit)
			{
				DrawDebugSphere(GetWorld(), Hit.Location, 10.f, 8, FColor::Yellow, false, 2.f);
			}
		}
	}
	virtual bool IsTalking() const override { return bIsTalking; }
	// ~End IML_DialogueSpeaker Implementation
};