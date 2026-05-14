// Copyright Myceland Team, All Rights Reserved.


#include "Subsystem/ML_NarrativeSubsystem.h"

#include "Actors/ML_NarrativeTrigger.h"
#include "Actors/ML_TalkingThing.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "Camera/CameraComponent.h"
#include "Data Asset/ML_NarrativeSequence.h"
#include "Developer Settings/ML_MycelandDeveloperSettings.h"
#include "EnhancedInputSubsystems.h"
#include "FMODAudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"
#include "Player/ML_PlayerController.h"

AML_PlayerController* UML_NarrativeSubsystem::GetPlayerController() const
{
	if (!PlayerController)
	{
		PlayerController = Cast<AML_PlayerController>(UGameplayStatics::GetPlayerController(this, 0));
		ensureMsgf(PlayerController, TEXT("No player controller found"));
	}
	return PlayerController;
}

void UML_NarrativeSubsystem::CleanupCurrentSequence()
{
	// Clear all timers
	GetWorld()->GetTimerManager().ClearTimer(DialogueTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(CameraBlendTimerHandle);

	// Unbind all audio callbacks and stop audio
	if (CurrentSequence && CurrentLineIndex < CurrentSequence->DialogueLines.Num())
	{
		const FDialogueLine& Line = CurrentSequence->DialogueLines[CurrentLineIndex];
		if (IML_DialogueSpeaker* Speaker = GetSpeaker(Line.SpeakerTag))
		{
			if (UFMODAudioComponent* AudioComp = Speaker->GetAudioComponent())
			{
				AudioComp->OnEventStopped.RemoveAll(this);
				if (AudioComp->IsPlaying())
					AudioComp->Stop();
			}
			Speaker->SetIsTalking(false);
		}
	}

	// If in cinematic mode, restore player control
	if (CurrentSequence && CurrentSequence->bIsCinematicMode)
		RestorePlayerControl();

	// Reset all state variables
	CurrentSequence = nullptr;
	CurrentNarrativeTrigger = nullptr;
	CurrentLineIndex = 0;
	bCurrentLineStarted = false;
	bWaitingForCinematicSetup = false;
	bCameraBlendFinished = false;
	bPlayerMovementFinished = false;
}

void UML_NarrativeSubsystem::PlayNextLine()
{
	bCurrentLineStarted = false;

	if (!CurrentSequence || CurrentLineIndex >= CurrentSequence->DialogueLines.Num())
    {
        OnSequenceEnd.Broadcast(CurrentSequence);

		// When cinematic mode is finished, this will enable player input,
		// and replace the camera to the player.
        if (CurrentSequence->bIsCinematicMode)
            RestorePlayerControl();

        CurrentSequence = nullptr;
        CurrentNarrativeTrigger = nullptr;
        CurrentLineIndex = 0;
		bWaitingForCinematicSetup = false;
        return;
    }

    const FDialogueLine& Line = CurrentSequence->DialogueLines[CurrentLineIndex];
    if (Line.PreDelay > 0.f)
    {
        GetWorld()->GetTimerManager().SetTimer(
            DialogueTimerHandle,
            [this, Line]() { StartLine(Line); },
            Line.PreDelay,
            false
        );
    }
    else
    	StartLine(Line);
}

void UML_NarrativeSubsystem::StartLine(const FDialogueLine& Line)
{
	if (!CurrentSequence)
	{
		UE_LOG(LogTemp, Warning, TEXT("StartLine called but CurrentSequence is null"));
		return;
	}
	
	// Validate and set speaker talking
	IML_DialogueSpeaker* Speaker = GetSpeaker(Line.SpeakerTag);
	if (Speaker)
		Speaker->SetIsTalking(true);
	else
	{
		UE_LOG(LogTemp, Error, TEXT("No speaker found for tag %d in line %d of trigger %s"), static_cast<int32>(Line.SpeakerTag), CurrentLineIndex, *CurrentNarrativeTrigger->GetName());
		return;
	}
	
	bCurrentLineStarted = true;
	OnDialogueLineStart.Broadcast(Line, Line.SpeakerTag);

	if (!Line.Sound)
	{
		UE_LOG(LogTemp, Warning, TEXT("No sound found for line %d in sequence %s"), CurrentLineIndex, *CurrentSequence->GetName());
		OnFMODEventStopped();
		return;
	}

	UFMODAudioComponent* AudioComp = Speaker->GetAudioComponent();
	if (!AudioComp)
	{
		UE_LOG(LogTemp, Error, TEXT("Speaker for line %d has no audio component"), CurrentLineIndex);
		OnFMODEventStopped();
		return;
	}

	AudioComp->SetEvent(Line.Sound);
	AudioComp->OnEventStopped.AddDynamic(this, &UML_NarrativeSubsystem::OnFMODEventStopped);
	AudioComp->Play();
}

void UML_NarrativeSubsystem::OnLineFinished()
{
	if (!CurrentSequence) return;

	const FDialogueLine& Line = CurrentSequence->DialogueLines[CurrentLineIndex];

	if (IML_DialogueSpeaker* Speaker = GetSpeaker(Line.SpeakerTag))
	{
		if (UFMODAudioComponent* AudioComp = Speaker->GetAudioComponent())
		{
			// Unbind callback to prevent double-triggering
			AudioComp->OnEventStopped.RemoveAll(this);
			
			if (AudioComp->IsPlaying())
				AudioComp->Stop();
		}
		
		Speaker->SetIsTalking(false);
	}

	OnDialogueLineEnd.Broadcast(Line, Line.SpeakerTag);

	CurrentLineIndex++;
	PlayNextLine();
}

bool UML_NarrativeSubsystem::SkipCurrentLine()
{
	if (!CurrentSequence) return false;
	if (!CurrentSequence->bAllowSkip) return false;

	// In cinematic mode, only allow skip when camera has reached its position
	if (CurrentSequence->bIsCinematicMode && !bCameraBlendFinished)
		return false;

	GetWorld()->GetTimerManager().ClearTimer(DialogueTimerHandle);

	if (bCurrentLineStarted && CurrentLineIndex < CurrentSequence->DialogueLines.Num())
	{
		const FDialogueLine& Line = CurrentSequence->DialogueLines[CurrentLineIndex];

		if (IML_DialogueSpeaker* Speaker = GetSpeaker(Line.SpeakerTag))
		{
			if (UFMODAudioComponent* AudioComp = Speaker->GetAudioComponent())
			{
				// Unbind callback BEFORE stopping to prevent async OnEventStopped call
				AudioComp->OnEventStopped.RemoveAll(this);
				
				if (AudioComp->IsPlaying())
					AudioComp->Stop();
			}
			
			Speaker->SetIsTalking(false);
		}

		OnDialogueLineEnd.Broadcast(Line, Line.SpeakerTag);
	}

	CurrentLineIndex++;
	PlayNextLine();
	return true;
}

void UML_NarrativeSubsystem::SetupCinematicMode()
{
	// Swap IMCs: remove main (no movement/click) and add cinematic-only (skip only)
	const UML_MycelandDeveloperSettings* DevSettings = UML_MycelandDeveloperSettings::GetMycelandDeveloperSettings();
	if (UEnhancedInputLocalPlayerSubsystem* InputSub = GetPlayerController()->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
	{
		if (UInputMappingContext* MainIMC = DevSettings->DefaultInputMappingContext.Mapping.LoadSynchronous())
			InputSub->RemoveMappingContext(MainIMC);

		if (UInputMappingContext* CinematicIMC = DevSettings->CinematicInputMappingContext.Mapping.LoadSynchronous())
			InputSub->AddMappingContext(CinematicIMC, DevSettings->CinematicInputMappingContext.Priority);
	}

	UAIBlueprintHelperLibrary::SimpleMoveToLocation(GetPlayerController(), CurrentNarrativeTrigger->TargetArrow->GetComponentLocation());

	// Bind to the movement end to apply rotation
	if (UPathFollowingComponent* PFC = GetPlayerController()->FindComponentByClass<UPathFollowingComponent>())
		PFC->OnRequestFinished.AddUObject(this, &UML_NarrativeSubsystem::OnCinematicMoveFinished);

	// Activate the camera component so CalcCamera finds it (bAutoActivate is false by default)
	if (CurrentNarrativeTrigger && CurrentNarrativeTrigger->GetCinematicCamera())
	{
		CurrentNarrativeTrigger->GetCinematicCamera()->Activate();
		PreviousViewTarget = GetPlayerController()->GetViewTarget();
		GetPlayerController()->BlendToViewTarget(
			CurrentNarrativeTrigger,
			CurrentSequence->CameraBlendTime,
			VTBlend_Linear
		);
		
		// Start timer for camera blend completion
		if (CurrentSequence->bWaitForCameraBlendToFinish)
		{
			GetWorld()->GetTimerManager().SetTimer(
				CameraBlendTimerHandle,
				this,
				&UML_NarrativeSubsystem::OnCameraBlendCompleted,
				CurrentSequence->CameraBlendTime,
				false
			);
		}
	}
}

void UML_NarrativeSubsystem::OnCinematicMoveFinished(FAIRequestID, const FPathFollowingResult&)
{
	// Unbind immediatly to not react to the next movements
	if (UPathFollowingComponent* PFC = GetPlayerController()->FindComponentByClass<UPathFollowingComponent>())
		PFC->OnRequestFinished.RemoveAll(this);

	if (!CurrentNarrativeTrigger) return;

	APawn* Pawn = GetPlayerController()->GetPawn();
	if (!Pawn) return;

	const FRotator CurrentRot = Pawn->GetActorRotation();
	const float TargetYaw = CurrentNarrativeTrigger->TargetArrow->GetComponentRotation().Yaw;
	Pawn->SetActorRotation(FRotator(CurrentRot.Pitch, TargetYaw, CurrentRot.Roll));
	
	// Mark player movement as finished
	bPlayerMovementFinished = true;
	CheckCinematicSetupComplete();
}

void UML_NarrativeSubsystem::OnCameraBlendCompleted()
{
	bCameraBlendFinished = true;
	CheckCinematicSetupComplete();
}

void UML_NarrativeSubsystem::CheckCinematicSetupComplete()
{
	if (!bWaitingForCinematicSetup)
		return;

	if (bCameraBlendFinished && bPlayerMovementFinished)
	{
		bWaitingForCinematicSetup = false;
		PlayNextLine();
	}
}

void UML_NarrativeSubsystem::RestorePlayerControl() const
{
	// Unbind if the sequence ends before arriving
	if (UPathFollowingComponent* PFC = GetPlayerController()->FindComponentByClass<UPathFollowingComponent>())
		PFC->OnRequestFinished.RemoveAll(this);

	// Swap IMCs back: remove cinematic, restore main
	const UML_MycelandDeveloperSettings* DevSettings = UML_MycelandDeveloperSettings::GetMycelandDeveloperSettings();
	if (UEnhancedInputLocalPlayerSubsystem* InputSub = GetPlayerController()->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
	{
		if (UInputMappingContext* CinematicIMC = DevSettings->CinematicInputMappingContext.Mapping.LoadSynchronous())
			InputSub->RemoveMappingContext(CinematicIMC);

		if (UInputMappingContext* MainIMC = DevSettings->DefaultInputMappingContext.Mapping.LoadSynchronous())
			InputSub->AddMappingContext(MainIMC, DevSettings->DefaultInputMappingContext.Priority);
	}

	GetPlayerController()->BlendToViewTarget(
		PreviousViewTarget,
		CurrentSequence->CameraBlendTime,
		VTBlend_Linear
	);

	// Deactivate the cinematic camera AFTER THE BLEND so it doesn't interfere with future view targets 
	UCameraComponent* CamToDeactivate = CurrentNarrativeTrigger ? CurrentNarrativeTrigger->GetCinematicCamera() : nullptr;
	const float BlendTime = CurrentSequence->CameraBlendTime;

	if (CamToDeactivate)
	{
		FTimerHandle TempHandle;
		GetWorld()->GetTimerManager().SetTimer(
			TempHandle,
			[CamToDeactivate]()
			{
				if (IsValid(CamToDeactivate))
					CamToDeactivate->Deactivate();
			},
			BlendTime + 0.1f,
			false
		);
	}
}

void UML_NarrativeSubsystem::PlaySequence(UML_NarrativeSequence* Sequence, AML_NarrativeTrigger* Trigger)
{
	if (!Sequence || Sequence->DialogueLines.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("PlaySequence called with invalid sequence"));
		return;
	}

	// Clean up any existing sequence first
	if (CurrentSequence)
		CleanupCurrentSequence();
	
	CurrentSequence = Sequence;
	CurrentNarrativeTrigger = Trigger;
	CurrentLineIndex = 0;
	
	OnSequenceStart.Broadcast(CurrentSequence);
	
	if (CurrentSequence->bIsCinematicMode)
	{
		bWaitingForCinematicSetup = true;
		bCameraBlendFinished = !CurrentSequence->bWaitForCameraBlendToFinish; // if bWaitForCameraBlendToFinish is false, then tell bCameraBlendFinished that it is already considered finished
		bPlayerMovementFinished = !CurrentSequence->bWaitForPlayerMovementToFinish; // if bWaitForPlayerMovementToFinish is false, then tell bPlayerMovementFinished that it is already considered finished
		
		SetupCinematicMode();
		CheckCinematicSetupComplete();
	}
	else
	{
		PlayNextLine();
	}
}

IML_DialogueSpeaker* UML_NarrativeSubsystem::GetSpeaker(const ESpeakerTag Tag) const
{
	return CurrentNarrativeTrigger ? CurrentNarrativeTrigger->GetSpeaker(Tag) : nullptr;
}

void UML_NarrativeSubsystem::OnFMODEventStopped()
{
	// Validate that we still have a valid sequence and line index
	if (!CurrentSequence || CurrentLineIndex >= CurrentSequence->DialogueLines.Num())
	{
		UE_LOG(LogTemp, Warning, TEXT("OnFMODEventStopped called but sequence or line index is invalid"));
		return;
	}

	const FDialogueLine& Line = CurrentSequence->DialogueLines[CurrentLineIndex];
	
	// Unbind le callback
	if (const IML_DialogueSpeaker* Speaker = GetSpeaker(Line.SpeakerTag))
		if (UFMODAudioComponent* AudioComp = Speaker->GetAudioComponent())
			AudioComp->OnEventStopped.RemoveAll(this);
	
	// If PostDelay, then wait
	if (Line.PostDelay > 0.f)
	{
		GetWorld()->GetTimerManager().SetTimer(
			DialogueTimerHandle,
			this,
			&UML_NarrativeSubsystem::OnLineFinished,
			Line.PostDelay,
			false
		);
	}
	else
		OnLineFinished();
}

float UML_NarrativeSubsystem::GetSequenceProgress() const
{
	if (!CurrentSequence || CurrentSequence->DialogueLines.Num() == 0)
		return 0.0f;
	
	return static_cast<float>(CurrentLineIndex) / static_cast<float>(CurrentSequence->DialogueLines.Num());
}

void UML_NarrativeSubsystem::StopSequence()
{
	if (!CurrentSequence)
	{
		UE_LOG(LogTemp, Warning, TEXT("StopSequence called but no sequence is playing"));
		return;
	}

	OnSequenceEnd.Broadcast(CurrentSequence);
	CleanupCurrentSequence();
}
