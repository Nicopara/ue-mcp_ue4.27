#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonValue.h"
#include "Dom/JsonObject.h"

class FLandscapeHandlers
{
public:
	static void RegisterHandlers(class FMCPHandlerRegistry& Registry);

private:
	static TSharedPtr<FJsonValue> GetLandscapeInfo(const TSharedPtr<FJsonObject>& Params);
	static TSharedPtr<FJsonValue> ListLandscapeLayers(const TSharedPtr<FJsonObject>& Params);
	static TSharedPtr<FJsonValue> SampleLandscape(const TSharedPtr<FJsonObject>& Params);
	static TSharedPtr<FJsonValue> ListLandscapeSplines(const TSharedPtr<FJsonObject>& Params);
	static TSharedPtr<FJsonValue> GetLandscapeComponent(const TSharedPtr<FJsonObject>& Params);
};
