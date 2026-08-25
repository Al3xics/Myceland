// Copyright Myceland Team, All Rights Reserved.

#include "Tiles/TileBase/ML_TileGrass.h"

#include "Components/ChildActorComponent.h"
#include "Subsystem/ML_WinLoseSubsystem.h"
#include "Tiles/ML_BoardSpawner.h"
#include "Tiles/ML_Tile.h"

AML_TileGrass::AML_TileGrass()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AML_TileGrass::BeginPlay()
{
	Super::BeginPlay();

	OwnerTile = ResolveOwnerTile();

	WinLoseSubsystem = GetWorld()
		? GetWorld()->GetSubsystem<UML_WinLoseSubsystem>()
		: nullptr;

	if (WinLoseSubsystem)
	{
		WinLoseSubsystem->OnWin.RemoveDynamic(
			this,
			&AML_TileGrass::HandleWin
		);

		WinLoseSubsystem->OnWin.AddDynamic(
			this,
			&AML_TileGrass::HandleWin
		);
	}
}

void AML_TileGrass::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (WinLoseSubsystem)
	{
		WinLoseSubsystem->OnWin.RemoveDynamic(
			this,
			&AML_TileGrass::HandleWin
		);
	}

	Super::EndPlay(EndPlayReason);
}

AML_Tile* AML_TileGrass::ResolveOwnerTile() const
{
	if (const UChildActorComponent* ChildActorComponent =
		Cast<UChildActorComponent>(GetParentComponent()))
	{
		return Cast<AML_Tile>(ChildActorComponent->GetOwner());
	}

	return Cast<AML_Tile>(GetOwner());
}

void AML_TileGrass::HandleWin()
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

	AML_BoardSpawner* GrassBoard =
		OwnerTile->GetBoardSpawnerFromTile();

	AML_BoardSpawner* PlayerBoard =
		PlayerTile->GetBoardSpawnerFromTile();

	if (!IsValid(GrassBoard) || !IsValid(PlayerBoard))
	{
		return;
	}

	// Only trigger grass belonging to the board that was just completed.
	if (GrassBoard != PlayerBoard)
	{
		return;
	}

	SwitchAlive();
}