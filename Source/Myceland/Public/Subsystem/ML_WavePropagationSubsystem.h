// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ML_WavePropagationSubsystem.generated.h"

struct FML_WaveChange;
class UML_MycelandDeveloperSettings;
class UML_PropagationRulesData;
class AML_Tile;
class AML_PlayerController;
class AML_BoardSpawner;
class UML_WinLoseSubsystem;

UCLASS()
class MYCELAND_API UML_WavePropagationSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

private:
	UPROPERTY()
	UML_WinLoseSubsystem* WinLoseSubsystem;
	
	UPROPERTY(Transient)
	TArray<FML_WaveChange> PendingChanges;
	
	UPROPERTY()
	AML_PlayerController* PlayerController = nullptr;
	
	UPROPERTY()
	const UML_MycelandDeveloperSettings* DevSettings = nullptr;
	
	UPROPERTY(Transient)
	AML_Tile* CurrentOriginTile = nullptr;
	
	UPROPERTY(Transient)
	TArray<AML_Tile*> ParasitesThatAteGrass;
	
	bool bIsResolvingTiles = false;
	int ActiveTilUpdates = 0;
	int32 CurrentWaveIndex = 0;
	bool bCycleHasChanges = false;
	
	void RunWave();
	void ScheduleNextPriority();
	void ProcessNextWave();
	void EndTileResolved();
	
public:
	void EnsureInitialized();
	
	// Entry point for starting the Wave Propagation Cycle
	UFUNCTION(BlueprintCallable, Category="Myceland Wave Propagation")
	void BeginTileResolved(AML_Tile* HitTile);
};
