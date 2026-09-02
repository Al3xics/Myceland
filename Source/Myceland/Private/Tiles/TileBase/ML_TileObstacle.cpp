// Copyright Myceland Team, All Rights Reserved.

#include "Tiles/TileBase/ML_TileObstacle.h"

#include "Components/ChildActorComponent.h"
#include "Subsystem/ML_WinLoseSubsystem.h"
#include "Tiles/ML_BoardSpawner.h"
#include "Tiles/ML_Tile.h"

AML_TileObstacle::AML_TileObstacle()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AML_TileObstacle::BeginPlay()
{
	Super::BeginPlay();

	OwnerTile = ResolveOwnerTile();

	WinLoseSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UML_WinLoseSubsystem>() : nullptr;

	if (WinLoseSubsystem)
	{
		WinLoseSubsystem->OnWin.RemoveDynamic(this, &AML_TileObstacle::HandleWin);
		WinLoseSubsystem->OnWin.AddDynamic(this, &AML_TileObstacle::HandleWin);
	}
}

void AML_TileObstacle::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (WinLoseSubsystem)
	{
		WinLoseSubsystem->OnWin.RemoveDynamic(this, &AML_TileObstacle::HandleWin);
	}

	Super::EndPlay(EndPlayReason);
}

AML_Tile* AML_TileObstacle::ResolveOwnerTile() const
{
	if (const UChildActorComponent* ChildActorComponent = Cast<UChildActorComponent>(GetParentComponent()))
	{
		return Cast<AML_Tile>(ChildActorComponent->GetOwner());
	}

	return Cast<AML_Tile>(GetOwner());
}

void AML_TileObstacle::HandleWin()
{
	if (!IsValid(OwnerTile))
	{
		OwnerTile = ResolveOwnerTile();
	}

	if (!IsValid(OwnerTile) || !IsValid(WinLoseSubsystem))
	{
		return;
	}

	AML_Tile* PlayerTile = WinLoseSubsystem->GetPlayerCurrentTile();
	if (!IsValid(PlayerTile))
	{
		return;
	}

	AML_BoardSpawner* ObstacleBoard = OwnerTile->GetBoardSpawnerFromTile();
	AML_BoardSpawner* PlayerBoard = PlayerTile->GetBoardSpawnerFromTile();

	if (!IsValid(ObstacleBoard) || !IsValid(PlayerBoard))
	{
		return;
	}

	if (ObstacleBoard != PlayerBoard)
	{
		return;
	}

	 FTimerHandle TimerHandle;
        GetWorldTimerManager().SetTimer(
            TimerHandle,
            this,
            &AML_TileObstacle::SwitchAlive,
            1.0f,
            false
        );
}