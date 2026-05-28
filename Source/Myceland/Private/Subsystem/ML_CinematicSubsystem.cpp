#include "Subsystem/ML_CinematicSubsystem.h"

#include "GameFramework/PlayerController.h"
#include "LevelSequence.h"
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
	APlayerController* PlayerController
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
		StopCurrentCinematic();
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
		return;
	}

	CurrentSequencePlayer->OnFinished.AddDynamic(
		this,
		&UML_CinematicSubsystem::HandleCinematicFinished
	);

	CurrentSequencePlayer->Play();
}

void UML_CinematicSubsystem::StopCurrentCinematic()
{
	FinishCinematic(bIsCinematicPlaying);
}

void UML_CinematicSubsystem::HandleCinematicFinished()
{
	FinishCinematic(true);
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
}

void UML_CinematicSubsystem::RestorePlayerInput()
{
	if (!IsValid(CachedPlayerController))
	{
		return;
	}

	CachedPlayerController->EnableInput(CachedPlayerController);
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
}