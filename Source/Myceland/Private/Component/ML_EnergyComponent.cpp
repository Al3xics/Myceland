// Copyright Myceland Team, All Rights Reserved.

#include "Component/ML_EnergyComponent.h"

void UML_EnergyComponent::SetCurrentEnergy(int32 NewEnergy)
{
	// Infinite energy cheat: every write goes through here (planting, entering a board, a rollback
	// restore), so flooring the value once covers all of them - no path can drain the stock.
	if (bInfiniteEnergy)
		NewEnergy = FMath::Max(NewEnergy, InfiniteEnergyAmount);

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
	// Remember the real budget even while the cheat floors the stock, so switching the cheat off
	// hands back a value that matches the board the player actually stands on.
	EnergyWithoutCheat = Energy;

	SetCurrentEnergy(Energy);
}

void UML_EnergyComponent::SetInfiniteEnergy(bool bEnabled, int32 Amount)
{
	if (bInfiniteEnergy == bEnabled)
		return;

	if (bEnabled)
	{
		EnergyWithoutCheat = CurrentEnergy;
		InfiniteEnergyAmount = FMath::Max(Amount, 1);
		bInfiniteEnergy = true;
		SetCurrentEnergy(InfiniteEnergyAmount);
	}
	else
	{
		bInfiniteEnergy = false;
		SetCurrentEnergy(EnergyWithoutCheat);
	}
}
