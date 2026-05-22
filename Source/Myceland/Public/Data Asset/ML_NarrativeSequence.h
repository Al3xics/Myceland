// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/ML_NarrativeData.h"
#include "Engine/DataAsset.h"
#include "ML_NarrativeSequence.generated.h"


UCLASS()
class MYCELAND_API UML_NarrativeSequence : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue")
	bool bAllowSkip = true;
    
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue")
	TArray<FDialogueLine> DialogueLines;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cinematic")
	bool bIsCinematicMode = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cinematic", meta = (EditCondition = "bIsCinematicMode", ClampMin = "0.1", ClampMax = "5.0"))
	float CameraBlendTime = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cinematic", meta = (EditCondition = "bIsCinematicMode", ToolTip = "If true, the sequence will wait for the camera blend to finish before starting with the first line."))
	bool bWaitForCameraBlendToFinish = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cinematic", meta = (EditCondition = "bIsCinematicMode", ToolTip = "If true, the sequence will wait for the player movement to finish before starting with the first line."))
	bool bWaitForPlayerMovementToFinish = true;
};
