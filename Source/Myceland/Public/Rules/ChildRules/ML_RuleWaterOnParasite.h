// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Rules/ML_PropagationRules.h"
#include "ML_RuleWaterOnParasite.generated.h"

UCLASS()
class MYCELAND_API UML_RuleWaterOnParasite : public UML_PropagationRules
{
	GENERATED_BODY()
	
protected:
	virtual void CheckRule() override;
};
