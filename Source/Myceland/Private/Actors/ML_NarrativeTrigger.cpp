// Copyright Myceland Team, All Rights Reserved.


#include "Actors/ML_NarrativeTrigger.h"
#include "Components/BoxComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/ArrowComponent.h"
#include "Data Asset/ML_NarrativeSequence.h"
#include "Subsystem/ML_NarrativeSubsystem.h"
#include "Save System/ML_SaveSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"
#include "Player/ML_PlayerCharacter.h"

AML_NarrativeTrigger::AML_NarrativeTrigger()
{
    PrimaryActorTick.bCanEverTick = false;

    // Root component (SceneComponent)
    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);
    
    // Target scene component
    TargetArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("TargetArrow"));
    TargetArrow->SetupAttachment(Root);

    // Trigger box
    TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
    TriggerBox->SetupAttachment(Root);
    TriggerBox->SetBoxExtent(FVector(100.f, 100.f, 100.f));
    TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
    TriggerBox->SetGenerateOverlapEvents(true);

    // Cinematic camera
    CinematicCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("CinematicCamera"));
    CinematicCamera->SetupAttachment(Root);
    CinematicCamera->bAutoActivate = false;
}

void AML_NarrativeTrigger::BeginPlay()
{
    Super::BeginPlay();

    // Restore the played state from the save file so a play-once trigger that
    // already fired in a previous session does not play again after loading.
    if (const UML_SaveSubsystem* SaveSys = GetSaveSubsystem())
        bHasBeenPlayed = SaveSys->IsNarrativeTriggerPlayed(GetTriggerSaveID());

    // Bind overlap event
    if (TriggerBox)
        TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AML_NarrativeTrigger::OnTriggerBeginOverlap);

    // Bind to subsystem events
    if (UML_NarrativeSubsystem* SubSys = UML_NarrativeSubsystem::Get(this))
    {
        SubSys->OnSequenceStart.AddDynamic(this, &AML_NarrativeTrigger::HandleSequenceStart);
        SubSys->OnSequenceEnd.AddDynamic(this, &AML_NarrativeTrigger::HandleSequenceEnd);
    }
}

void AML_NarrativeTrigger::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UML_NarrativeSubsystem* SubSys = UML_NarrativeSubsystem::Get(this))
        SubSys->OnSequenceEnd.RemoveDynamic(this, &AML_NarrativeTrigger::HandleSequenceEnd);
    
    Super::EndPlay(EndPlayReason);
}

void AML_NarrativeTrigger::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    // Check if it's the player character
    const AML_PlayerCharacter* PlayerCharacter = Cast<AML_PlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
    if (OtherActor != PlayerCharacter) return;

    // Check if already played
    if (bPlayOnce && bHasBeenPlayed) return;

    // Check if we have a valid sequence
    if (!NarrativeSequence)
    {
        UE_LOG(LogTemp, Warning, TEXT("NarrativeTrigger '%s' has no NarrativeSequence assigned!"), *GetName());
        return;
    }

    // Play the sequence
    if (UML_NarrativeSubsystem* SubSys = UML_NarrativeSubsystem::Get(this))
    {
        bHasBeenPlayed = true;

        // Persist the played state so it survives a reload.
        if (UML_SaveSubsystem* SaveSys = GetSaveSubsystem())
            SaveSys->SetNarrativeTriggerPlayed(GetTriggerSaveID());

        OnSequenceStarted(NarrativeSequence);
        SubSys->PlaySequence(NarrativeSequence, this);
    }
}

void AML_NarrativeTrigger::HandleSequenceStart(UML_NarrativeSequence* Sequence)
{
    // Only handle if this is OUR sequence
    if (Sequence == NarrativeSequence)
        OnSequenceStarted(Sequence);
}

void AML_NarrativeTrigger::HandleSequenceEnd(UML_NarrativeSequence* Sequence)
{
    // Only handle if this is OUR sequence
    if (Sequence == NarrativeSequence)
        OnSequenceEnded(Sequence);
}

void AML_NarrativeTrigger::OnSequenceStarted_Implementation(UML_NarrativeSequence* Sequence)
{
}

void AML_NarrativeTrigger::OnSequenceEnded_Implementation(UML_NarrativeSequence* Sequence)
{
}

void AML_NarrativeTrigger::ResetTrigger()
{
    bHasBeenPlayed = false;

    // Clear the persisted flag so the trigger can play again after a reset.
    if (UML_SaveSubsystem* SaveSys = GetSaveSubsystem())
        SaveSys->ClearNarrativeTriggerPlayed(GetTriggerSaveID());
}

UML_SaveSubsystem* AML_NarrativeTrigger::GetSaveSubsystem() const
{
    const UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    return GI ? GI->GetSubsystem<UML_SaveSubsystem>() : nullptr;
}

FName AML_NarrativeTrigger::GetTriggerSaveID() const
{
    // Level-placed actors keep a stable FName across sessions, which makes it a
    // reliable per-trigger save key without any extra editor setup.
    return GetFName();
}

IML_DialogueSpeaker* AML_NarrativeTrigger::GetSpeaker(const ESpeakerTag Tag) const
{
    if (Tag == ESpeakerTag::Player)
    {
        AML_PlayerCharacter* PlayerCharacter = Cast<AML_PlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
        return Cast<IML_DialogueSpeaker>(PlayerCharacter);
    }
    
    if (AActor* const* Found = Speakers.Find(Tag))
        return Cast<IML_DialogueSpeaker>(*Found);
    
    return nullptr;
}
