// Copyright Myceland Team, All Rights Reserved.

#include "Misc/AutomationTest.h"

#include "Tiles/ML_BoardSpawner.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Truth table of the board locks.
 * AML_BoardSpawner::ResolveLockAction is the single rule deciding what happens to a board's switches
 * (hover glow + board entry) when its Required Puzzles are re-evaluated; it is pure on purpose so the
 * rule can be pinned here without a world, a save or a level. RefreshLockState() only gathers the facts
 * — are the prerequisites solved, is the rule already holding this board, is the board itself solved.
 *
 * The invariant that matters: the rule may only ever give back what it took. A board switched off by a
 * designer, by a win or by Blueprint must never be re-enabled behind their back, which is why Unlock is
 * reachable only from a board the rule locked (bLockedByRule), and never for a solved one.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FML_BoardSpawnerResolveLockActionTest,
                                 "Myceland.Progression.PuzzleLocks",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FML_BoardSpawnerResolveLockActionTest::RunTest(const FString& Parameters)
{
	using EAction = EML_BoardLockAction;
	auto Resolve = &AML_BoardSpawner::ResolveLockAction;
	//                    PrerequisitesSolved, LockedByRule, IsSolved

	// ---------- Waiting on a prerequisite ----------
	TestEqual(TEXT("A board whose prerequisites are unsolved gets locked"),
	          Resolve(false, false, false), EAction::Lock);
	TestEqual(TEXT("An already-locked board is left alone"),
	          Resolve(false, true, false), EAction::None);
	TestEqual(TEXT("A solved board waiting on a prerequisite is still locked (its switches are off anyway)"),
	          Resolve(false, false, true), EAction::Lock);

	// ---------- Prerequisites met ----------
	TestEqual(TEXT("Prerequisites met gives the switches back to a board the rule locked"),
	          Resolve(true, true, false), EAction::Unlock);
	TestEqual(TEXT("A solved board gets released, never switched back on"),
	          Resolve(true, true, true), EAction::Release);

	// ---------- Boards the rule never touched ----------
	// This is the case of every board switched off by a designer, by a win, or by Blueprint: the rule
	// holds nothing on them, so it must do nothing — whatever their prerequisites say.
	TestEqual(TEXT("An open board the rule does not hold is left alone"),
	          Resolve(true, false, false), EAction::None);
	TestEqual(TEXT("A solved board the rule does not hold is left alone"),
	          Resolve(true, false, true), EAction::None);

	// ---------- Invariants over the whole input space ----------
	for (int32 Mask = 0; Mask < 8; ++Mask)
	{
		const bool bPrerequisitesSolved = (Mask & 1) != 0;
		const bool bLockedByRule        = (Mask & 2) != 0;
		const bool bIsSolved            = (Mask & 4) != 0;

		const EAction Action = Resolve(bPrerequisitesSolved, bLockedByRule, bIsSolved);

		// Only a board the rule is holding can ever be switched back on, and never a solved one.
		if (Action == EAction::Unlock)
		{
			TestTrue(FString::Printf(TEXT("Mask %d: Unlock only ever follows a lock this rule took"), Mask),
			         bLockedByRule && !bIsSolved && bPrerequisitesSolved);
		}

		// Locking twice would make the rule claim a board it did not switch off.
		if (Action == EAction::Lock)
		{
			TestFalse(FString::Printf(TEXT("Mask %d: a board is never locked twice"), Mask), bLockedByRule);
		}

		// Whatever happens, a board with unsolved prerequisites never ends up open.
		if (!bPrerequisitesSolved)
		{
			TestNotEqual(FString::Printf(TEXT("Mask %d: unsolved prerequisites never open a board"), Mask),
			             Action, EAction::Unlock);
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
