// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Waves/ML_PropagationWaves.h"
#include "ML_WaveCollectible.generated.h"

UCLASS()
class MYCELAND_API UML_WaveCollectible : public UML_PropagationWaves
{
	GENERATED_BODY()
	
public:
	virtual void ComputeWaveForCollectibles(AML_Tile* OriginTile, const TArray<AML_Tile*>& ParasitesThatAteGrass, TArray<FML_WaveChange>& OutChanges) override;
};
