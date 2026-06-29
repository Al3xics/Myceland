// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Central FMOD Studio paths. Keep these names synchronized with Game Content.xlsx
// and Myceland_FMOD/Scripts/Create_SFX_Events.js.
namespace MLFMODEvents
{
	// Avatar
	inline constexpr TCHAR AvatarFootstepDirt[] = TEXT("event:/Events/Gameplay/Avatar_Footstep_Dirt");
	inline constexpr TCHAR AvatarEngulfedVocal[] = TEXT("event:/Events/Gameplay/Avatar_Engulfed_Vocal");
	inline constexpr TCHAR AvatarSurpriseLow[] = TEXT("event:/Events/Gameplay/Avatar_Surprise_Low");
	inline constexpr TCHAR AvatarSurpriseMedium[] = TEXT("event:/Events/Gameplay/Avatar_Surprise_Medium");
	inline constexpr TCHAR AvatarSurpriseHigh[] = TEXT("event:/Events/Gameplay/Avatar_Surprise_High");
	inline constexpr TCHAR AvatarVictoryVocal[] = TEXT("event:/Events/Gameplay/Avatar_Victory_Vocal");

	// Tiles and world interactions
	inline constexpr TCHAR TilePlant[] = TEXT("event:/Events/Gameplay/Tile_Plant");
	inline constexpr TCHAR TileParasiteSpread[] = TEXT("event:/Events/Gameplay/Tile_Parasite_Spread");
	inline constexpr TCHAR TileParasiteEngulf[] = TEXT("event:/Events/Gameplay/Tile_Parasite_Engulf");
	inline constexpr TCHAR TileParasiteDieWater[] = TEXT("event:/Events/Gameplay/Tile_Parasite_Die_Water");
	inline constexpr TCHAR TileEarthDig[] = TEXT("event:/Events/Gameplay/Tile_Earth_Dig");
	inline constexpr TCHAR TileWaterFill[] = TEXT("event:/Events/Gameplay/Tile_Water_Fill");
	inline constexpr TCHAR TileBridgeGrow[] = TEXT("event:/Events/Gameplay/Tile_Bridge_Grow");

	// Non-diegetic feedback
	inline constexpr TCHAR TileNaturePlaceSuccess[] = TEXT("event:/Events/Feedback/Tile_Nature_Place_Success");
	inline constexpr TCHAR TilePlacementInvalid[] = TEXT("event:/Events/Feedback/Tile_Placement_Invalid");
	inline constexpr TCHAR EnergyCollect[] = TEXT("event:/Events/Feedback/Energy_Collect");
	inline constexpr TCHAR EnergySpawn[] = TEXT("event:/Events/Feedback/Energy_Spawn");
	inline constexpr TCHAR TimelineUndo[] = TEXT("event:/Events/Feedback/Timeline_Undo");
	inline constexpr TCHAR TimelineReset[] = TEXT("event:/Events/Feedback/Timeline_Reset");
	inline constexpr TCHAR ReactionChainNature[] = TEXT("event:/Events/Feedback/Reaction_Chain_Nature");
	inline constexpr TCHAR ReactionChainParasite[] = TEXT("event:/Events/Feedback/Reaction_Chain_Parasite");
	inline constexpr TCHAR ReactionChainWater[] = TEXT("event:/Events/Feedback/Reaction_Chain_Water");
	inline constexpr TCHAR TreeLinkMotif[] = TEXT("event:/Events/Feedback/Tree_Link_Motif");
}
