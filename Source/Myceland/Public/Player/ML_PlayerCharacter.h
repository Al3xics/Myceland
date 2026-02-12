// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ML_PlayerCharacter.generated.h"

class AML_Tile;

UCLASS()
class MYCELAND_API AML_PlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AML_PlayerCharacter();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	
	UFUNCTION(BlueprintCallable, Category="Myceland Character")
	AML_Tile* GetCurrentTileOn();
};
