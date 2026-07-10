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
class MYCELAND_API UML_WinPropagationSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	// Ticks only while a win ring is being time-sliced across frames (see IsTickable).
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;

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

	// Read cursor into WinWaveEntries (consumed in place, no removal).
	int32 WinWaveIndex = 0;

	// Time-slicing state: a ring (all entries at the same DistanceFromOrigin) is
	// applied under a per-frame CPU budget (DevSettings->WinFrameBudgetMs).
	// If the ring doesn't fit in one frame, the remainder continues in Tick.
	bool bWinRingInProgress = false;
	int32 CurrentWinRingDistance = 0;

	FTimerHandle WinWaveTimerHandle;

	UFUNCTION()
	void HandleBoardPropagationOnWin();

	void RunWinWave();
	void ProcessWinRingSlice(double Deadline);
	double MakeWinSliceDeadline() const;
};
