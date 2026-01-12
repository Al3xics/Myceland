#pragma once

#include "CoreMinimal.h"
#include "GameSaveData.generated.h"

USTRUCT(BlueprintType)
struct FGameSaveData
{
	GENERATED_BODY()
	//In game advancement
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Level = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 CurrentPuzzle = 0;

	//Options to save
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Brightness = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MusicVolume = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString PlayerName = TEXT("Player");

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FIntPoint Resolution = FIntPoint(1920, 1080);

	//Player statistics
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int64 TotalPlayTimeSeconds = 0;
};
