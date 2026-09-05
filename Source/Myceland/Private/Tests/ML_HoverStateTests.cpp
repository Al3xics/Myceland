// Copyright Myceland Team, All Rights Reserved.

#include "Misc/AutomationTest.h"

#include "Component/ML_HoverPreviewComponent.h"
#include "Core/ML_CoreData.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Truth table of the cursor / selection glow.
 * UML_HoverPreviewComponent::ResolveHoverState is the single rule deciding which color a hovered tile
 * shows; it is pure on purpose so the rule can be pinned here without a world, a player or a board.
 * ComputeHoverState() only gathers the facts (is it the player's tile, is it plantable, is there
 * energy left, is it walkable) and hands them over.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FML_HoverPreviewResolveHoverStateTest,
                                 "Myceland.HoverPreview.ResolveHoverState",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FML_HoverPreviewResolveHoverStateTest::RunTest(const FString& Parameters)
{
	using EState = EML_TileHoverState;
	auto Resolve = &UML_HoverPreviewComponent::ResolveHoverState;
	//                          Interactable, PlayerTile, CanPlant, HasEnergy, Walkable

	// ---------- Not interactable: the obstacle ring framing a board reads as empty space ----------
	TestEqual(TEXT("Non-interactable tile shows no glow"),
	          Resolve(false, false, false, false, false), EState::None);
	TestEqual(TEXT("Non-interactable tile shows no glow even under the player"),
	          Resolve(false, true, true, true, true), EState::None);

	// ---------- The player's own tile wins over every other role ----------
	TestEqual(TEXT("Player tile shows the player state"),
	          Resolve(true, true, false, false, true), EState::Player);
	TestEqual(TEXT("Player tile wins over plantable"),
	          Resolve(true, true, true, true, true), EState::Player);

	// ---------- Plantable, but only while the plant can actually happen ----------
	TestEqual(TEXT("Plantable tile with energy left is plantable"),
	          Resolve(true, false, true, true, true), EState::Plantable);
	TestEqual(TEXT("Plantable tile without energy falls back to walkable"),
	          Resolve(true, false, true, false, true), EState::Walkable);
	TestEqual(TEXT("Plantable wins over walkable"),
	          Resolve(true, false, true, true, false), EState::Plantable);

	// ---------- Walkable / blocked ----------
	TestEqual(TEXT("Walkable non-plantable tile (grass, water path) is walkable"),
	          Resolve(true, false, false, true, true), EState::Walkable);
	TestEqual(TEXT("Non-walkable tile (water, parasite, obstacle, tree) is blocked"),
	          Resolve(true, false, false, true, false), EState::Blocked);
	TestEqual(TEXT("Energy alone never changes a non-plantable tile"),
	          Resolve(true, false, false, false, false), EState::Blocked);

	// ---------- Invariants over the whole input space ----------
	for (int32 Mask = 0; Mask < 32; ++Mask)
	{
		const bool bInteractable = (Mask & 1) != 0;
		const bool bPlayerTile   = (Mask & 2) != 0;
		const bool bCanPlant     = (Mask & 4) != 0;
		const bool bHasEnergy    = (Mask & 8) != 0;
		const bool bWalkable     = (Mask & 16) != 0;

		const EState State = Resolve(bInteractable, bPlayerTile, bCanPlant, bHasEnergy, bWalkable);

		// None means "nothing to show" and must stay reserved for non-interactable tiles, otherwise a
		// hovered tile could end up with no glow at all while the cursor sits on it.
		TestEqual(FString::Printf(TEXT("Mask %d: None if and only if the tile is not interactable"), Mask),
		          State == EState::None, !bInteractable);

		if (bInteractable && bPlayerTile)
		{
			TestEqual(FString::Printf(TEXT("Mask %d: the player's tile always wins"), Mask),
			          State, EState::Player);
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
