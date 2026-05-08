// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Interfaces/ML_DialogueSpeaker.h"
#include "Myceland/Public/Tiles/ML_BoardSpawner.h"
#include "ML_PlayerCharacter.generated.h"

class AML_PlayerController;
class UCameraComponent;
class USpringArmComponent;
class AML_Tile;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCurrentTileChanged, const AML_Tile*, OldTile, const AML_Tile*, NewTile);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBoardChanged, const AML_Tile*, OldTile, const AML_Tile*, NewTile);

UCLASS()
class MYCELAND_API AML_PlayerCharacter : public ACharacter, public IML_DialogueSpeaker
{
	GENERATED_BODY()
	
private:
	UPROPERTY()
	AML_PlayerController* MycelandController;
	
	FVector LastCheckedLocation = FVector::ZeroVector;
	bool bIsTalking = false;
	
	void HandleTileStateChange(const AML_Tile* OldTile, const AML_Tile* NewTile) const;

public:
	virtual void BeginPlay() override;
	
	void UpdateCurrentTile();

	UPROPERTY(BlueprintReadOnly, Category="Myceland Character")
	AML_Tile* CurrentTileOn = nullptr;
	
	UPROPERTY(BlueprintAssignable, Category="Myceland Character|Delegates")
	FOnBoardChanged OnBoardChanged;
	
	UPROPERTY(BlueprintAssignable, Category = "Myceland Character|Delegates")
	FOnCurrentTileChanged OnCurrentTileChanged;
	
	AML_PlayerCharacter();
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	
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
