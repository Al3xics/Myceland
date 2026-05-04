// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ML_NarrativeData.generated.h"

// ==================== ENUM ====================

UENUM(BlueprintType)
enum class ESpeakerTag : uint8
{
	None,
	Player,
	TalkingTree
};

// ==================== STRUCT ====================

USTRUCT(Blueprintable)
struct FDialogueLine
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText SubtitleText = FText();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	USoundBase* Sound = nullptr;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float PreDelay = 0.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float PostDelay = 0.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ESpeakerTag SpeakerTag = ESpeakerTag::None;
};
