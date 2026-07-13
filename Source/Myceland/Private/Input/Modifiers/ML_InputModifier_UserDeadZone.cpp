// Copyright Myceland Team, All Rights Reserved.


#include "Input/Modifiers/ML_InputModifier_UserDeadZone.h"

#include "User Settings/ML_GameUserSettings.h"

FInputActionValue UML_InputModifier_UserDeadZone::ModifyRaw_Implementation(const UEnhancedPlayerInput* PlayerInput, FInputActionValue CurrentValue, float DeltaTime)
{
	// Can't apply a dead zone to a boolean type (0 or 1 are the only options)
	const EInputActionValueType ValueType = CurrentValue.GetValueType();
	if (ValueType == EInputActionValueType::Boolean)
		return CurrentValue;

	const UML_GameUserSettings* Settings = UML_GameUserSettings::GetMycelandGameUserSettings();
	const float DeadZone = FMath::Clamp(Settings ? Settings->GetGamepadDeadZone() : 0.1f, 0.f, 0.99f);

	// Remove the dead zone, then rescale so the output still ramps from 0 at the dead zone
	// edge up to 1 at full deflection (no jump when the stick leaves the zone).
	auto DeadZoneLambda = [DeadZone](const float AxisVal) -> float
	{
		return FMath::Min(1.f, FMath::Max(0.f, FMath::Abs(AxisVal) - DeadZone) / (1.f - DeadZone)) * FMath::Sign(AxisVal);
	};

	// Radial: the dead zone applies to the stick deflection as a whole, not per axis,
	// so diagonals behave the same as cardinal directions.
	FVector NewValue = CurrentValue.Get<FVector>();
	switch (ValueType)
	{
		case EInputActionValueType::Axis3D:
			NewValue = NewValue.GetSafeNormal() * DeadZoneLambda(NewValue.Size());
			break;

		case EInputActionValueType::Axis2D:
			NewValue = NewValue.GetSafeNormal2D() * DeadZoneLambda(NewValue.Size2D());
			break;

		default:
			NewValue.X = DeadZoneLambda(NewValue.X);
			break;
	}

	return FInputActionValue(ValueType, NewValue);
}
