// Copyright Myceland Team, All Rights Reserved.

#include "Player/ML_EnergyComponent.h"

void UML_EnergyComponent::SetCurrentEnergy(int32 NewEnergy)
{
	NewEnergy = FMath::Clamp(NewEnergy, 0, INT32_MAX);

	if (CurrentEnergy == NewEnergy)
		return;

	CurrentEnergy = NewEnergy;
	OnEnergyChanged.Broadcast(CurrentEnergy);
}

void UML_EnergyComponent::AddEnergy(int32 Delta)
{
	SetCurrentEnergy(CurrentEnergy + Delta);
}

void UML_EnergyComponent::InitNumberOfEnergyForLevel(int32 Energy)
{
	SetCurrentEnergy(Energy);
}
