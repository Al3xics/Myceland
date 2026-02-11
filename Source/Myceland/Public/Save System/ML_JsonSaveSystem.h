// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Templates/SharedPointer.h"

/**
 * 
 */
class MYCELAND_API ML_JsonSaveSystem
{
public:
	ML_JsonSaveSystem();
	~ML_JsonSaveSystem();

	static void WriteJsonFile(FString JsonStringPath, TSharedPtr<FJsonObject> JsonObject);

	static TSharedPtr<FJsonObject> ReadJsonFile(FString JsonStringPath);
};
