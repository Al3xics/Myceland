// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/ObjectKey.h"
#include "ML_AmbienceSubsystem.generated.h"

class AML_BoardSpawner;
class UML_MycelandDeveloperSettings;
class UML_WinLoseSubsystem;
class UML_SoundPlaybackHandle;

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

	// ==================== Music Layers ====================

	// Starts the music layers: layer 0 (base bed) immediately, plus every layer already
	// unlocked by puzzles solved in a previous session (see SeedStateFromAlreadySolvedBoards).
	// No-op if layers are already playing.
	UFUNCTION(BlueprintCallable, Category="Myceland|Audio|Music")
	void StartMusicLayers();

	// Stops every active music layer.
	UFUNCTION(BlueprintCallable, Category="Myceland|Audio|Music")
	void StopMusicLayers();

	UFUNCTION(BlueprintPure, Category="Myceland|Audio|Music")
	int32 GetActiveMusicLayerCount() const { return ActiveMusicLayerHandles.Num(); }

private:
	UPROPERTY()
	const UML_MycelandDeveloperSettings* DevSettings = nullptr;

	UPROPERTY()
	TObjectPtr<UML_WinLoseSubsystem> WinLoseSubsystem = nullptr;

	UPROPERTY()
	TArray<TObjectPtr<UML_SoundPlaybackHandle>> ActiveMusicLayerHandles;

	TSet<FObjectKey> WonBoards;

	FTimerHandle AmbienceTimerHandle;
	FTimerHandle SeedStateTimerHandle;

	int32 WonPuzzleCount = 0;
	int32 TotalPuzzleCount = 0;
	bool bAmbienceRunning = false;

	UFUNCTION()
	void HandlePuzzleWon();

	void PlayNextAmbienceSound();
	void ScheduleNextAmbienceSound();
	void InitializePuzzleCount();

	// Runs one tick after OnWorldBeginPlay — by then every AML_BoardSpawner's own BeginPlay
	// (which restores bIsPuzzleSolved from the save) has already run. Seeds WonPuzzleCount /
	// WonBoards from boards that were already solved in a previous session — otherwise a level
	// re-entered mid-playthrough would sound freshly dead and start the music back at layer 0 —
	// then starts ambience and music layers so they reflect real progress from the first sound.
	void SeedStateFromAlreadySolvedBoards();

	// Starts the next not-yet-playing entry in MusicLayerEventPaths, if any remain.
	void UnlockNextMusicLayer();

	int32 GetConfiguredPuzzleCountForCurrentLevel() const;
	FString GetCleanMapName() const;
};
