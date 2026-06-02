// Copyright Myceland Team, All Rights Reserved.


#include "Player/ML_PlayerCharacter.h"

#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "FMODAudioComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/ML_PlayerController.h"
#include "Save System/ML_SaveSubsystem.h"
#include "Tiles/ML_Tile.h"
#include "Tiles/ML_TileBase.h"


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
	
	AudioComponent = CreateDefaultSubobject<UFMODAudioComponent>(TEXT("FMODAudioComponent"));
	AudioComponent->SetupAttachment(RootComponent);
}

void AML_PlayerCharacter::UpdateCurrentTile()
{
	const FVector ActorLocation = GetActorLocation();

	float CapsuleRadius;
	float CapsuleHalfHeight;
	GetCapsuleComponent()->GetScaledCapsuleSize(CapsuleRadius, CapsuleHalfHeight);

	FVector Start = ActorLocation;
	FVector End = ActorLocation;
	End.Z = ActorLocation.Z - CapsuleHalfHeight - RaycastDistance;

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
	
	AML_Tile* OldTile = CurrentTileOn;
	AML_Tile* NewTile = nullptr;

	if (bHit)
		if (AActor* HitActor = Hit.GetActor())
			if (AML_TileBase* TileBase = Cast<AML_TileBase>(HitActor))
				if (AActor* ParentActor = TileBase->GetAttachParentActor())
					NewTile = Cast<AML_Tile>(ParentActor);
			else
				NewTile = Cast<AML_Tile>(HitActor);

	if (NewTile == OldTile)
		return;

	CurrentTileOn = NewTile;
	OnCurrentTileChanged.Broadcast(OldTile, NewTile);
	HandleTileStateChange(OldTile, NewTile);
}

void AML_PlayerCharacter::HandleTileStateChange(const AML_Tile* OldTile, const AML_Tile* NewTile) const
{
	AML_BoardSpawner* OldBoard = OldTile ? OldTile->GetBoardSpawnerFromTile() : nullptr;
	AML_BoardSpawner* NewBoard = NewTile ? NewTile->GetBoardSpawnerFromTile() : nullptr;

	// No meaningful change.
	if (OldTile == NewTile)
		return;

	// Same board => internal movement, do not trigger camera / board change logic.
	if (OldBoard && NewBoard && OldBoard == NewBoard)
		return;

	// If we changed directly from one board to another, keep it.
	// If we genuinely crossed a gate, keep it too.
	OnBoardChanged.Broadcast(OldTile, NewTile);
}

void AML_PlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	MycelandController = Cast<AML_PlayerController>(GetController());

	// Defer by one tick: board BeginPlay calls (which restore solved grid state) all
	// happen during the same frame, so we wait until they have all finished before
	// querying which board's exit tile we should land on.
	GetWorld()->GetTimerManager().SetTimerForNextTick(this,
		&AML_PlayerCharacter::ApplySavedSpawnPosition);
}

void AML_PlayerCharacter::ApplySavedSpawnPosition()
{
	const UGameInstance* GI = GetGameInstance();
	if (!GI) return;

	const UML_SaveSubsystem* SaveSys = GI->GetSubsystem<UML_SaveSubsystem>();
	if (!SaveSys) return;

	const FName LastPuzzle = SaveSys->GetLastSolvedPuzzleID();
	if (LastPuzzle.IsNone()) return;

	UWorld* World = GetWorld();
	if (!World) return;

	for (TActorIterator<AML_BoardSpawner> It(World); It; ++It)
	{
		AML_BoardSpawner* Board = *It;
		if (!IsValid(Board)) continue;
		if (Board->PuzzleID.GetTagName() != LastPuzzle) continue;

		// Found the board — use the exit tile of its first water path as the spawn point.
		if (Board->WaterPaths.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("[PlayerCharacter] Last solved board '%s' has no WaterPaths — skipping spawn restore."), *LastPuzzle.ToString());
			break;
		}

		const AML_Tile* ExitTile = Board->WaterPaths[0].ExitTile.Get();
		if (!IsValid(ExitTile))
		{
			UE_LOG(LogTemp, Warning, TEXT("[PlayerCharacter] Last solved board '%s' exit tile is invalid — skipping spawn restore."), *LastPuzzle.ToString());
			break;
		}

		// Place the player just above the exit tile so the character controller
		// settles onto the surface naturally.
		float CapsuleHalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const FVector SpawnLocation = ExitTile->GetActorLocation() + FVector(0.f, 0.f, CapsuleHalfHeight + 10.f);
		TeleportTo(SpawnLocation, GetActorRotation());

		UE_LOG(LogTemp, Log, TEXT("[PlayerCharacter] Restored spawn to exit tile of puzzle '%s'."), *LastPuzzle.ToString());
		break;
	}
}

void AML_PlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	// Only check if the character has moved
	FVector CurrentLocation = GetActorLocation();
	CurrentLocation.Z = 0.f;
	if (!LastCheckedLocation.Equals(CurrentLocation, 10.f)) // Tolerance of 10 units
	{
		UpdateCurrentTile();
		LastCheckedLocation = CurrentLocation;
	}
}

void AML_PlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}
