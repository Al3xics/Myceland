// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ML_PlayerController.generated.h"

UCLASS()
class MYCELAND_API AML_PlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable, Category = "Myceland Controller")
	void TryPlantGrass();
	
	UFUNCTION(BlueprintCallable, Category = "Myceland Controller")
	void ConfirmTurn();
};
