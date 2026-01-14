// Fill out your copyright notice in the Description page of Project Settings.


#include "JsonSaveSystem.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

JsonSaveSystem::JsonSaveSystem()
{
}

JsonSaveSystem::~JsonSaveSystem()
{
}

TSharedPtr<FJsonObject> JsonSaveSystem::ReadJsonFile(FString JsonStringPath)
{
	FString JsonString;
	FFileHelper::LoadFileToString(JsonString, *JsonStringPath);

	TSharedPtr<FJsonObject> ReturnedJsonObject;
	FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(JsonString), ReturnedJsonObject);

	return ReturnedJsonObject;
}


void JsonSaveSystem::WriteJsonFile(FString JsonStringPath, TSharedPtr<FJsonObject> JsonObject)
{
	FString JsonString;

	FJsonSerializer::Serialize(JsonObject.ToSharedRef(),TJsonWriterFactory<>::Create(&JsonString));
	FFileHelper::SaveStringToFile(JsonString, *JsonStringPath);

}
