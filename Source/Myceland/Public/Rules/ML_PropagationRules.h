// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ML_PropagationRules.generated.h"

UCLASS(Abstract, Blueprintable)
class MYCELAND_API UML_PropagationRules : public UObject
{
	GENERATED_BODY()
	
protected:
	virtual void CheckRule() PURE_VIRTUAL(UML_PropagationRules::CheckRule, ); // leave ", " because it signifies the void return type
};
