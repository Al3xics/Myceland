// Copyright Myceland Team, All Rights Reserved.


#include "Player/ML_PlayerCharacter.h"

#include "Tiles/ML_Tile.h"


AML_PlayerCharacter::AML_PlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AML_PlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	
}

void AML_PlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AML_PlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

AML_Tile* AML_PlayerCharacter::GetCurrentTileOn()
{
	return nullptr;
}

