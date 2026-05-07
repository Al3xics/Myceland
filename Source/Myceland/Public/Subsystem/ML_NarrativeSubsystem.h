// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/ML_NarrativeData.h"
#include "Kismet/GameplayStatics.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ML_NarrativeSubsystem.generated.h"

class AML_TalkingThing;
class AML_NarrativeTrigger;
class UML_NarrativeSequence;
struct FAIRequestID;
struct FPathFollowingResult;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDialogueLineStart, const FDialogueLine&, Line, ESpeakerTag, Speaker);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDialogueLineEnd, const FDialogueLine&, Line, ESpeakerTag, Speaker);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSequenceStart, UML_NarrativeSequence*, Sequence);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSequenceEnd, UML_NarrativeSequence*, Sequence);

UCLASS()
class MYCELAND_API UML_NarrativeSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
private:
	UPROPERTY(Transient)
	TMap<ESpeakerTag, AML_TalkingThing*> RegisteredSpeakers;

	FTimerHandle DialogueTimerHandle;
	int32 CurrentLineIndex = 0;
	bool bCurrentLineStarted = false;

	UPROPERTY(Transient)
	UAudioComponent* ActiveAudioComponent = nullptr;

	UPROPERTY()
	UML_NarrativeSequence* CurrentSequence = nullptr;

	UPROPERTY()
	AML_NarrativeTrigger* CurrentNarrativeTrigger = nullptr;

	void PlayNextLine();
	void OnLineFinished();
	void SetupCinematicMode();
	void RestorePlayerControl();
	void OnCinematicMoveFinished(FAIRequestID RequestID, const FPathFollowingResult& Result);
	
public:
	// ==================== Events ====================
	
	UPROPERTY(BlueprintAssignable, Category="Narrative")
	FOnDialogueLineStart OnDialogueLineStart;
	
	UPROPERTY(BlueprintAssignable, Category="Narrative")
	FOnDialogueLineEnd OnDialogueLineEnd;
	
	UPROPERTY(BlueprintAssignable, Category="Narrative")
	FOnSequenceStart OnSequenceStart;
	
	UPROPERTY(BlueprintAssignable, Category="Narrative")
	FOnSequenceEnd OnSequenceEnd;
	
	
	
	// ==================== Main API ====================

	static UML_NarrativeSubsystem* Get(const UObject* WorldContextObject)
	{
		const UGameInstance* GI = UGameplayStatics::GetGameInstance(WorldContextObject);
		return GI ? GI->GetSubsystem<UML_NarrativeSubsystem>() : nullptr;
	}

	UFUNCTION(BlueprintCallable, Category="Narrative")
	void PlaySequence(UML_NarrativeSequence* Sequence, AML_NarrativeTrigger* Trigger);

	UFUNCTION(BlueprintCallable, Category="Narrative")
	bool SkipCurrentLine();

	UFUNCTION(BlueprintPure, Category="Narrative")
	bool IsSequencePlaying() const { return CurrentSequence != nullptr; }
	
	
	
	// ==================== Registry ====================
	
	void RegisterTalkingThing(AML_TalkingThing* Thing);
	void UnregisterTalkingThing(const AML_TalkingThing* Thing);
	AML_TalkingThing* GetSpeaker(ESpeakerTag Tag) const;
};
