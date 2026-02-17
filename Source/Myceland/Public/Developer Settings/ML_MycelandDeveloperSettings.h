// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DeveloperSettings.h"
#include "ML_MycelandDeveloperSettings.generated.h"

UCLASS(config=Game, defaultconfig, meta=(DisplayName="Myceland"))
class MYCELAND_API UML_MycelandDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, config, Category="Levels", meta=(ForceInlineRow, Categories="Level"))
	TMap< FGameplayTag, TSoftObjectPtr<UWorld>> Levels;
	
	UFUNCTION(BlueprintPure, Category="Myceland Settings")
	static const UML_MycelandDeveloperSettings* GetMycelandDeveloperSettings()
	{
		return GetDefault<UML_MycelandDeveloperSettings>();
	}
};
