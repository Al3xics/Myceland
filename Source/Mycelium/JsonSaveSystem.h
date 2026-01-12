// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Templates/SharedPointer.h"

/**
 * 
 */
class MYCELIUM_API JsonSaveSystem
{
public:
	JsonSaveSystem();
	~JsonSaveSystem();

	static void WriteJsonFile(FString JsonStringPath, TSharedPtr<FJsonObject> JsonObject);

	static TSharedPtr<FJsonObject> ReadJsonFile(FString JsonStringPath);
};
