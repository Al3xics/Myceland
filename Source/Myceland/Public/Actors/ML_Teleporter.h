// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ML_Teleporter.generated.h"

class UML_MycelandDeveloperSettings;
class UBoxComponent;
class UInputAction;

UCLASS(Blueprintable)
class MYCELAND_API AML_Teleporter : public AActor
{
    GENERATED_BODY()

private:
    UPROPERTY()
    const UML_MycelandDeveloperSettings* DevSettings = nullptr;
    bool bPlayerInside = false;
    bool bTeleportActionBound = false;
    
    // Adds/removes the mapping context and enables/disables input on this actor.
    // The teleport input therefore only exists while the player overlaps the box.
    void SetTeleportInputEnabled(bool bEnabled);

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UFUNCTION()
    void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

    void OnTeleportTriggered();

public:
    AML_Teleporter();

    // ==================== COMPONENTS ====================

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UBoxComponent* TriggerBox;

    // ==================== DATA ====================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "01 Teleporter", meta = (ToolTip = "Level opened when the player presses the teleport input while inside the trigger box."))
    TSoftObjectPtr<UWorld> DestinationLevel;

    // The teleport mapping context comes from the Myceland Developer Settings
    // (TeleportInputMappingContext). It is added while the player is inside the box,
    // removed when they leave.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "01 Teleporter|Input", meta = (ToolTip = "Action that opens the destination level. Only active while the player is inside the trigger box.\n\nMust be mapped in the Teleport Input Mapping Context set in the Myceland Developer Settings."))
    UInputAction* TeleportAction = nullptr;
};
