// Copyright Myceland Team, All Rights Reserved.


#include "Player/ML_PlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Tiles/ML_Tile.h"


AML_PlayerCharacter::AML_PlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	
	GetCharacterMovement()->GravityScale = 1.5f;
	GetCharacterMovement()->MaxAcceleration = 1000.f;
	GetCharacterMovement()->BrakingFrictionFactor = 1.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 1000.f;
	GetCharacterMovement()->PerchRadiusThreshold = 20.f;
	GetCharacterMovement()->bUseFlatBaseForFloorChecks = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 640.f, 0.f);
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->bConstrainToPlane = true;
	GetCharacterMovement()->bSnapToPlaneAtStart = true;
	
	SpringArm = CreateDefaultSubobject<USpringArmComponent>("SpringArm");
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->SetRelativeRotation(FRotator(-50.f, 0.f, 0.f));
	SpringArm->TargetArmLength = 1400.f;
	SpringArm->bDoCollisionTest = false;
	SpringArm->bEnableCameraLag = true;
	SpringArm->CameraLagSpeed = 3.f;
	
	Camera = CreateDefaultSubobject<UCameraComponent>("Camera");
	Camera->SetupAttachment(SpringArm);
	Camera->FieldOfView = 55.f;
}

void AML_PlayerCharacter::UpdateCurrentTile()
{
	const FVector ActorLocation = GetActorLocation();

	float CapsuleRadius;
	float CapsuleHalfHeight;
	GetCapsuleComponent()->GetScaledCapsuleSize(CapsuleRadius, CapsuleHalfHeight);

	FVector Start = ActorLocation;
	FVector End = ActorLocation;
	End.Z = (ActorLocation.Z - (CapsuleHalfHeight - 10.f)) - 20.f;

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	Params.bTraceComplex = false;

	bool bHit = GetWorld()->LineTraceSingleByChannel(
		Hit,
		Start,
		End,
		ECC_Visibility,
		Params
	);

	AML_Tile* NewTile = nullptr;

	if (bHit)
		NewTile = Cast<AML_Tile>(Hit.GetActor());

	if (NewTile == CurrentTileOn)
		return;

	if (CurrentTileOn)
	{
		for (AML_Tile* Neighbor : CurrentTileOn->GetBoardSpawnerFromTile()->GetNeighbors(CurrentTileOn))
		{
			if (Neighbor)
				Neighbor->StopGlowing();
		}
	}

	CurrentTileOn = NewTile;

	if (CurrentTileOn)
	{
		for (AML_Tile* Neighbor : CurrentTileOn->GetBoardSpawnerFromTile()->GetNeighbors(CurrentTileOn))
		{
			if (Neighbor && Neighbor->IsBlocked() == false)
				Neighbor->Glow();
		}
	}
}

void AML_PlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
}

void AML_PlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateCurrentTile();
}

void AML_PlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

