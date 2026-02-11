// Copyright Myceland Team, All Rights Reserved.


#include "Save System/ML_JsonConsoleCommands.h"
#include "Save System/ML_JsonSaveSystem.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"

// Global-scope static console command
static FAutoConsoleCommand TestSaveCmd(
	TEXT("Test.Save"),
	TEXT("Tests JSON save"),
	FConsoleCommandDelegate::CreateStatic([]()
	{
		ML_JsonSaveSystem SaveSystem;
		FString Path = FPaths::ProjectSavedDir() / TEXT("ConsoleTest.json");

		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetNumberField("Level", 8);
		Obj->SetStringField("PlayerName", "Babu");
		Obj->SetNumberField("MusicVolume", 25);
		Obj->SetStringField("Resolution", "1920*1080");

		SaveSystem.WriteJsonFile(Path, Obj);

		UE_LOG(LogTemp, Warning, TEXT("Test.Save executed"));
	})
);
static FAutoConsoleCommand TestLoadCmd(
	TEXT("Test.Load"),
	TEXT("Tests JSON load"),
	FConsoleCommandDelegate::CreateStatic([]()
	{
		ML_JsonSaveSystem SaveSystem;
		FString Path = FPaths::ProjectSavedDir() / TEXT("ConsoleTest.json");

		TSharedPtr<FJsonObject> LoadedJson = SaveSystem.ReadJsonFile(Path);

		if (LoadedJson.IsValid())
		{
			FString PlayerName;
			int32 Music = 0, Level = 0;
			FString Resolution;

			LoadedJson->TryGetStringField(TEXT("PlayerName"), PlayerName);
			LoadedJson->TryGetNumberField(TEXT("MusicVolume"),  Music); 
			LoadedJson->TryGetNumberField(TEXT("Level"), Level); 
			LoadedJson->TryGetStringField(TEXT("Resolution"), Resolution);


			UE_LOG(LogTemp, Warning, TEXT("Loaded JSON: %s Level=%d Music=%d Res=%s"),
			       *PlayerName, Level, Music, *Resolution);


			UE_LOG(LogTemp, Warning, TEXT("Test.Load executed"));
		}
	})
);
