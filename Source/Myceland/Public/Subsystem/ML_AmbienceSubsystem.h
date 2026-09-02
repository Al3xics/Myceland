// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/ObjectKey.h"
#include "ML_AmbienceSubsystem.generated.h"

class AML_BoardSpawner;
class UML_MycelandDeveloperSettings;
class UML_WinLoseSubsystem;

UCLASS()
class MYCELAND_API UML_AmbienceSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category="Myceland|Audio|Ambience")
	void StartAmbience();

	UFUNCTION(BlueprintCallable, Category="Myceland|Audio|Ambience")
	void StopAmbience();

	UFUNCTION(BlueprintPure, Category="Myceland|Audio|Ambience")
	int32 GetWonPuzzleCount() const { return WonPuzzleCount; }

	UFUNCTION(BlueprintPure, Category="Myceland|Audio|Ambience")
	int32 GetTotalPuzzleCount() const { return TotalPuzzleCount; }

	UFUNCTION(BlueprintPure, Category="Myceland|Audio|Ambience")
	float GetLivingAmbienceRatio() const;

private:
	UPROPERTY()
	const UML_MycelandDeveloperSettings* DevSettings = nullptr;

	UPROPERTY()
	TObjectPtr<UML_WinLoseSubsystem> WinLoseSubsystem = nullptr;

	TSet<FObjectKey> WonBoards;

	FTimerHandle AmbienceTimerHandle;

	int32 WonPuzzleCount = 0;
	int32 TotalPuzzleCount = 0;
	bool bAmbienceRunning = false;

	UFUNCTION()
	void HandlePuzzleWon();

	void PlayNextAmbienceSound();
	void ScheduleNextAmbienceSound();
	void InitializePuzzleCount();
	int32 GetConfiguredPuzzleCountForCurrentLevel() const;
	FString GetCleanMapName() const;
};
