// Copyright Epic Games, Inc. All Rights Reserved.

#include "MyceliumCharacter.h"
#include "UObject/ConstructorHelpers.h"
#include "Camera/CameraComponent.h"
#include "Components/DecalComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/Material.h"
#include "Engine/World.h"

AMyceliumCharacter::AMyceliumCharacter()
{
	
}

void AMyceliumCharacter::BeginPlay()
{
	Super::BeginPlay();

	// stub
}

void AMyceliumCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

	// stub
}
