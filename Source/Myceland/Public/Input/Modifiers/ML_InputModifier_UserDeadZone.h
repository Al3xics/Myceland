// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputModifiers.h"
#include "ML_InputModifier_UserDeadZone.generated.h"

/**
 * Radial dead zone whose threshold is the player's GamepadDeadZone user setting,
 * read every frame (no event needed when the setting changes).
 */
UCLASS(NotBlueprintable, meta = (DisplayName = "User Dead Zone"))
class MYCELAND_API UML_InputModifier_UserDeadZone : public UInputModifier
{
	GENERATED_BODY()

protected:
	virtual FInputActionValue ModifyRaw_Implementation(const UEnhancedPlayerInput* PlayerInput, FInputActionValue CurrentValue, float DeltaTime) override;
};
