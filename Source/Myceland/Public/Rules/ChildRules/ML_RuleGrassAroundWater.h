// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Rules/ML_PropagationRules.h"
#include "ML_RuleGrassAroundWater.generated.h"

UCLASS()
class MYCELAND_API UML_RuleGrassAroundWater : public UML_PropagationRules
{
	GENERATED_BODY()
	
protected:
	virtual void CheckRule() override;
};
