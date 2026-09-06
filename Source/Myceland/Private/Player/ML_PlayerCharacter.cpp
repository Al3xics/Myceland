// Copyright Myceland Team, All Rights Reserved.


#include "Player/ML_PlayerCharacter.h"

#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "FMODAudioComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/ML_HexPathfinder.h"
#include "Player/ML_PlayerController.h"
#include "Save System/ML_SaveSubsystem.h"
#include "TechArt/ML_NatureZone.h"
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

	// ---- Step 1: teleport onto the last solved board's first exit tile and sync CurrentTileOn ----
	// bPlaced gates the cinematic replay below: if we never land on a valid tile, CurrentTileOn stays
	// null and replaying the win cinematics would reproduce the very "Accessed None ... CurrentTileOn"
	// error we're preventing (BP_ProgressionManager reads it on OnCinematicFinished), so we skip them.
	bool bPlaced = false;
	for (TActorIterator<AML_BoardSpawner> It(World); It; ++It)
	{
		AML_BoardSpawner* Board = *It;
		if (!IsValid(Board)) continue;
		if (Board->PuzzleID.GetTagName() != LastPuzzle) continue;

		// Pick the spawn tile with a walkability fallback chain, since a solved board can leave its
		// authored first exit tile non-walkable (blocked / flooded), and standing the player there
		// would trap them:
		//   1. the first walkable ExitTile across all BoardExits (in order),
		//   2. failing that, the first walkable water-path tile (Exit then Entry, in order).
		const AML_Tile* SpawnTile = nullptr;

		for (const FML_BoardExit& Exit : Board->BoardExits)
		{
			for (const TObjectPtr<AML_Tile>& ExitTile : Exit.ExitTiles)
			{
				if (UML_HexPathfinder::IsTileWalkable(ExitTile.Get()))
				{
					SpawnTile = ExitTile.Get();
					break;
				}
			}
			if (SpawnTile) break;
		}

		// Final fallback: no walkable exit tile — try the water-path tiles.
		if (!SpawnTile)
		{
			for (const FML_WaterPath& WaterPath : Board->WaterPaths)
			{
				if (UML_HexPathfinder::IsTileWalkable(WaterPath.ExitTile.Get()))
				{
					SpawnTile = WaterPath.ExitTile.Get();
					break;
				}
				if (UML_HexPathfinder::IsTileWalkable(WaterPath.EntryTile.Get()))
				{
					SpawnTile = WaterPath.EntryTile.Get();
					break;
				}
			}
		}

		if (!IsValid(SpawnTile))
		{
			UE_LOG(LogTemp, Warning, TEXT("[PlayerCharacter] Last solved board '%s' has no walkable exit or water-path tile — skipping spawn restore."), *LastPuzzle.ToString());
			break;
		}

		// Place the player just above the tile so the character controller settles onto the surface.
		const float CapsuleHalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const FVector SpawnLocation = SpawnTile->GetActorLocation() + FVector(0.f, 0.f, CapsuleHalfHeight + 10.f);
		TeleportTo(SpawnLocation, GetActorRotation());

		// Force the tile lookup NOW (synchronously) instead of waiting for the movement-based check
		// in Tick, so CurrentTileOn points at the tile we just landed on before any cinematic plays.
		UpdateCurrentTile();
		bPlaced = true;

		UE_LOG(LogTemp, Log, TEXT("[PlayerCharacter] Restored spawn to walkable tile of puzzle '%s'."), *LastPuzzle.ToString());
		break;
	}

	// ---- Step 2: revive the nature zones of every solved board ----
	// Instead of replaying the win cinematics on load, we directly revitalize each solved board's
	// nature zones. Revive() is a BlueprintNativeEvent authored in the nature-zone Blueprint, so the
	// grown foliage is restored instantly without the camera-hijacking cinematics. Gated on bPlaced
	// so CurrentTileOn is valid first (keeps BP_ProgressionManager from reading a null tile).
	if (bPlaced)
	{
		for (TActorIterator<AML_BoardSpawner> It(World); It; ++It)
		{
			AML_BoardSpawner* Board = *It;
			if (!IsValid(Board) || !Board->bIsPuzzleSolved) continue;

			for (AActor* ZoneActor : Board->GetAssociatedNatureZones())
			{
				if (AML_NatureZone* Zone = Cast<AML_NatureZone>(ZoneActor))
					Zone->Revive();
			}
		}
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
