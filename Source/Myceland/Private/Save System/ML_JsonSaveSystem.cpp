// Copyright Myceland Team, All Rights Reserved.


#include "Save System/ML_JsonSaveSystem.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

ML_JsonSaveSystem::ML_JsonSaveSystem()
{
}

ML_JsonSaveSystem::~ML_JsonSaveSystem()
{
}

TSharedPtr<FJsonObject> ML_JsonSaveSystem::ReadJsonFile(FString JsonStringPath)
{
	FString JsonString;
	FFileHelper::LoadFileToString(JsonString, *JsonStringPath);

	TSharedPtr<FJsonObject> ReturnedJsonObject;
	FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(JsonString), ReturnedJsonObject);

	return ReturnedJsonObject;
}


void ML_JsonSaveSystem::WriteJsonFile(FString JsonStringPath, TSharedPtr<FJsonObject> JsonObject)
{
	FString JsonString;

	FJsonSerializer::Serialize(JsonObject.ToSharedRef(),TJsonWriterFactory<>::Create(&JsonString));
	FFileHelper::SaveStringToFile(JsonString, *JsonStringPath);

}
