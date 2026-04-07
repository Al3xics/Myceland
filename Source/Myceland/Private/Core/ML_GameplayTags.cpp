// Copyright Myceland Team, All Rights Reserved.


#include "Core/ML_GameplayTags.h"

namespace ML_GameplayTags
{
	// ============================ Levels ============================
	UE_DEFINE_GAMEPLAY_TAG(Level_Menu, "Level.Menu");
	UE_DEFINE_GAMEPLAY_TAG(Level_WorldMap, "Level.WorldMap");
	UE_DEFINE_GAMEPLAY_TAG(Level_TutoShowcase, "Level.TutoShowcase");
	UE_DEFINE_GAMEPLAY_TAG(Level_LevelShowcase, "Level.LevelShowcase");
	
	
	// ============================ UI ============================
	// ========== Context ==========
	UE_DEFINE_GAMEPLAY_TAG(UI_Context_Menu, "UI.Context.Menu");
	UE_DEFINE_GAMEPLAY_TAG(UI_Context_Game, "UI.Context.Game");
	
	// ========== Widgets ==========
	UE_DEFINE_GAMEPLAY_TAG(UI_Widget_TitleScreen, "UI.Widget.TitleScreen");
	UE_DEFINE_GAMEPLAY_TAG(UI_Widget_MainMenu, "UI.Widget.MainMenu");
	UE_DEFINE_GAMEPLAY_TAG(UI_Widget_Settings, "UI.Widget.Settings");
	UE_DEFINE_GAMEPLAY_TAG(UI_Widget_Credits, "UI.Widget.Credits");
	UE_DEFINE_GAMEPLAY_TAG(UI_Widget_InGameHUD, "UI.Widget.InGameHUD");
	UE_DEFINE_GAMEPLAY_TAG(UI_Widget_PauseMenu, "UI.Widget.PauseMenu");
	UE_DEFINE_GAMEPLAY_TAG(UI_Widget_Win, "UI.Widget.Win");
	UE_DEFINE_GAMEPLAY_TAG(UI_Widget_Lose, "UI.Widget.Lose");
}

