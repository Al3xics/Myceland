// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ML_DialogueSpeaker.generated.h"

// This class does not need to be modified.
UINTERFACE()
class UML_DialogueSpeaker : public UInterface
{
	GENERATED_BODY()
};

class MYCELAND_API IML_DialogueSpeaker
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	virtual void SetIsTalking(const bool bTalking) = 0;
	virtual bool IsTalking() const = 0;
};
