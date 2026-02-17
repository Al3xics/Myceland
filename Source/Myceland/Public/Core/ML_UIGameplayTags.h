// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "NativeGameplayTags.h"


namespace UIGameplayTags
{
	// ============================ Levels ============================
	MYCELAND_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Level_Menu);
	MYCELAND_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Level_LevelSelectionMenu);
	MYCELAND_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Level_1);
	MYCELAND_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Level_2);
	MYCELAND_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Level_3);
	
	
	// ============================ UI ============================
	// ========== Root ==========
	MYCELAND_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Root_Menu);
	MYCELAND_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Root_Game);
	
	// ========== Widgets ==========
	MYCELAND_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Widget_TitleScreen);
	MYCELAND_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Widget_MainMenu);
	MYCELAND_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Widget_Settings);
}
