// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Waves/ML_PropagationWaves.h"
#include "ML_WaveGrass.generated.h"

UCLASS()
class MYCELAND_API UML_WaveGrass : public UML_PropagationWaves
{
	GENERATED_BODY()
	
public:
	virtual void ComputeWave(AML_Tile* OriginTile, TArray<FML_WaveChange>& OutChanges) override;
};
