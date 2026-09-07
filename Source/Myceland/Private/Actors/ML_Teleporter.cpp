// Copyright Myceland Team, All Rights Reserved.

#include "Actors/ML_Teleporter.h"

#include "Blueprint/UserWidget.h"
#include "Components/BoxComponent.h"
#include "Developer Settings/ML_MycelandDeveloperSettings.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Kismet/GameplayStatics.h"
#include "Player/ML_PlayerCharacter.h"
#include "TimerManager.h"


AML_Teleporter::AML_Teleporter()
{
    PrimaryActorTick.bCanEverTick = false;

    // Root component
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
        TriggerBox->OnComponentBeginOverlap.AddDynamic(
            this,
            &AML_Teleporter::OnTriggerBeginOverlap);

        TriggerBox->OnComponentEndOverlap.AddDynamic(
            this,
            &AML_Teleporter::OnTriggerEndOverlap);
    }
}


void AML_Teleporter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Make sure the mapping context doesn't leak.
    // The LocalPlayer survives level changes.
    if (bPlayerInside)
    {
        SetTeleportInputEnabled(false);
    }

    // Clear delayed level-opening timer.
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(OpenLevelTimerHandle);
    }

    // Release/cancel any pending async load if this actor disappears
    // before travelling.
    if (DestinationLoadHandle.IsValid())
    {
        if (!DestinationLoadHandle->HasLoadCompleted())
        {
            DestinationLoadHandle->CancelHandle();
        }

        DestinationLoadHandle.Reset();
    }

    Super::EndPlay(EndPlayReason);
}


void AML_Teleporter::OnTriggerBeginOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    const AML_PlayerCharacter* PlayerCharacter =
        Cast<AML_PlayerCharacter>(
            UGameplayStatics::GetPlayerCharacter(this, 0));

    if (OtherActor != PlayerCharacter || bPlayerInside)
        return;

    SetTeleportInputEnabled(true);
}


void AML_Teleporter::OnTriggerEndOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex)
{
    const AML_PlayerCharacter* PlayerCharacter =
        Cast<AML_PlayerCharacter>(
            UGameplayStatics::GetPlayerCharacter(this, 0));

    if (OtherActor != PlayerCharacter || !bPlayerInside)
        return;

    // Don't interfere once teleportation has already started.
    if (bTeleportInProgress)
        return;

    SetTeleportInputEnabled(false);
}


void AML_Teleporter::SetTeleportInputEnabled(bool bEnabled)
{
    bPlayerInside = bEnabled;

    APlayerController* PC =
        UGameplayStatics::GetPlayerController(this, 0);

    if (!PC || !PC->GetLocalPlayer())
        return;

    // The mapping context only exists while the player is inside the box.
    if (UEnhancedInputLocalPlayerSubsystem* InputSub =
        PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
    {
        int32 Priority = 0;

        if (UInputMappingContext* TeleportIMC =
            DevSettings->GetInputMappingContext(
                EInputMappingType::Teleport,
                Priority))
        {
            if (bEnabled)
            {
                InputSub->AddMappingContext(
                    TeleportIMC,
                    Priority);
            }
            else
            {
                InputSub->RemoveMappingContext(
                    TeleportIMC);
            }
        }
    }

    if (bEnabled)
    {
        // Bind on this actor's own input component.
        EnableInput(PC);

        if (!bTeleportActionBound && TeleportAction)
        {
            if (UEnhancedInputComponent* EIC =
                Cast<UEnhancedInputComponent>(InputComponent))
            {
                EIC->BindAction(
                    TeleportAction,
                    ETriggerEvent::Started,
                    this,
                    &AML_Teleporter::OnTeleportTriggered);

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
    if (!bPlayerInside)
        return;

    if (bTeleportInProgress)
        return;

    if (DestinationLevel.IsNull())
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("ML_Teleporter '%s': no DestinationLevel set."),
            *GetName());

        return;
    }

    BeginAsyncTeleport();
}


void AML_Teleporter::BeginAsyncTeleport()
{
    if (bTeleportInProgress)
        return;

    bTeleportInProgress = true;

    // Remove teleport input immediately so it cannot be triggered twice.
    SetTeleportInputEnabled(false);

    // ============================================================
    // SHOW LOADING WIDGET
    // ============================================================

    if (LoadingWidgetClass)
    {
        if (APlayerController* PC =
            UGameplayStatics::GetPlayerController(this, 0))
        {
            LoadingWidgetInstance =
                CreateWidget<UUserWidget>(
                    PC,
                    LoadingWidgetClass);

            if (LoadingWidgetInstance)
            {
                LoadingWidgetInstance->AddToViewport(9999);
            }
        }
    }

    // ============================================================
    // ASYNC LOAD DESTINATION LEVEL
    // ============================================================

    const FSoftObjectPath DestinationPath =
        DestinationLevel.ToSoftObjectPath();

    if (!DestinationPath.IsValid())
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("ML_Teleporter '%s': DestinationLevel path is invalid."),
            *GetName());

        bTeleportInProgress = false;

        if (LoadingWidgetInstance)
        {
            LoadingWidgetInstance->RemoveFromParent();
            LoadingWidgetInstance = nullptr;
        }

        return;
    }

    FStreamableManager& StreamableManager =
        UAssetManager::GetStreamableManager();

    DestinationLoadHandle =
        StreamableManager.RequestAsyncLoad(
            DestinationPath,
            FStreamableDelegate::CreateUObject(
                this,
                &AML_Teleporter::OnDestinationLevelLoaded),
            FStreamableManager::AsyncLoadHighPriority);

    if (!DestinationLoadHandle.IsValid())
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("ML_Teleporter '%s': failed to create async load handle."),
            *GetName());

        bTeleportInProgress = false;

        if (LoadingWidgetInstance)
        {
            LoadingWidgetInstance->RemoveFromParent();
            LoadingWidgetInstance = nullptr;
        }
    }
}


void AML_Teleporter::OnDestinationLevelLoaded()
{
    if (!GetWorld())
        return;

    UE_LOG(
        LogTemp,
        Log,
        TEXT("ML_Teleporter '%s': destination loaded. Opening in %.2f seconds."),
        *GetName(),
        DelayAfterLoad);

    // Wait AFTER loading completes.
    if (DelayAfterLoad <= 0.0f)
    {
        OpenDestinationLevel();
        return;
    }

    GetWorld()->GetTimerManager().SetTimer(
        OpenLevelTimerHandle,
        this,
        &AML_Teleporter::OpenDestinationLevel,
        DelayAfterLoad,
        false);
}


void AML_Teleporter::OpenDestinationLevel()
{
    if (DestinationLevel.IsNull())
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("ML_Teleporter '%s': DestinationLevel became invalid before travel."),
            *GetName());

        return;
    }

    // The map asset has already been asynchronously loaded.
    // Now perform the actual world travel.
    UGameplayStatics::OpenLevelBySoftObjectPtr(
        this,
        DestinationLevel);
}