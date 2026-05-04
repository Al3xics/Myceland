#include "Tiles/TileBase/ML_TileObstacle.h"

#include "Subsystems/GameInstanceSubsystem.h"
#include "Components/ChildActorComponent.h"
#include "Kismet/GameplayStatics.h"

#include "Tiles/ML_Tile.h"
#include "Tiles/ML_BoardSpawner.h"
#include "Subsystem/ML_WinLoseSubsystem.h"

AML_TileObstacle::AML_TileObstacle()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AML_TileObstacle::BeginPlay()
{
	Super::BeginPlay();

	OwnerTile = ResolveOwnerTile();

	UML_WinLoseSubsystem* Subsystem = GetWorld()->GetSubsystem<UML_WinLoseSubsystem>();
	if (Subsystem)
	{
		Subsystem->OnWin.AddDynamic(this, &AML_TileObstacle::HandleWin);
	}
}

AML_Tile* AML_TileObstacle::ResolveOwnerTile() const
{
	// CORRECT WAY for ChildActorComponent
	if (const UChildActorComponent* CAC = Cast<UChildActorComponent>(GetParentComponent()))
	{
		return Cast<AML_Tile>(CAC->GetOwner());
	}

	// fallback (rare cases)
	return Cast<AML_Tile>(GetOwner());
}

void AML_TileObstacle::HandleWin()
{
	if (!IsValid(OwnerTile))
	{
		OwnerTile = ResolveOwnerTile();
	}

	if (!IsValid(OwnerTile))
	{
		return;
	}

	// OPTIONAL: if you want board filtering later
	// AML_BoardSpawner* Board = OwnerTile->GetBoardSpawnerFromTile();

	SwitchAlive(); // BP event
}