// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FMODEvent.h"
#include "ML_NarrativeData.generated.h"

// ==================== ENUM ====================

UENUM(BlueprintType)
enum class ESpeakerTag : uint8
{
	None,
	Player,
	Thing0,
	Thing1,
	Thing2,
	Thing3,
	Thing4,
	Thing5,
	Thing6,
	Thing7,
	Thing8,
	Thing9
};

// ==================== STRUCT ====================

USTRUCT(Blueprintable)
struct FDialogueLine
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText SubtitleText = FText();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UFMODEvent> Sound = nullptr;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float PreDelay = 0.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float PostDelay = 0.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ESpeakerTag SpeakerTag = ESpeakerTag::None;
};
