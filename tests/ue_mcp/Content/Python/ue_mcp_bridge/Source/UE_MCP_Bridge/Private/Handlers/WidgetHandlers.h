#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonValue.h"
#include "Dom/JsonObject.h"

class FWidgetHandlers
{
public:
	static void RegisterHandlers(class FMCPHandlerRegistry& Registry);

private:
	static TSharedPtr<FJsonValue> ListWidgetBlueprints(const TSharedPtr<FJsonObject>& Params);
	static TSharedPtr<FJsonValue> CreateWidgetBlueprint(const TSharedPtr<FJsonObject>& Params);
	static TSharedPtr<FJsonValue> ReadWidgetTree(const TSharedPtr<FJsonObject>& Params);
	static TSharedPtr<FJsonValue> CreateEditorUtilityWidget(const TSharedPtr<FJsonObject>& Params);
	static TSharedPtr<FJsonValue> CreateEditorUtilityBlueprint(const TSharedPtr<FJsonObject>& Params);
};
