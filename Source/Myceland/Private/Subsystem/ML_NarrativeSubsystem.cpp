// Copyright Myceland Team, All Rights Reserved.


#include "Subsystem/ML_NarrativeSubsystem.h"

#include "Actors/ML_NarrativeTrigger.h"
#include "Actors/ML_TalkingThing.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Data Asset/ML_NarrativeSequence.h"
#include "Developer Settings/ML_MycelandDeveloperSettings.h"
#include "EnhancedInputSubsystems.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"
#include "Player/ML_PlayerController.h"

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
        return;
    }

    const FDialogueLine& Line = CurrentSequence->DialogueLines[CurrentLineIndex];

	auto StartLine = [this, Line]()
	{
		bCurrentLineStarted = true;

		if (AML_TalkingThing* Speaker = GetSpeaker(Line.SpeakerTag))
			Speaker->SetIsTalking(true);

		OnDialogueLineStart.Broadcast(Line, Line.SpeakerTag);

		ActiveAudioComponent = Line.Sound ? UGameplayStatics::SpawnSound2D(this, Line.Sound) : nullptr;

		const float LineDuration = Line.Sound ? Line.Sound->GetDuration() : 1.0f;
		GetWorld()->GetTimerManager().SetTimer(
			DialogueTimerHandle,
			this,
			&UML_NarrativeSubsystem::OnLineFinished,
			LineDuration + Line.PostDelay,
			false
		);
	};

    if (Line.PreDelay > 0.f)
    {
        GetWorld()->GetTimerManager().SetTimer(
            DialogueTimerHandle,
            [this, StartLine]() { StartLine(); },
            Line.PreDelay,
            false
        );
    }
    else
    {
    	StartLine();
    }
}

void UML_NarrativeSubsystem::OnLineFinished()
{
	if (!CurrentSequence) return;

	const FDialogueLine& Line = CurrentSequence->DialogueLines[CurrentLineIndex];

	ActiveAudioComponent = nullptr;

	if (AML_TalkingThing* Speaker = GetSpeaker(Line.SpeakerTag))
		Speaker->SetIsTalking(false);

	OnDialogueLineEnd.Broadcast(Line, Line.SpeakerTag);

	CurrentLineIndex++;
	PlayNextLine();
}

bool UML_NarrativeSubsystem::SkipCurrentLine()
{
	if (!CurrentSequence) return false;
	if (!CurrentSequence->bAllowSkip) return false;

	GetWorld()->GetTimerManager().ClearTimer(DialogueTimerHandle);

	if (bCurrentLineStarted && CurrentLineIndex < CurrentSequence->DialogueLines.Num())
	{
		const FDialogueLine& Line = CurrentSequence->DialogueLines[CurrentLineIndex];

		if (ActiveAudioComponent && ActiveAudioComponent->IsPlaying())
			ActiveAudioComponent->Stop();
		ActiveAudioComponent = nullptr;

		if (AML_TalkingThing* Speaker = GetSpeaker(Line.SpeakerTag))
			Speaker->SetIsTalking(false);

		OnDialogueLineEnd.Broadcast(Line, Line.SpeakerTag);
	}

	CurrentLineIndex++;
	PlayNextLine();
	return true;
}

void UML_NarrativeSubsystem::SetupCinematicMode()
{
	AML_PlayerController* PC = Cast<AML_PlayerController>(UGameplayStatics::GetPlayerController(this, 0));
	if (!PC) return;

	// Swap IMCs: remove main (no movement/click) and add cinematic-only (skip only)
	const UML_MycelandDeveloperSettings* DevSettings = UML_MycelandDeveloperSettings::GetMycelandDeveloperSettings();
	if (UEnhancedInputLocalPlayerSubsystem* InputSub = PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
	{
		if (UInputMappingContext* MainIMC = DevSettings->DefaultInputMappingContext.Mapping.LoadSynchronous())
			InputSub->RemoveMappingContext(MainIMC);

		if (UInputMappingContext* CinematicIMC = DevSettings->CinematicInputMappingContext.Mapping.LoadSynchronous())
			InputSub->AddMappingContext(CinematicIMC, DevSettings->CinematicInputMappingContext.Priority);
	}

	UAIBlueprintHelperLibrary::SimpleMoveToLocation(PC, CurrentNarrativeTrigger->TargetArrow->GetComponentLocation());

	// Bind to the movement end to apply rotation
	if (UPathFollowingComponent* PFC = PC->FindComponentByClass<UPathFollowingComponent>())
		PFC->OnRequestFinished.AddUObject(this, &UML_NarrativeSubsystem::OnCinematicMoveFinished);

	// Activate the camera component so CalcCamera finds it (bAutoActivate is false by default)
	if (CurrentNarrativeTrigger && CurrentNarrativeTrigger->GetCinematicCamera())
	{
		CurrentNarrativeTrigger->GetCinematicCamera()->Activate();
		PC->SetViewTargetWithBlend(
			CurrentNarrativeTrigger,
			CurrentSequence->CameraBlendTime,
			VTBlend_Linear
		);
	}
}

void UML_NarrativeSubsystem::OnCinematicMoveFinished(FAIRequestID, const FPathFollowingResult&)
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC) return;

	// Unbind immediatly to not react to the next movements
	if (UPathFollowingComponent* PFC = PC->FindComponentByClass<UPathFollowingComponent>())
		PFC->OnRequestFinished.RemoveAll(this);

	if (!CurrentNarrativeTrigger) return;

	APawn* Pawn = PC->GetPawn();
	if (!Pawn) return;

	const FRotator CurrentRot = Pawn->GetActorRotation();
	const float TargetYaw = CurrentNarrativeTrigger->TargetArrow->GetComponentRotation().Yaw;
	Pawn->SetActorRotation(FRotator(CurrentRot.Pitch, TargetYaw, CurrentRot.Roll));
}

void UML_NarrativeSubsystem::RestorePlayerControl()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC) return;

	// Unbind if the sequence ends before arriving
	if (UPathFollowingComponent* PFC = PC->FindComponentByClass<UPathFollowingComponent>())
		PFC->OnRequestFinished.RemoveAll(this);

	// Deactivate the cinematic camera so it doesn't interfere with future view targets
	if (CurrentNarrativeTrigger && CurrentNarrativeTrigger->GetCinematicCamera())
		CurrentNarrativeTrigger->GetCinematicCamera()->Deactivate();

	// Swap IMCs back: remove cinematic, restore main
	const UML_MycelandDeveloperSettings* DevSettings = UML_MycelandDeveloperSettings::GetMycelandDeveloperSettings();
	if (UEnhancedInputLocalPlayerSubsystem* InputSub = PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
	{
		if (UInputMappingContext* CinematicIMC = DevSettings->CinematicInputMappingContext.Mapping.LoadSynchronous())
			InputSub->RemoveMappingContext(CinematicIMC);

		if (UInputMappingContext* MainIMC = DevSettings->DefaultInputMappingContext.Mapping.LoadSynchronous())
			InputSub->AddMappingContext(MainIMC, DevSettings->DefaultInputMappingContext.Priority);
	}

	PC->SetViewTargetWithBlend(
		PC->GetPawn(),
		CurrentSequence->CameraBlendTime,
		VTBlend_Linear
	);
}

void UML_NarrativeSubsystem::PlaySequence(UML_NarrativeSequence* Sequence, AML_NarrativeTrigger* Trigger)
{
	if (!Sequence || Sequence->DialogueLines.Num() == 0) return;
	
	CurrentSequence = Sequence;
	CurrentNarrativeTrigger = Trigger;
	CurrentLineIndex = 0;
	
	if (CurrentSequence->bIsCinematicMode)
		SetupCinematicMode();
	
	OnSequenceStart.Broadcast(CurrentSequence);
	PlayNextLine();
}

void UML_NarrativeSubsystem::RegisterTalkingThing(AML_TalkingThing* Thing)
{
	if (!Thing) return;
	RegisteredSpeakers.Add(Thing->GetSpeakerTag(), Thing);
}

void UML_NarrativeSubsystem::UnregisterTalkingThing(const AML_TalkingThing* Thing)
{
	if (!Thing) return;
	RegisteredSpeakers.Remove(Thing->GetSpeakerTag());
}

AML_TalkingThing* UML_NarrativeSubsystem::GetSpeaker(const ESpeakerTag Tag) const
{
	if (AML_TalkingThing* const* Found = RegisteredSpeakers.Find(Tag))
		return *Found;
	
	return nullptr;
}
