// Copyright Myceland Team, All Rights Reserved.


#include "Subsystem/ML_WinLoseSubsystem.h"

#include "Core/CoreData.h"
#include "Tiles/ML_TileBase.h"
#include "Tiles/ML_Tile.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Player/ML_PlayerCharacter.h"

FML_GameResult UML_WinLoseSubsystem::CheckWinLose()
{
	if (CurrentBoardSpawner == nullptr)
	{
		CurrentBoardSpawner = FindBoardSpawner();
	}
	
	return FML_GameResult();
}

bool UML_WinLoseSubsystem::CheckPlayerKilled(AML_Tile* CurrentTileOn)
{
	return false;
}

AML_BoardSpawner* UML_WinLoseSubsystem::FindBoardSpawner() const
{
	UWorld* World = GetWorld();
	if (!World) return nullptr;

	AML_PlayerCharacter* PlayerCharacter = Cast<AML_PlayerCharacter>(UGameplayStatics::GetPlayerCharacter(World, 0));
	if (!PlayerCharacter) return nullptr;
	
	AML_Tile* ChildTile = PlayerCharacter->GetCurrentTileOn();
	if (!IsValid(ChildTile)) return nullptr;

	AActor* BoardActor = ChildTile->GetAttachParentActor();
	if (!BoardActor) return nullptr;

	AML_BoardSpawner* RetrivedBoardSpawner = Cast<AML_BoardSpawner>(BoardActor);
	if (!RetrivedBoardSpawner) return nullptr;
	
	return RetrivedBoardSpawner;
}
