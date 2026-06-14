// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "NativeGameplayTags.h"


namespace ML_GameplayTags
{
	// ============================ Levels ============================
	MYCELAND_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Level_Menu);
	MYCELAND_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Level_WorldMap);
	MYCELAND_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Level_World1_Level0);
	MYCELAND_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Level_World1_Level1);
	MYCELAND_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Level_World1_Level2);
	
	
	// ============================ UI ============================
	// ========== Context ==========
	MYCELAND_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Context_Menu);
	MYCELAND_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Context_Game);
	
	// ========== Widgets ==========
	MYCELAND_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Widget_TitleScreen);
	MYCELAND_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Widget_MainMenu);
	MYCELAND_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Widget_Settings);
	MYCELAND_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Widget_Credits);
	MYCELAND_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Widget_InGameHUD);
	MYCELAND_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Widget_PauseMenu);
	MYCELAND_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Widget_Win);
	MYCELAND_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Widget_Lose);
}
