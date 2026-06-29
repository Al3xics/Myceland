// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/ML_CoreData.h"
#include "Subsystems/WorldSubsystem.h"
#include "ML_WinPropagationSubsystem.generated.h"

class UML_WinLoseSubsystem;
class UML_MycelandDeveloperSettings;
class AML_Tile;

UCLASS()
class MYCELAND_API UML_WinPropagationSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

private:
	// =========================================================================
	// Core references
	// =========================================================================

	UPROPERTY()
	UML_WinLoseSubsystem* WinLoseSubsystem = nullptr;

	UPROPERTY()
	const UML_MycelandDeveloperSettings* DevSettings = nullptr;

	bool bDelegateBound = false;

	// =========================================================================
	// Win wave runtime
	// =========================================================================

	// All tiles visited by the BFS, sorted by ring distance from the nearest tree.
	// TargetType == current type means pass-through (event only, no type change).
	UPROPERTY(Transient)
	TArray<FML_WaveChange> WinWaveEntries;

	FTimerHandle WinWaveTimerHandle;

	UFUNCTION()
	void HandleBoardPropagationOnWin();

	void RunWinWave();
};
