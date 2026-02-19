// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/ML_CoreData.h"
#include "UObject/Object.h"
#include "ML_PropagationWaves.generated.h"

class AML_Tile;

UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced)
class MYCELAND_API UML_PropagationWaves : public UObject
{
	GENERATED_BODY()
	
public:
	virtual void ComputeWave(AML_Tile* OriginTile, TArray<FML_WaveChange>& OutChanges) PURE_VIRTUAL(UML_PropagationWaves::ComputeWave, ); // leave ", " because it signifies the void return type
	virtual void ComputeWaveForCollectibles(AML_Tile* OriginTile, const TArray<AML_Tile*>& ParasitesThatAteGrass, TArray<FML_WaveChange>& OutChanges) PURE_VIRTUAL(UML_PropagationWaves::ComputeWaveForCollectibles, ); // leave ", " because it signifies the void return type
};
