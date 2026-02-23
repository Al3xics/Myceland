// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ML_PlayerCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class AML_Tile;

UCLASS()
class MYCELAND_API AML_PlayerCharacter : public ACharacter
{
	GENERATED_BODY()
	
private:
	UPROPERTY(Category="Myceland Character", VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> SpringArm;
	
	UPROPERTY(Category="Myceland Character", VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> Camera;
	
	void UpdateCurrentTile();

protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY(BlueprintReadOnly, Category="Myceland Character")
	AML_Tile* CurrentTileOn = nullptr;
	
	AML_PlayerCharacter();
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
};
