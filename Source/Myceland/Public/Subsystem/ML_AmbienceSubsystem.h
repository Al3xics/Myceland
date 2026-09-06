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

	// ==================== Music Progression ====================
	// One track plays at a time. The active track index follows the number of puzzles won in
	// this level: 0 won -> track 0 ("Musique 1"), 1 won -> track 1 ("Musique 2"), etc. Winning a
	// puzzle switches (stops the old track, starts the new one) instead of layering on top of it.
	//
	// Replaces the earlier additive-layers approach (kept below, commented out, for reference —
	// it started one extra looping stem per puzzle win and stacked them instead of switching).

	// Starts (or re-syncs) the music track matching the current puzzle-win count.
	UFUNCTION(BlueprintCallable, Category="Myceland|Audio|Music")
	void StartMusicProgression();

	// Stops whatever music track is currently playing.
	UFUNCTION(BlueprintCallable, Category="Myceland|Audio|Music")
	void StopMusicProgression();

	// Index into MusicTrackEventPaths of the track currently playing (INDEX_NONE if none).
	UFUNCTION(BlueprintPure, Category="Myceland|Audio|Music")
	int32 GetCurrentMusicTrackIndex() const { return CurrentMusicTrackIndex; }

	// ---------- Ancienne logique (layers additifs) — conservée en commentaire pour référence ----------
	//
	// UFUNCTION(BlueprintCallable, Category="Myceland|Audio|Music")
	// void StartMusicLayers();
	//
	// UFUNCTION(BlueprintCallable, Category="Myceland|Audio|Music")
	// void StopMusicLayers();
	//
	// UFUNCTION(BlueprintPure, Category="Myceland|Audio|Music")
	// int32 GetActiveMusicLayerCount() const { return ActiveMusicLayerHandles.Num(); }

private:
	UPROPERTY()
	const UML_MycelandDeveloperSettings* DevSettings = nullptr;

	UPROPERTY()
	TObjectPtr<UML_WinLoseSubsystem> WinLoseSubsystem = nullptr;

	// Currently playing music track (Music Progression). Only one at a time.
	UPROPERTY()
	TObjectPtr<UML_SoundPlaybackHandle> CurrentMusicHandle = nullptr;

	int32 CurrentMusicTrackIndex = INDEX_NONE;

	// ---------- Ancienne logique (layers additifs) — conservée en commentaire pour référence ----------
	// UPROPERTY()
	// TArray<TObjectPtr<UML_SoundPlaybackHandle>> ActiveMusicLayerHandles;

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
	// re-entered mid-playthrough would sound freshly dead and restart the music at track 0 —
	// then starts ambience and syncs the music track to the real progress from the first sound.
	void SeedStateFromAlreadySolvedBoards();

	// Stops whatever music track is currently playing and starts the one matching WonPuzzleCount
	// (clamped to the last authored track once progress goes further than the list provides).
	void SwitchToMusicTrackForCurrentProgress();

	// ---------- Ancienne logique (layers additifs) — conservée en commentaire pour référence ----------
	// Starts the next not-yet-playing entry in MusicLayerEventPaths, if any remain.
	// void UnlockNextMusicLayer();

	int32 GetConfiguredPuzzleCountForCurrentLevel() const;
	FString GetCleanMapName() const;
};
