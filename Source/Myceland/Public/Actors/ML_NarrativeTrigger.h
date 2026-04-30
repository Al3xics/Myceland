// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ArrowComponent.h"
#include "GameFramework/Actor.h"
#include "ML_NarrativeTrigger.generated.h"

class UML_NarrativeSequence;
class UBoxComponent;
class UCameraComponent;
class UNarrativeSubsystem;

UCLASS(Blueprintable)
class MYCELAND_API AML_NarrativeTrigger : public AActor
{
    GENERATED_BODY()

private:
    void BindToSubsystemEvents();
    void UnbindFromSubsystemEvents();

    UFUNCTION()
    void HandleSequenceStart(UML_NarrativeSequence* Sequence);

    UFUNCTION()
    void HandleSequenceEnd(UML_NarrativeSequence* Sequence);

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

    UPROPERTY(BlueprintReadOnly, Category = "Narrative")
    bool bHasBeenPlayed = false;

    
    
    // ==================== API ====================
    
    UFUNCTION(BlueprintCallable, Category = "Narrative")
    void ResetTrigger();

    UFUNCTION(BlueprintCallable, Category = "Narrative")
    UCameraComponent* GetCinematicCamera() const { return CinematicCamera; }
};