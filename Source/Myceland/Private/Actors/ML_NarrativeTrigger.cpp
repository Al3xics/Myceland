// Copyright Myceland Team, All Rights Reserved.


#include "Actors/ML_NarrativeTrigger.h"
#include "Components/BoxComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/ArrowComponent.h"
#include "Data Asset/ML_NarrativeSequence.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Subsystem/ML_NarrativeSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Player/ML_PlayerCharacter.h"

AML_NarrativeTrigger::AML_NarrativeTrigger()
{
    // Tick drives the cinematic approach at runtime and the debug path drawing in the editor
    // (see ShouldTickIfViewportsOnly). Disabled at BeginPlay when not needed.
    PrimaryActorTick.bCanEverTick = true;

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

    // Only tick when a cinematic approach is running (or to keep drawing debug paths in PIE)
    SetActorTickEnabled(bDrawDebugPaths);

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
    StopCinematicApproach();

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
    if (!canPlay) return;
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
        OnSequenceStarted(NarrativeSequence);
        SubSys->PlaySequence(NarrativeSequence, this);
    }
}

void AML_NarrativeTrigger::PlaySequence()
{
    // Play the sequence
    if (UML_NarrativeSubsystem* SubSys = UML_NarrativeSubsystem::Get(this))
    {
        bHasBeenPlayed = true;
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
}

void AML_NarrativeTrigger::Tick(const float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (ApproachPhase != EApproachPhase::None)
        TickApproach(DeltaTime);

    if (bDrawDebugPaths)
        DrawDebugApproachPaths();
}

void AML_NarrativeTrigger::StartCinematicApproach()
{
    ACharacter* Character = Cast<ACharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
    if (!Character || !TargetArrow)
    {
        // Nothing to move: report as finished so the sequence isn't stuck waiting for us
        if (UML_NarrativeSubsystem* SubSys = UML_NarrativeSubsystem::Get(this))
            SubSys->NotifyCinematicApproachFinished();
        return;
    }

    ApproachCharacter = Character;
    ApproachElapsedTime = 0.f;
    ApproachPhase = EApproachPhase::WalkToTarget;

    // We drive the yaw manually during the approach; OrientRotationToMovement would fight it
    if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
    {
        bSavedOrientRotationToMovement = MoveComp->bOrientRotationToMovement;
        MoveComp->bOrientRotationToMovement = false;
    }

    SetActorTickEnabled(true);
}

void AML_NarrativeTrigger::StopCinematicApproach()
{
    if (ApproachPhase == EApproachPhase::None) return;
    ApproachPhase = EApproachPhase::None;

    if (ApproachCharacter)
        if (UCharacterMovementComponent* MoveComp = ApproachCharacter->GetCharacterMovement())
            MoveComp->bOrientRotationToMovement = bSavedOrientRotationToMovement;

    ApproachCharacter = nullptr;
    SetActorTickEnabled(bDrawDebugPaths);
}

void AML_NarrativeTrigger::FinishApproach()
{
    StopCinematicApproach();

    if (UML_NarrativeSubsystem* SubSys = UML_NarrativeSubsystem::Get(this))
        SubSys->NotifyCinematicApproachFinished();
}

float AML_NarrativeTrigger::StepSteeringYaw(const float CurrentYaw, const FVector& From, const FVector& Target, const float TurnSpeedDeg, const float MoveSpeed, const float DeltaTime)
{
    FVector ToTarget = Target - From;
    ToTarget.Z = 0.f;
    const float Distance = FMath::Max(ToTarget.Size(), 1.f);

    // Minimum turn speed so the turn radius (MoveSpeed / omega) stays <= half the remaining distance
    const float MinTurnSpeed = FMath::RadiansToDegrees(2.f * MoveSpeed / Distance);
    const float EffectiveTurnSpeed = FMath::Max(TurnSpeedDeg, MinTurnSpeed);

    return FMath::FixedTurn(CurrentYaw, ToTarget.Rotation().Yaw, EffectiveTurnSpeed * DeltaTime);
}

void AML_NarrativeTrigger::TickApproach(const float DeltaTime)
{
    if (!ApproachCharacter || !TargetArrow)
    {
        FinishApproach();
        return;
    }

    // Failsafe: blocked on the way (wall, prop...) -> skip to the alignment so the sequence can start
    ApproachElapsedTime += DeltaTime;
    if (ApproachPhase == EApproachPhase::WalkToTarget && ApproachElapsedTime >= ApproachTimeout)
        ApproachPhase = EApproachPhase::AlignToArrow;

    const FRotator CurrentRot = ApproachCharacter->GetActorRotation();

    if (ApproachPhase == EApproachPhase::WalkToTarget)
    {
        const FVector TargetLocation = TargetArrow->GetComponentLocation();

        if (FVector::Dist2D(ApproachCharacter->GetActorLocation(), TargetLocation) <= ApproachAcceptanceRadius)
        {
            ApproachPhase = EApproachPhase::AlignToArrow;
        }
        else
        {
            const UCharacterMovementComponent* MoveComp = ApproachCharacter->GetCharacterMovement();
            const float MoveSpeed = MoveComp ? MoveComp->MaxWalkSpeed : 400.f;

            const float NewYaw = StepSteeringYaw(CurrentRot.Yaw, ApproachCharacter->GetActorLocation(), TargetLocation, ApproachTurnSpeed, MoveSpeed, DeltaTime);
            ApproachCharacter->SetActorRotation(FRotator(CurrentRot.Pitch, NewYaw, CurrentRot.Roll));
            ApproachCharacter->AddMovementInput(ApproachCharacter->GetActorForwardVector());
            return;
        }
    }

    if (ApproachPhase == EApproachPhase::AlignToArrow)
    {
        const float TargetYaw = TargetArrow->GetComponentRotation().Yaw;
        const float NewYaw = FMath::FixedTurn(CurrentRot.Yaw, TargetYaw, ApproachTurnSpeed * DeltaTime);
        ApproachCharacter->SetActorRotation(FRotator(CurrentRot.Pitch, NewYaw, CurrentRot.Roll));

        if (FMath::Abs(FMath::FindDeltaAngleDegrees(NewYaw, TargetYaw)) <= 0.5f)
            FinishApproach();
    }
}

void AML_NarrativeTrigger::DrawDebugApproachPaths() const
{
    if (!TriggerBox || !TargetArrow || !GetWorld()) return;

    const FVector TargetLocation = TargetArrow->GetComponentLocation();
    const FTransform BoxTransform = TriggerBox->GetComponentTransform();
    const FVector Extent = TriggerBox->GetUnscaledBoxExtent();

    // Entry points sampled on the 4 vertical sides of the box, heading inward
    struct FSide { FVector Normal; FVector Tangent; float NormalExtent; float TangentExtent; };
    const FSide Sides[4] = {
        { FVector( 1.f, 0.f, 0.f), FVector(0.f, 1.f, 0.f), Extent.X, Extent.Y },
        { FVector(-1.f, 0.f, 0.f), FVector(0.f, 1.f, 0.f), Extent.X, Extent.Y },
        { FVector(0.f,  1.f, 0.f), FVector(1.f, 0.f, 0.f), Extent.Y, Extent.X },
        { FVector(0.f, -1.f, 0.f), FVector(1.f, 0.f, 0.f), Extent.Y, Extent.X },
    };

    constexpr float SimStep = 1.f / 30.f;
    constexpr int32 MaxSimSteps = 600; // 20 seconds of simulated walk, guards against endless loops

    for (const FSide& Side : Sides)
    {
        for (int32 i = 0; i < DebugPathsPerSide; ++i)
        {
            const float Alpha = (DebugPathsPerSide == 1) ? 0.f : FMath::Lerp(-0.8f, 0.8f, static_cast<float>(i) / static_cast<float>(DebugPathsPerSide - 1));
            const FVector LocalEntry = Side.Normal * Side.NormalExtent + Side.Tangent * (Side.TangentExtent * Alpha);
            FVector Position = BoxTransform.TransformPosition(LocalEntry);
            Position.Z = TargetLocation.Z;

            const FVector WorldInward = -BoxTransform.TransformVectorNoScale(Side.Normal);
            float Yaw = WorldInward.Rotation().Yaw;

            for (int32 Step = 0; Step < MaxSimSteps; ++Step)
            {
                if (FVector::Dist2D(Position, TargetLocation) <= ApproachAcceptanceRadius) break;

                Yaw = StepSteeringYaw(Yaw, Position, TargetLocation, ApproachTurnSpeed, DebugWalkSpeed, SimStep);
                const FVector NewPosition = Position + FRotator(0.f, Yaw, 0.f).Vector() * (DebugWalkSpeed * SimStep);
                DrawDebugLine(GetWorld(), Position, NewPosition, FColor::Green, false, -1.f, 0, 2.f);
                Position = NewPosition;
            }
        }
    }

    // Acceptance radius around the arrow + final facing direction
    DrawDebugCircle(GetWorld(), TargetLocation, ApproachAcceptanceRadius, 24, FColor::Yellow, false, -1.f, 0, 2.f, FVector(1.f, 0.f, 0.f), FVector(0.f, 1.f, 0.f), false);
    DrawDebugDirectionalArrow(GetWorld(), TargetLocation, TargetLocation + TargetArrow->GetForwardVector() * 100.f, 30.f, FColor::Cyan, false, -1.f, 0, 3.f);
}

IML_DialogueSpeaker* AML_NarrativeTrigger::GetSpeaker(const ESpeakerTag Tag) const
{
    if (Tag != ESpeakerTag::Player)
    {
        if (AActor* const* Found = Speakers.Find(Tag))
            if (IML_DialogueSpeaker* Speaker = Cast<IML_DialogueSpeaker>(*Found))
                return Speaker;
    }

    // Player tag, or fallback: a tag with no bound actor plays as a voice-over on the player
    AML_PlayerCharacter* PlayerCharacter = Cast<AML_PlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
    return Cast<IML_DialogueSpeaker>(PlayerCharacter);
}
