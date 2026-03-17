// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ML_PlayerCharacter.generated.h"

class AML_PlayerController;
class UCameraComponent;
class USpringArmComponent;
class AML_Tile;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBoardChanged, const AML_Tile*, NewTile);

UCLASS()
class MYCELAND_API AML_PlayerCharacter : public ACharacter
{
	GENERATED_BODY()
	
private:
	UPROPERTY(Category="Myceland Character", VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> SpringArm;
	
	UPROPERTY(Category="Myceland Character", VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> Camera;
	
	UPROPERTY()
	AML_PlayerController* MycelandController;
	
	void UpdateCurrentTile();
	void HandleTileStateChange(const AML_Tile* OldTile, const AML_Tile* NewTile) const;

protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY(BlueprintReadOnly, Category="Myceland Character")
	AML_Tile* CurrentTileOn = nullptr;
	
	UPROPERTY(BlueprintAssignable, Category="Myceland Character")
	FOnBoardChanged OnBoardChanged;
	
	AML_PlayerCharacter();
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
};
