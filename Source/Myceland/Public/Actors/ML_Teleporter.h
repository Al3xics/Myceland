// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ML_Teleporter.generated.h"

class UML_MycelandDeveloperSettings;
class UBoxComponent;
class UInputAction;
class UUserWidget;
struct FStreamableHandle;

UCLASS(Blueprintable)
class MYCELAND_API AML_Teleporter : public AActor
{
    GENERATED_BODY()

private:
    UPROPERTY()
    const UML_MycelandDeveloperSettings* DevSettings = nullptr;

    bool bPlayerInside = false;
    bool bTeleportActionBound = false;
    bool bTeleportInProgress = false;

    // Keeps the async-loaded destination asset alive until we travel.
    TSharedPtr<FStreamableHandle> DestinationLoadHandle;

    FTimerHandle OpenLevelTimerHandle;

    // Adds/removes the mapping context and enables/disables input on this actor.
    // The teleport input therefore only exists while the player overlaps the box.
    void SetTeleportInputEnabled(bool bEnabled);

    // Starts loading the destination level asynchronously.
    void BeginAsyncTeleport();

    // Called once the destination level asset has finished loading.
    void OnDestinationLevelLoaded();

    // Actually performs the level travel after the loading delay.
    void OpenDestinationLevel();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UFUNCTION()
    void OnTriggerBeginOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);

    UFUNCTION()
    void OnTriggerEndOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex);

    void OnTeleportTriggered();

public:
    AML_Teleporter();

    // ==================== COMPONENTS ====================

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UBoxComponent* TriggerBox;

    // ==================== DATA ====================

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "01 Teleporter",
        meta = (ToolTip = "Level opened when the player presses the teleport input while inside the trigger box."))
    TSoftObjectPtr<UWorld> DestinationLevel;

    // ==================== INPUT ====================

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "01 Teleporter|Input",
        meta = (ToolTip = "Action that opens the destination level. Only active while the player is inside the trigger box.\n\nMust be mapped in the Teleport Input Mapping Context set in the Myceland Developer Settings."))
    UInputAction* TeleportAction = nullptr;

    // ==================== LOADING ====================

    // Widget displayed immediately when teleportation starts.
    // Assign your loading screen Widget Blueprint here.
    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "01 Teleporter|Loading",
        meta = (ToolTip = "Widget displayed while the destination level is loading."))
    TSubclassOf<UUserWidget> LoadingWidgetClass;

    UPROPERTY(Transient)
    TObjectPtr<UUserWidget> LoadingWidgetInstance = nullptr;

    // Additional delay after the destination has finished loading before opening it.
    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "01 Teleporter|Loading",
        meta = (ClampMin = "0.0"))
    float DelayAfterLoad = 1.0f;
};