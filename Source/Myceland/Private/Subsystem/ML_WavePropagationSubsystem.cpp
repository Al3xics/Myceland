// Copyright Myceland Team, All Rights Reserved.


#include "Subsystem/ML_WavePropagationSubsystem.h"

#include "Player/ML_PlayerController.h"
#include "Subsystem/ML_WinLoseSubsystem.h"
#include "Rules/ChildRules/ML_RuleParasiteOnGrass.h"
#include "Rules/ChildRules/ML_RuleGrassAroundWater.h"
#include "Rules/ChildRules/ML_RuleWaterOnParasite.h"

void UML_WavePropagationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	// Get WinLoseSubsystem to keep one reference and not have to get it everytime
	WinLoseSubsystem = GetWorld()->GetSubsystem<UML_WinLoseSubsystem>();
	
	// Initialize Array - cannot be modified - to access `Rules`, use `GetRules()`
	Rules.Add(NewObject<UML_RuleGrassAroundWater>(this));
	Rules.Add(NewObject<UML_RuleParasiteOnGrass>(this));
	Rules.Add(NewObject<UML_RuleWaterOnParasite>(this));
	
	// Get the player controller
	PlayerController = Cast<AML_PlayerController>(GetWorld()->GetFirstPlayerController());
}

void UML_WavePropagationSubsystem::EndTileResolved()
{
	bIsResolvingTiles = false;
	PlayerController->EnableInput(PlayerController);
}

void UML_WavePropagationSubsystem::ComputesChanges()
{
}

void UML_WavePropagationSubsystem::RunWave()
{
}

void UML_WavePropagationSubsystem::RunFullCycle()
{
}

void UML_WavePropagationSubsystem::BeginTileResolved()
{
	bIsResolvingTiles = true;
	PlayerController->DisableInput(PlayerController);
}

void UML_WavePropagationSubsystem::RegisterTileUpdate()
{
	ActiveTilUpdates++;
}

void UML_WavePropagationSubsystem::UnregisterTileUpdate()
{
	ActiveTilUpdates--;
	
	if (ActiveTilUpdates <= 0)
		EndTileResolved();
}

void UML_WavePropagationSubsystem::SetInitialTileChanged(FIntPoint AxialCoord)
{
}
