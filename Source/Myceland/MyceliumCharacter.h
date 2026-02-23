// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Tiles/ML_Tile.h"
#include "MyceliumCharacter.generated.h"

/**
 *  A controllable top-down perspective character
 */
UCLASS(abstract)
class AMyceliumCharacter : public ACharacter
{
	GENERATED_BODY()

private:

	

public:

	/** Constructor */
	AMyceliumCharacter();

	/** Initialization */
	virtual void BeginPlay() override;

	/** Update */
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "Mycelium Character")
	AML_Tile* CurrentTileOn;
};

