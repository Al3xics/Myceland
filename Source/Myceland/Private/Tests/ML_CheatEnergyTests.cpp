// Copyright Myceland Team, All Rights Reserved.

#include "Misc/AutomationTest.h"

#include "Component/ML_EnergyComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Infinite-energy cheat (UML_CheatSubsystem::Cheat_ToggleInfiniteEnergy).
 *
 * The cheat floors the stock inside SetCurrentEnergy, which every other path goes through:
 * planting (AddEnergy(-1)), entering a board (InitNumberOfEnergyForLevel) and the rollback
 * restores (SetCurrentEnergy of the recorded value). This pins both halves of that: the component
 * must behave exactly as before while the cheat is off, and nothing must drain it while it is on.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FML_EnergyInfiniteCheatTest,
                                 "Myceland.Cheats.InfiniteEnergy",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FML_EnergyInfiniteCheatTest::RunTest(const FString& Parameters)
{
	UML_EnergyComponent* Energy = NewObject<UML_EnergyComponent>();
	if (!TestNotNull(TEXT("Energy component created"), Energy))
		return false;

	// ---------- Cheat OFF: unchanged behaviour ----------

	Energy->InitNumberOfEnergyForLevel(5);
	TestEqual(TEXT("Init sets the board budget"), Energy->GetCurrentEnergy(), 5);

	Energy->AddEnergy(-1);
	TestEqual(TEXT("Planting spends one energy"), Energy->GetCurrentEnergy(), 4);

	Energy->AddEnergy(+1);
	TestEqual(TEXT("Picking a collectible gives one back"), Energy->GetCurrentEnergy(), 5);

	Energy->SetCurrentEnergy(-3);
	TestEqual(TEXT("Energy never goes below zero"), Energy->GetCurrentEnergy(), 0);

	TestFalse(TEXT("Infinite energy is off by default"), Energy->IsInfiniteEnergy());

	// ---------- Cheat ON ----------

	Energy->InitNumberOfEnergyForLevel(5);
	Energy->SetInfiniteEnergy(true, 99);

	TestTrue(TEXT("Infinite energy reports itself as on"), Energy->IsInfiniteEnergy());
	TestEqual(TEXT("Enabling floors the stock at the cheat amount"), Energy->GetCurrentEnergy(), 99);

	// Planting: the case the cheat exists for.
	Energy->AddEnergy(-1);
	Energy->AddEnergy(-10);
	TestEqual(TEXT("Planting cannot drain the stock"), Energy->GetCurrentEnergy(), 99);

	// Entering another board would normally reset the stock to that board's budget.
	Energy->InitNumberOfEnergyForLevel(3);
	TestEqual(TEXT("Changing board cannot reset the stock"), Energy->GetCurrentEnergy(), 99);

	// The rollback restores a recorded value with a direct write, bypassing AddEnergy.
	Energy->SetCurrentEnergy(2);
	TestEqual(TEXT("A rollback restore cannot lower the stock"), Energy->GetCurrentEnergy(), 99);

	// Gaining energy above the floor still works.
	Energy->AddEnergy(+5);
	TestEqual(TEXT("Collectibles still add on top of the floor"), Energy->GetCurrentEnergy(), 104);

	// ---------- Cheat OFF again ----------

	Energy->SetInfiniteEnergy(false);

	TestFalse(TEXT("Infinite energy reports itself as off"), Energy->IsInfiniteEnergy());
	TestEqual(TEXT("Disabling hands back the budget of the board the player is on"),
	          Energy->GetCurrentEnergy(), 3);

	Energy->AddEnergy(-1);
	TestEqual(TEXT("Planting spends again once the cheat is off"), Energy->GetCurrentEnergy(), 2);

	// ---------- Toggling twice is a no-op ----------

	Energy->SetInfiniteEnergy(false);
	TestEqual(TEXT("Disabling an already disabled cheat changes nothing"), Energy->GetCurrentEnergy(), 2);

	Energy->SetInfiniteEnergy(true, 99);
	Energy->SetInfiniteEnergy(true, 42);
	TestEqual(TEXT("Enabling an already enabled cheat keeps the first amount"),
	          Energy->GetCurrentEnergy(), 99);

	Energy->SetInfiniteEnergy(false);
	TestEqual(TEXT("The stock the player had before the cheat comes back"), Energy->GetCurrentEnergy(), 2);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
