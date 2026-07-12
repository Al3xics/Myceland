// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ArrowComponent.h"
#include "Core/ML_NarrativeData.h"
#include "GameFramework/Actor.h"
#include "Interfaces/ML_DialogueSpeaker.h"
#include "ML_NarrativeTrigger.generated.h"

class ACharacter;
class AML_TalkingThing;
class UML_NarrativeSequence;
class UBoxComponent;
class UCameraComponent;
class UNarrativeSubsystem;

UCLASS(Blueprintable)
class MYCELAND_API AML_NarrativeTrigger : public AActor
{
    GENERATED_BODY()

private:
    UFUNCTION()
    void HandleSequenceStart(UML_NarrativeSequence* Sequence);

    UFUNCTION()
    void HandleSequenceEnd(UML_NarrativeSequence* Sequence);

    // ==================== CINEMATIC APPROACH (runtime state) ====================

    enum class EApproachPhase : uint8 { None, WalkToTarget, AlignToArrow };
    EApproachPhase ApproachPhase = EApproachPhase::None;

    UPROPERTY()
    ACharacter* ApproachCharacter = nullptr;

    bool bSavedOrientRotationToMovement = false;
    float ApproachElapsedTime = 0.f;

    void TickApproach(float DeltaTime);
    void FinishApproach();
    void DrawDebugApproachPaths() const;

    // One steering step: turns CurrentYaw towards Target at TurnSpeedDeg. The turn speed is
    // boosted when close so the turn radius stays smaller than the remaining distance,
    // otherwise the character orbits around the target forever. Shared by the runtime
    // approach and the editor debug simulation so the drawn arcs match the real movement.
    static float StepSteeringYaw(float CurrentYaw, const FVector& From, const FVector& Target, float TurnSpeedDeg, float MoveSpeed, float DeltaTime);

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    
    UFUNCTION()
    void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION(BlueprintNativeEvent, Category = "Narrative")
    void OnSequenceStarted(UML_NarrativeSequence* Sequence);
    virtual void OnSequenceStarted_Implementation(UML_NarrativeSequence* Sequence);

    UFUNCTION(BlueprintNativeEvent, Category = "Narrative")
    void OnSequenceEnded(UML_NarrativeSequence* Sequence);
    virtual void OnSequenceEnded_Implementation(UML_NarrativeSequence* Sequence);

public:
    AML_NarrativeTrigger();
    
    // ==================== COMPONENTS ====================

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UArrowComponent* TargetArrow;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UBoxComponent* TriggerBox;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UCameraComponent* CinematicCamera;

    
    
    // ==================== DATA ====================
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative")
    UML_NarrativeSequence* NarrativeSequence;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative")
    bool bPlayOnce = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative", meta = (ToolTip="All speakers related to this narrative trigger.\n\n⚠️ DON'T ADD THE PLAYER TAG! It's handled automatically.\n\nOnly add actors that implement IML_DialogueSpeaker interface (e.g., TalkingThing actors).\n\nA line whose tag has no actor here is played on the player (voice-over). Use the line's 'Spoken Out Loud' flag to control the talking animation."))
    TMap<ESpeakerTag, AActor*> Speakers;

    UPROPERTY(BlueprintReadOnly, Category = "Narrative")
    bool bHasBeenPlayed = false;



    // ==================== CINEMATIC APPROACH ====================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Cinematic Approach", meta = (ClampMin = "10.0", ClampMax = "720.0", ToolTip = "How fast the player turns (deg/s) while walking to the arrow.\n\nLower values = wider arc. Automatically boosted near the target to avoid orbiting around it.\n\nAlso used for the final alignment with the arrow direction once arrived."))
    float ApproachTurnSpeed = 120.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Cinematic Approach", meta = (ClampMin = "10.0", ToolTip = "Distance to the arrow (cm) at which the walk stops and the final alignment starts."))
    float ApproachAcceptanceRadius = 50.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Cinematic Approach", meta = (ClampMin = "1.0", ToolTip = "Failsafe: if the player still hasn't reached the arrow after this many seconds (e.g., blocked by an obstacle), the approach is forced to finish so the sequence can start."))
    float ApproachTimeout = 8.f;

    UPROPERTY(EditAnywhere, Category = "Narrative|Cinematic Approach|Debug", meta = (ToolTip = "Draw the simulated approach arcs in the editor viewport (enable 'Realtime' in the viewport to see them).\n\nPaths start from the sides of the trigger box, heading inward. The real path also depends on where/how the player actually enters, so this is an approximation."))
    bool bDrawDebugPaths = false;

    UPROPERTY(EditAnywhere, Category = "Narrative|Cinematic Approach|Debug", meta = (EditCondition = "bDrawDebugPaths", ClampMin = "50.0", ToolTip = "Walk speed (cm/s) used by the debug simulation. Set it to the player's MaxWalkSpeed for accurate arcs."))
    float DebugWalkSpeed = 400.f;

    UPROPERTY(EditAnywhere, Category = "Narrative|Cinematic Approach|Debug", meta = (EditCondition = "bDrawDebugPaths", ClampMin = "1", ClampMax = "10", ToolTip = "Number of simulated entry points per side of the trigger box."))
    int32 DebugPathsPerSide = 3;



    // ==================== API ====================
    
    UFUNCTION(BlueprintCallable, Category = "Narrative")
    void ResetTrigger();

    UFUNCTION(BlueprintCallable, Category = "Narrative")
    UCameraComponent* GetCinematicCamera() const { return CinematicCamera; }

    IML_DialogueSpeaker* GetSpeaker(ESpeakerTag Tag) const;

    // Walks the player to the TargetArrow with a steering arc (no navmesh), then aligns
    // them with the arrow direction. Notifies the NarrativeSubsystem when done.
    void StartCinematicApproach();

    // Interrupts the approach (sequence skipped/stopped) and restores the character movement settings.
    void StopCinematicApproach();

    virtual void Tick(float DeltaTime) override;
    virtual bool ShouldTickIfViewportsOnly() const override { return true; }
};