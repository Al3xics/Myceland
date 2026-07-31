#include "Subsystem/ML_CinematicSubsystem.h"

#include "GameFramework/PlayerController.h"
#include "LevelSequence.h"
#include "Camera/CameraActor.h"
#include "Player/ML_PlayerController.h"
#include "Player/ML_PlayerCharacter.h"
#include "Tiles/ML_BoardSpawner.h"
#include "Tiles/ML_Tile.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieSceneSequencePlayer.h"
#include "TimerManager.h"

bool UML_CinematicSubsystem::PlayCinematic(
	ULevelSequence* LevelSequence,
	AActor* BlendCamera,
	float BlendTime,
	float ReturnBlendTime,
	bool bWaitForBlendToFinish,
	APlayerController* PlayerController,
	float PlayRate,
	bool bQueueIfBusy,
	bool bRecenterCameraWhenDone
)
{
	if (!IsValid(LevelSequence))
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	if (bIsCinematicPlaying)
	{
		// Queue mode: don't interrupt the running cinematic — remember this request and play it
		// once the current one finishes (see TryPlayNextQueued). Otherwise, keep the old behavior
		// of stopping the current cinematic and starting this one immediately.
		if (bQueueIfBusy)
		{
			FML_QueuedCinematic Queued;
			Queued.LevelSequence         = LevelSequence;
			Queued.BlendCamera           = BlendCamera;
			Queued.PlayerController       = PlayerController;
			Queued.BlendTime             = BlendTime;
			Queued.ReturnBlendTime       = ReturnBlendTime;
			Queued.bWaitForBlendToFinish = bWaitForBlendToFinish;
			Queued.PlayRate              = PlayRate;
			Queued.bRecenterCameraWhenDone = bRecenterCameraWhenDone;
			CinematicQueue.Add(Queued);
			return true;
		}

		// Interrupt the current cinematic but keep the queue intact — a non-queued cinematic
		// takes over now, and the queue resumes when it finishes. (Only an explicit
		// StopCurrentCinematic() cancels the queue.)
		FinishCinematic(bIsCinematicPlaying);
	}

	APlayerController* TargetPlayerController = PlayerController ? PlayerController : World->GetFirstPlayerController();
	if (!IsValid(TargetPlayerController))
	{
		return false;
	}

	bIsCinematicPlaying = true;
	PendingLevelSequence = LevelSequence;
	CachedPlayerController = TargetPlayerController;
	OriginalViewTarget = TargetPlayerController->GetViewTarget();
	CachedReturnBlendTime = ReturnBlendTime;
	CachedPlayRate = PlayRate;
	bCachedRecenterCameraWhenDone = bRecenterCameraWhenDone;

	DisablePlayerInput(TargetPlayerController);

	if (IsValid(BlendCamera) && BlendTime > 0.f)
	{
		TargetPlayerController->SetViewTargetWithBlend(
			BlendCamera,
			BlendTime,
			VTBlend_Cubic
		);

		if (bWaitForBlendToFinish)
		{
			World->GetTimerManager().SetTimer(
				BlendTimerHandle,
				this,
				&UML_CinematicSubsystem::StartPendingCinematic,
				BlendTime,
				false
			);
		}
		else
		{
			StartPendingCinematic();
		}
	}
	else
	{
		StartPendingCinematic();
	}

	OnCinematicStarted.Broadcast();
	return true;
}

void UML_CinematicSubsystem::StartPendingCinematic()
{
	UWorld* World = GetWorld();
	if (!World || !IsValid(PendingLevelSequence))
	{
		FinishCinematic(true);
		TryPlayNextQueued(); // don't let a bad entry stall the queue
		return;
	}

	FMovieSceneSequencePlaybackSettings PlaybackSettings;
	PlaybackSettings.bAutoPlay = false;

	ALevelSequenceActor* OutSequenceActor = nullptr;

	CurrentSequencePlayer = ULevelSequencePlayer::CreateLevelSequencePlayer(
		World,
		PendingLevelSequence,
		PlaybackSettings,
		OutSequenceActor
	);

	CurrentSequenceActor = OutSequenceActor;

	if (!IsValid(CurrentSequencePlayer))
	{
		FinishCinematic(true);
		TryPlayNextQueued(); // don't let a bad entry stall the queue
		return;
	}

	CurrentSequencePlayer->OnFinished.AddDynamic(
		this,
		&UML_CinematicSubsystem::HandleCinematicFinished
	);

	// Applied before Play() so the whole sequence (and every Sequencer event it fires,
	// including the one that calls Revive) still runs in order — just fast-forwarded.
	CurrentSequencePlayer->SetPlayRate(CachedPlayRate);

	CurrentSequencePlayer->Play();
}

void UML_CinematicSubsystem::StopCurrentCinematic()
{
	// A deliberate stop cancels anything still queued too — otherwise a pending cinematic
	// would surprise-play later. (Queue draining only happens on a natural finish.)
	CinematicQueue.Empty();
	FinishCinematic(bIsCinematicPlaying);
}

void UML_CinematicSubsystem::HandleCinematicFinished()
{
	// Capture before FinishCinematic() clears the cached flags.
	const bool bShouldRecenter = bCachedRecenterCameraWhenDone;

	FinishCinematic(true);

	// A cinematic just ended on its own — start the next queued one, if any. When nothing else is
	// queued the whole run is over; only then, and only if this run was an on-load replay that asked
	// for it, pull the camera back onto the player's board. Normal win cinematics leave the flag
	// false, so finishing one during gameplay never moves the camera.
	if (!TryPlayNextQueued() && bShouldRecenter)
	{
		RecenterCameraOnPlayer();
	}
}

bool UML_CinematicSubsystem::TryPlayNextQueued()
{
	if (bIsCinematicPlaying || CinematicQueue.Num() == 0)
	{
		return false;
	}

	FML_QueuedCinematic Next = CinematicQueue[0];
	CinematicQueue.RemoveAt(0);

	// bQueueIfBusy=false: nothing is playing now, so this one starts immediately.
	PlayCinematic(
		Next.LevelSequence,
		Next.BlendCamera,
		Next.BlendTime,
		Next.ReturnBlendTime,
		Next.bWaitForBlendToFinish,
		Next.PlayerController,
		Next.PlayRate,
		/*bQueueIfBusy=*/false,
		Next.bRecenterCameraWhenDone
	);

	return true;
}

void UML_CinematicSubsystem::RecenterCameraOnPlayer()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	AML_PlayerController* PlayerController = Cast<AML_PlayerController>(World->GetFirstPlayerController());
	if (!IsValid(PlayerController))
	{
		return;
	}

	AML_PlayerCharacter* PlayerCharacter = PlayerController->GetMycelandCharacter();
	if (!IsValid(PlayerCharacter) || !IsValid(PlayerCharacter->CurrentTileOn))
	{
		return;
	}

	// On load the player is spawned onto a solved board, so grab that board's associated camera and
	// switch the view straight to it — same as the controller's InsideBoard settle — instead of
	// leaving the camera parked on a cine camera wherever the win cinematics ended.
	const AML_BoardSpawner* Board = PlayerCharacter->CurrentTileOn->GetBoardSpawnerFromTile();
	if (!IsValid(Board))
	{
		return;
	}

	ACameraActor* BoardCamera = Board->GetAssociatedCamera();
	if (!IsValid(BoardCamera))
	{
		return;
	}

	PlayerController->SetViewTarget(BoardCamera);
}

void UML_CinematicSubsystem::FinishCinematic(bool bBroadcastFinished)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BlendTimerHandle);
	}

	if (CurrentSequencePlayer)
	{
		CurrentSequencePlayer->OnFinished.RemoveDynamic(
			this,
			&UML_CinematicSubsystem::HandleCinematicFinished
		);

		CurrentSequencePlayer->Stop();
	}

	if (IsValid(CachedPlayerController) && IsValid(OriginalViewTarget))
	{
		CachedPlayerController->SetViewTargetWithBlend(
			OriginalViewTarget,
			CachedReturnBlendTime,
			VTBlend_Cubic
		);
	}

	RestorePlayerInput();

	if (CurrentSequenceActor)
	{
		CurrentSequenceActor->Destroy();
	}

	ClearCinematicReferences();

	if (bBroadcastFinished)
	{
		OnCinematicFinished.Broadcast();
	}
}

void UML_CinematicSubsystem::DisablePlayerInput(APlayerController* PlayerController)
{
	if (!IsValid(PlayerController))
	{
		return;
	}

	PlayerController->DisableInput(PlayerController);

	// Hide the cursor and stop the hover/path previews while the cinematic plays.
	if (AML_PlayerController* MLController = Cast<AML_PlayerController>(PlayerController))
		MLController->NotifyCinematicModeChanged(true);
}

void UML_CinematicSubsystem::RestorePlayerInput()
{
	if (!IsValid(CachedPlayerController))
	{
		return;
	}

	CachedPlayerController->EnableInput(CachedPlayerController);

	if (AML_PlayerController* MLController = Cast<AML_PlayerController>(CachedPlayerController))
		MLController->NotifyCinematicModeChanged(false);
}

void UML_CinematicSubsystem::ClearCinematicReferences()
{
	PendingLevelSequence = nullptr;
	CurrentSequencePlayer = nullptr;
	CurrentSequenceActor = nullptr;
	CachedPlayerController = nullptr;
	OriginalViewTarget = nullptr;

	bIsCinematicPlaying = false;
	CachedReturnBlendTime = 0.75f;
	CachedPlayRate = 1.0f;
	bCachedRecenterCameraWhenDone = false;
}