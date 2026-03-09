#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonValue.h"
#include "Dom/JsonObject.h"

class FAnimationHandlers
{
public:
	static void RegisterHandlers(class FMCPHandlerRegistry& Registry);

private:
	static TSharedPtr<FJsonValue> ListAnimAssets(const TSharedPtr<FJsonObject>& Params);
	static TSharedPtr<FJsonValue> ListSkeletalMeshes(const TSharedPtr<FJsonObject>& Params);
	static TSharedPtr<FJsonValue> GetSkeletonInfo(const TSharedPtr<FJsonObject>& Params);
	static TSharedPtr<FJsonValue> ListSockets(const TSharedPtr<FJsonObject>& Params);
	static TSharedPtr<FJsonValue> GetPhysicsAssetInfo(const TSharedPtr<FJsonObject>& Params);
};
