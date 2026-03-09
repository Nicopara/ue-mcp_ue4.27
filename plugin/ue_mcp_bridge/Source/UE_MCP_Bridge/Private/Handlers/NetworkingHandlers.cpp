#include "NetworkingHandlers.h"
#include "HandlerRegistry.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

void FNetworkingHandlers::RegisterHandlers(FMCPHandlerRegistry& Registry)
{
	Registry.RegisterHandler(TEXT("get_networking_info"), &GetNetworkingInfo);
	Registry.RegisterHandler(TEXT("set_replicates"), &SetReplicates);
	Registry.RegisterHandler(TEXT("configure_net_update_frequency"), &ConfigureNetUpdateFrequency);
	Registry.RegisterHandler(TEXT("set_net_dormancy"), &SetNetDormancy);
	Registry.RegisterHandler(TEXT("set_always_relevant"), &SetAlwaysRelevant);
	Registry.RegisterHandler(TEXT("set_net_priority"), &SetNetPriority);
	Registry.RegisterHandler(TEXT("set_replicate_movement"), &SetReplicateMovement);
}

AActor* FNetworkingHandlers::LoadBlueprintCDO(const FString& BlueprintPath, TSharedPtr<FJsonObject>& OutResult)
{
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
	if (!Blueprint || !Blueprint->GeneratedClass)
	{
		OutResult->SetStringField(TEXT("error"), FString::Printf(TEXT("Blueprint not found or has no generated class: %s"), *BlueprintPath));
		OutResult->SetBoolField(TEXT("success"), false);
		return nullptr;
	}

	AActor* CDO = Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject());
	if (!CDO)
	{
		OutResult->SetStringField(TEXT("error"), TEXT("CDO is not an Actor"));
		OutResult->SetBoolField(TEXT("success"), false);
		return nullptr;
	}

	return CDO;
}

void FNetworkingHandlers::SaveBlueprint(UBlueprint* Blueprint)
{
	if (!Blueprint) return;
	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	UPackage* Package = Blueprint->GetOutermost();
	if (Package)
	{
		Package->MarkPackageDirty();
		FString PackageFileName = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Standalone;
		UPackage::SavePackage(Package, nullptr, *PackageFileName, SaveArgs);
	}
}

TSharedPtr<FJsonValue> FNetworkingHandlers::GetNetworkingInfo(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	FString BlueprintPath;
	if (!Params->TryGetStringField(TEXT("blueprintPath"), BlueprintPath))
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing 'blueprintPath' parameter"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	AActor* CDO = LoadBlueprintCDO(BlueprintPath, Result);
	if (!CDO) return MakeShared<FJsonValueObject>(Result);

	Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
	Result->SetBoolField(TEXT("replicates"), CDO->GetIsReplicated());
	Result->SetNumberField(TEXT("netUpdateFrequency"), CDO->NetUpdateFrequency);
	Result->SetNumberField(TEXT("minNetUpdateFrequency"), CDO->MinNetUpdateFrequency);
	Result->SetNumberField(TEXT("netPriority"), CDO->NetPriority);
	Result->SetBoolField(TEXT("alwaysRelevant"), CDO->bAlwaysRelevant);
	Result->SetBoolField(TEXT("replicateMovement"), CDO->IsReplicatingMovement());
	Result->SetNumberField(TEXT("netDormancy"), (int32)CDO->NetDormancy);
	Result->SetBoolField(TEXT("success"), true);

	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FNetworkingHandlers::SetReplicates(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	FString BlueprintPath;
	if (!Params->TryGetStringField(TEXT("blueprintPath"), BlueprintPath))
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing 'blueprintPath' parameter"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	AActor* CDO = LoadBlueprintCDO(BlueprintPath, Result);
	if (!CDO) return MakeShared<FJsonValueObject>(Result);

	bool bReplicates = false;
	Params->TryGetBoolField(TEXT("replicates"), bReplicates);
	CDO->SetReplicates(bReplicates);

	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
	SaveBlueprint(Blueprint);

	Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
	Result->SetBoolField(TEXT("replicates"), bReplicates);
	Result->SetBoolField(TEXT("success"), true);
	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FNetworkingHandlers::ConfigureNetUpdateFrequency(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	FString BlueprintPath;
	if (!Params->TryGetStringField(TEXT("blueprintPath"), BlueprintPath))
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing 'blueprintPath' parameter"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	AActor* CDO = LoadBlueprintCDO(BlueprintPath, Result);
	if (!CDO) return MakeShared<FJsonValueObject>(Result);

	double NetUpdateFrequency = 0;
	if (Params->TryGetNumberField(TEXT("netUpdateFrequency"), NetUpdateFrequency))
	{
		CDO->NetUpdateFrequency = (float)NetUpdateFrequency;
	}
	double MinNetUpdateFrequency = 0;
	if (Params->TryGetNumberField(TEXT("minNetUpdateFrequency"), MinNetUpdateFrequency))
	{
		CDO->MinNetUpdateFrequency = (float)MinNetUpdateFrequency;
	}

	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
	SaveBlueprint(Blueprint);

	Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
	Result->SetNumberField(TEXT("netUpdateFrequency"), CDO->NetUpdateFrequency);
	Result->SetNumberField(TEXT("minNetUpdateFrequency"), CDO->MinNetUpdateFrequency);
	Result->SetBoolField(TEXT("success"), true);
	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FNetworkingHandlers::SetNetDormancy(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	FString BlueprintPath;
	if (!Params->TryGetStringField(TEXT("blueprintPath"), BlueprintPath))
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing 'blueprintPath' parameter"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	AActor* CDO = LoadBlueprintCDO(BlueprintPath, Result);
	if (!CDO) return MakeShared<FJsonValueObject>(Result);

	FString Dormancy;
	if (Params->TryGetStringField(TEXT("dormancy"), Dormancy))
	{
		if (Dormancy == TEXT("DORM_Never"))
			CDO->NetDormancy = DORM_Never;
		else if (Dormancy == TEXT("DORM_Awake"))
			CDO->NetDormancy = DORM_Awake;
		else if (Dormancy == TEXT("DORM_DormantAll"))
			CDO->NetDormancy = DORM_DormantAll;
		else if (Dormancy == TEXT("DORM_DormantPartial"))
			CDO->NetDormancy = DORM_DormantPartial;
		else if (Dormancy == TEXT("DORM_Initial"))
			CDO->NetDormancy = DORM_Initial;
	}

	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
	SaveBlueprint(Blueprint);

	Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
	Result->SetNumberField(TEXT("netDormancy"), (int32)CDO->NetDormancy);
	Result->SetBoolField(TEXT("success"), true);
	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FNetworkingHandlers::SetAlwaysRelevant(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	FString BlueprintPath;
	if (!Params->TryGetStringField(TEXT("blueprintPath"), BlueprintPath))
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing 'blueprintPath' parameter"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	AActor* CDO = LoadBlueprintCDO(BlueprintPath, Result);
	if (!CDO) return MakeShared<FJsonValueObject>(Result);

	bool bAlwaysRelevant = false;
	Params->TryGetBoolField(TEXT("alwaysRelevant"), bAlwaysRelevant);
	CDO->bAlwaysRelevant = bAlwaysRelevant;

	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
	SaveBlueprint(Blueprint);

	Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
	Result->SetBoolField(TEXT("alwaysRelevant"), bAlwaysRelevant);
	Result->SetBoolField(TEXT("success"), true);
	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FNetworkingHandlers::SetNetPriority(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	FString BlueprintPath;
	if (!Params->TryGetStringField(TEXT("blueprintPath"), BlueprintPath))
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing 'blueprintPath' parameter"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	AActor* CDO = LoadBlueprintCDO(BlueprintPath, Result);
	if (!CDO) return MakeShared<FJsonValueObject>(Result);

	double NetPriority = 1.0;
	if (Params->TryGetNumberField(TEXT("netPriority"), NetPriority))
	{
		CDO->NetPriority = (float)NetPriority;
	}

	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
	SaveBlueprint(Blueprint);

	Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
	Result->SetNumberField(TEXT("netPriority"), CDO->NetPriority);
	Result->SetBoolField(TEXT("success"), true);
	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FNetworkingHandlers::SetReplicateMovement(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	FString BlueprintPath;
	if (!Params->TryGetStringField(TEXT("blueprintPath"), BlueprintPath))
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing 'blueprintPath' parameter"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	AActor* CDO = LoadBlueprintCDO(BlueprintPath, Result);
	if (!CDO) return MakeShared<FJsonValueObject>(Result);

	bool bReplicateMovement = false;
	Params->TryGetBoolField(TEXT("replicateMovement"), bReplicateMovement);
	CDO->SetReplicatingMovement(bReplicateMovement);

	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
	SaveBlueprint(Blueprint);

	Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
	Result->SetBoolField(TEXT("replicateMovement"), bReplicateMovement);
	Result->SetBoolField(TEXT("success"), true);
	return MakeShared<FJsonValueObject>(Result);
}
