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
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UFMODAudioComponent* AudioComponent;
    	
	UPROPERTY()
	AML_PlayerController* MycelandController;
	
	FVector LastCheckedLocation = FVector::ZeroVector;
	bool bIsTalking = false;
	const float RaycastDistance = 50.f;
	
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
	virtual void SetIsTalking(const bool bTalking) override { this->bIsTalking = bTalking; }
	
	UFUNCTION(BlueprintCallable, Category="Dialogue")
    virtual bool IsTalking() const override { return bIsTalking; }
	
	UFUNCTION(BlueprintCallable, Category="Dialogue")
	virtual UFMODAudioComponent* GetAudioComponent() const override { return AudioComponent; }
	// ~End IML_DialogueSpeaker Implementation
};
