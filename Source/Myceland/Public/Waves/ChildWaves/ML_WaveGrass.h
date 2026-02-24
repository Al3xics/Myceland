// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Waves/ML_PropagationWaves.h"
#include "ML_WaveGrass.generated.h"

class AML_BoardSpawner;

UCLASS()
class MYCELAND_API UML_WaveGrass : public UML_PropagationWaves
{
	GENERATED_BODY()
	
private:
	static void ExpandWaterNetwork(AML_BoardSpawner* Board, AML_Tile* FromTile, TSet<AML_Tile*>& WaterConnected);
	static bool IsDirtLike(const AML_Tile* Tile);
	
public:
	virtual void ComputeWave(AML_Tile* OriginTile, TArray<FML_WaveChange>& OutChanges) override;
};
