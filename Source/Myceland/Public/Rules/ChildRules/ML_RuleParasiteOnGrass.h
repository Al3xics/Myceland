// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Rules/ML_PropagationRules.h"
#include "ML_RuleParasiteOnGrass.generated.h"

UCLASS()
class MYCELAND_API UML_RuleParasiteOnGrass : public UML_PropagationRules
{
	GENERATED_BODY()
	
protected:
	virtual void CheckRule() override;
};
