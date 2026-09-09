#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ML_CinematicSubsystem.generated.h"

class AActor;
class ALevelSequenceActor;
class APlayerController;
class ULevelSequence;
class ULevelSequencePlayer;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCinematicStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCinematicFinished);

// A queued PlayCinematic request. Captures every argument so a cinematic that arrives while
// another is playing (with bQueueIfBusy) can be replayed identically once the current one ends.
USTRUCT()
struct FML_QueuedCinematic
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	ULevelSequence* LevelSequence = nullptr;

	UPROPERTY(Transient)
	AActor* BlendCamera = nullptr;

	UPROPERTY(Transient)
	APlayerController* PlayerController = nullptr;

	float BlendTime = 1.0f;
	float ReturnBlendTime = 0.75f;
	bool bWaitForBlendToFinish = true;
	float PlayRate = 1.0f;

	// When true, once this cinematic (and the rest of the queue) finishes, the camera is snapped
	// back onto the player's board camera. Set only by the on-load solved-board replays.
	bool bRecenterCameraWhenDone = false;
};

UCLASS()
class MYCELAND_API UML_CinematicSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Myceland Cinematics")
	bool PlayCinematic(
	ULevelSequence* LevelSequence,
	AActor* BlendCamera = nullptr,
	float BlendTime = 1.0f,
	float ReturnBlendTime = 0.75f,
	bool bWaitForBlendToFinish = true,
	APlayerController* PlayerController = nullptr,
	float PlayRate = 1.0f,
	bool bQueueIfBusy = false,
	bool bRecenterCameraWhenDone = false
	);

	UFUNCTION(BlueprintCallable, Category = "Myceland Cinematics")
	void StopCurrentCinematic();

	UFUNCTION(BlueprintPure, Category = "Myceland Cinematics")
	bool IsCinematicPlaying() const { return bIsCinematicPlaying; }

	UPROPERTY(BlueprintAssignable, Category = "Myceland Cinematics")
	FOnCinematicStarted OnCinematicStarted;

	UPROPERTY(BlueprintAssignable, Category = "Myceland Cinematics")
	FOnCinematicFinished OnCinematicFinished;

private:
	UPROPERTY(Transient)
	ULevelSequence* PendingLevelSequence = nullptr;

	UPROPERTY(Transient)
	ULevelSequencePlayer* CurrentSequencePlayer = nullptr;

	UPROPERTY(Transient)
	ALevelSequenceActor* CurrentSequenceActor = nullptr;

	UPROPERTY(Transient)
	APlayerController* CachedPlayerController = nullptr;

	UPROPERTY(Transient)
	AActor* OriginalViewTarget = nullptr;

	// Pending cinematics waiting for the current one to finish (FIFO). Filled by PlayCinematic
	// calls made with bQueueIfBusy while another cinematic is active — e.g. several already-solved
	// boards each replaying their win cinematic on load. Drained one-at-a-time by TryPlayNextQueued.
	UPROPERTY(Transient)
	TArray<FML_QueuedCinematic> CinematicQueue;

	FTimerHandle BlendTimerHandle;

	bool bIsCinematicPlaying = false;
	float CachedReturnBlendTime = 0.75f;

	// Play rate applied to the sequence player when it starts (1.0 = normal speed).
	// Used to fast-forward on-load win-cinematic replays so they resolve near-instantly.
	float CachedPlayRate = 1.0f;

	// Whether the currently-playing cinematic asked for the camera to be recentered on the player's
	// board once the whole queue drains. Only the on-load replays set this — normal win cinematics
	// leave it false so finishing one during gameplay never yanks the camera.
	bool bCachedRecenterCameraWhenDone = false;

	void StartPendingCinematic();
	void FinishCinematic(bool bBroadcastFinished);

	// Plays the next queued cinematic, if any and nothing is currently playing. Called after a
	// cinematic finishes naturally so a queue built up on load drains one after another.
	// Returns true if it started another cinematic (queue still draining), false if the queue
	// is empty / nothing else will play.
	bool TryPlayNextQueued();

	// Switches the view to the associated camera of the board the player is standing on. Called once
	// the whole cinematic queue has drained (and only for on-load replays) — the win cinematics leave
	// the camera parked wherever they ended, so we snap the view back to the player's current board.
	void RecenterCameraOnPlayer();

	void DisablePlayerInput(APlayerController* PlayerController);
	void RestorePlayerInput();
	void ClearCinematicReferences();

	UFUNCTION()
	void HandleCinematicFinished();
};