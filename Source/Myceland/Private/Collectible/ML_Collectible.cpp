// Copyright Myceland Team, All Rights Reserved.

#include "Collectible/ML_Collectible.h"

#include "Audio/ML_FMODEvents.h"
#include "Components/SphereComponent.h"
#include "Developer Settings/ML_MycelandDeveloperSettings.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Player/ML_PlayerCharacter.h"
#include "Player/ML_PlayerController.h"
#include "Subsystem/ML_SoundSubsystem.h"
#include "Tiles/ML_Tile.h"


AML_Collectible::AML_Collectible()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->SetupAttachment(RootComponent);
	Collision->InitSphereRadius(50.f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionResponseToAllChannels(ECR_Overlap);
	Collision->SetGenerateOverlapEvents(true);
}

void AML_Collectible::DestroyCollectible()
{
	BeforeDestroyCollectible(OwningTile);
	
	if (OwningTile && OwningTile->CollectibleActor == this)
		OwningTile->CollectibleActor = nullptr;
	
	Destroy();
}

void AML_Collectible::BeginPlay()
{
	Super::BeginPlay();
}

void AML_Collectible::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopWaitingForSourceParasite();

	// Picked up or destroyed mid-flight: the Blueprint will never reach the end of its timeline, so report
	// here instead. Without this the propagation gate would hold on an actor that can no longer answer.
	NotifySpawnAnimationFinished();

	Super::EndPlay(EndPlayReason);
}

void AML_Collectible::NotifySpawnAnimationFinished()
{
	if (bSpawnAnimationFinished) return;

	bSpawnAnimationFinished = true;

	if (UWorld* World = GetWorld())
		World->GetTimerManager().ClearTimer(SpawnAnimationFallbackTimer);

	OnSpawnAnimationFinished.Broadcast(this);
}

void AML_Collectible::PrepareForSpawnSequence()
{
	bHiddenUntilSpawnSequence = true;
	SetActorHiddenInGame(true);

	if (Collision)
		Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AML_Collectible::WaitForSourceParasite(const float TimeoutSeconds)
{
	// The pickup overlap runs during FinishSpawning, so the caller can be holding an actor that was
	// already collected and destroyed.
	if (bSpawnSequenceStarted || !IsValid(this) || IsActorBeingDestroyed()) return;

	// No source (rollback restores a collectible without one) or the parasite is already grown: the
	// collectible has nothing to wait for.
	if (!IsValid(SourceParasite) || SourceParasite->IsParasiteReady())
	{
		BeginSpawnSequence();
		return;
	}

	// Unique rather than plain Add: a stale binding on the same tile would otherwise pile up silently.
	SourceParasite->OnParasiteReady.AddUniqueDynamic(this, &AML_Collectible::HandleSourceParasiteReady);

	if (TimeoutSeconds > 0.f)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				SpawnSequenceFallbackTimer,
				this,
				&AML_Collectible::BeginSpawnSequence,
				TimeoutSeconds,
				false
			);
		}
	}
}

void AML_Collectible::HandleSourceParasiteReady(AML_Tile* Tile)
{
	BeginSpawnSequence();
}

void AML_Collectible::BeginSpawnSequence()
{
	if (bSpawnSequenceStarted) return;
	bSpawnSequenceStarted = true;

	StopWaitingForSourceParasite();

	if (bHiddenUntilSpawnSequence)
	{
		bHiddenUntilSpawnSequence = false;
		SetActorHiddenInGame(false);
	}

	// Sounded here rather than at spawn: the actor now exists well before it is seen.
	if (UML_SoundSubsystem* SoundSubsystem = UML_SoundSubsystem::Get(this))
	{
		SoundSubsystem->StartSound2DByPath(MLFMODEvents::EnergySpawn);
	}

	StartSpawnAnimation();

	// The end of the flight is reported by the Blueprint through NotifySpawnAnimationFinished. If that call
	// is not wired, report by ourselves after roughly the flight duration: a wave waiting on us would
	// otherwise stall until its own settle timeout, which is far longer and reads as a freeze.
	const UML_MycelandDeveloperSettings* Settings = UML_MycelandDeveloperSettings::GetMycelandDeveloperSettings();
	if (Settings && Settings->CollectibleSpawnAnimationTimeout > 0.f)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				SpawnAnimationFallbackTimer,
				this,
				&AML_Collectible::NotifySpawnAnimationFinished,
				Settings->CollectibleSpawnAnimationTimeout,
				false
			);
		}
	}

	// From here the flight owns the collision: off while flying, on when it lands. With no source parasite
	// there is no flight, so nothing would ever turn it back on and the collectible would be unpickable.
	if (!IsValid(SourceParasite) && Collision)
		Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

void AML_Collectible::StopWaitingForSourceParasite()
{
	if (IsValid(SourceParasite))
		SourceParasite->OnParasiteReady.RemoveDynamic(this, &AML_Collectible::HandleSourceParasiteReady);

	if (UWorld* World = GetWorld())
		World->GetTimerManager().ClearTimer(SpawnSequenceFallbackTimer);
}

void AML_Collectible::AddEnergy(AML_PlayerController* MycelandController, AML_PlayerCharacter* MycelandCharacter)
{
	if (!MycelandController || !MycelandCharacter || !MycelandCharacter->CurrentTileOn)
	{
		return;
	}

	MycelandController->EnergyComponent->AddEnergy(+1);

	if (UML_SoundSubsystem* SoundSubsystem = UML_SoundSubsystem::Get(this))
	{
		SoundSubsystem->StartSound2DByPath(MLFMODEvents::EnergyCollect);
	}

	if (OwningTile)
	{
		// Record the pickup only during a normal move (not during undo playback).
		if (!MycelandController->IsUndoMovePlayback())
		{
			const FIntPoint PickedAxial = OwningTile->GetAxialCoord();
			MycelandController->NotifyCollectiblePickedOnAxial(PickedAxial);
		}

		OwningTile->CollectibleActor = nullptr;
		OwningTile->SetHasCollectible(false);
		OwningTile = nullptr;
	}
	DestroyCollectible();
}

// bool AML_Collectible::CheckIsOwningTile(AML_PlayerCharacter* MycelandCharacter)
// {
// 	return MycelandCharacter->CurrentTileOn == OwningTile;
// }
