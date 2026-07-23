// Copyright Myceland Team, All Rights Reserved.


#include "Actors/ML_Teleporter.h"

#include "Components/BoxComponent.h"
#include "Developer Settings/ML_MycelandDeveloperSettings.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Kismet/GameplayStatics.h"
#include "Player/ML_PlayerCharacter.h"

AML_Teleporter::AML_Teleporter()
{
    PrimaryActorTick.bCanEverTick = false;

    // Root component (SceneComponent)
    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    // Trigger box
    TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
    TriggerBox->SetupAttachment(Root);
    TriggerBox->SetBoxExtent(FVector(150.f, 150.f, 150.f));
    TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
    TriggerBox->SetGenerateOverlapEvents(true);
}

void AML_Teleporter::BeginPlay()
{
    Super::BeginPlay();
    DevSettings = UML_MycelandDeveloperSettings::GetMycelandDeveloperSettings();

    if (TriggerBox)
    {
        TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AML_Teleporter::OnTriggerBeginOverlap);
        TriggerBox->OnComponentEndOverlap.AddDynamic(this, &AML_Teleporter::OnTriggerEndOverlap);
    }
}

void AML_Teleporter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Make sure the mapping context doesn't leak: the LocalPlayer (and its Enhanced Input
    // subsystem) survives level changes, so a context left behind would stay active forever.
    if (bPlayerInside)
        SetTeleportInputEnabled(false);

    Super::EndPlay(EndPlayReason);
}

void AML_Teleporter::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    // Check if it's the player character
    const AML_PlayerCharacter* PlayerCharacter = Cast<AML_PlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
    if (OtherActor != PlayerCharacter || bPlayerInside) return;

    SetTeleportInputEnabled(true);
}

void AML_Teleporter::OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    const AML_PlayerCharacter* PlayerCharacter = Cast<AML_PlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
    if (OtherActor != PlayerCharacter || !bPlayerInside) return;

    SetTeleportInputEnabled(false);
}

void AML_Teleporter::SetTeleportInputEnabled(bool bEnabled)
{
    bPlayerInside = bEnabled;

    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (!PC || !PC->GetLocalPlayer()) return;

    // The mapping context only exists while the player is inside the box, so the
    // teleport keys do nothing anywhere else in the level.
    if (UEnhancedInputLocalPlayerSubsystem* InputSub = PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
    {
        int32 Priority = 0;
        if (UInputMappingContext* TeleportIMC = DevSettings->GetInputMappingContext(EInputMappingType::Teleport, Priority))
        {
            if (bEnabled)
                InputSub->AddMappingContext(TeleportIMC, Priority);
            else
                InputSub->RemoveMappingContext(TeleportIMC);
        }
    }

    if (bEnabled)
    {
        // Bind the action on this actor's own input component instead of the player's,
        // so the player classes don't need to know about teleporters.
        EnableInput(PC);

        if (!bTeleportActionBound && TeleportAction)
        {
            if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
            {
                EIC->BindAction(TeleportAction, ETriggerEvent::Started, this, &AML_Teleporter::OnTeleportTriggered);
                bTeleportActionBound = true;
            }
        }
    }
    else
    {
        DisableInput(PC);
    }
}

void AML_Teleporter::OnTeleportTriggered()
{
    if (!bPlayerInside) return;

    if (DestinationLevel.IsNull())
    {
        UE_LOG(LogTemp, Warning, TEXT("ML_Teleporter '%s': no DestinationLevel set."), *GetName());
        return;
    }

    // Remove the mapping context before travelling: the LocalPlayer survives OpenLevel,
    // so it would otherwise carry the teleport context into the destination level.
    SetTeleportInputEnabled(false);

    UGameplayStatics::OpenLevelBySoftObjectPtr(this, DestinationLevel);
}
