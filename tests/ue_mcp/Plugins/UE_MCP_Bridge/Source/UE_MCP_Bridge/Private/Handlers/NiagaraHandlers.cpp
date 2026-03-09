#include "NiagaraHandlers.h"
#include "HandlerRegistry.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "EditorScriptingUtilities/Public/EditorAssetLibrary.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/TopLevelAssetPath.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"

void FNiagaraHandlers::RegisterHandlers(FMCPHandlerRegistry& Registry)
{
	Registry.RegisterHandler(TEXT("list_niagara_systems"), &ListNiagaraSystems);
	Registry.RegisterHandler(TEXT("list_niagara_modules"), &ListNiagaraModules);
	Registry.RegisterHandler(TEXT("create_niagara_system"), &CreateNiagaraSystem);
	Registry.RegisterHandler(TEXT("get_niagara_info"), &GetNiagaraInfo);
	Registry.RegisterHandler(TEXT("list_emitters_in_system"), &ListEmittersInSystem);
	Registry.RegisterHandler(TEXT("create_niagara_emitter"), &CreateNiagaraEmitter);
}

TSharedPtr<FJsonValue> FNiagaraHandlers::ListNiagaraSystems(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	TArray<FAssetData> Assets;
	AR.GetAssetsByClass(FTopLevelAssetPath(TEXT("/Script/Niagara"), TEXT("NiagaraSystem")), Assets, true);

	TArray<TSharedPtr<FJsonValue>> AssetArray;
	for (const FAssetData& Asset : Assets)
	{
		TSharedPtr<FJsonObject> AssetObj = MakeShared<FJsonObject>();
		AssetObj->SetStringField(TEXT("name"), Asset.AssetName.ToString());
		AssetObj->SetStringField(TEXT("path"), Asset.GetObjectPathString());
		AssetArray.Add(MakeShared<FJsonValueObject>(AssetObj));
	}
	Result->SetArrayField(TEXT("systems"), AssetArray);
	Result->SetNumberField(TEXT("count"), AssetArray.Num());
	Result->SetBoolField(TEXT("success"), true);
	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FNiagaraHandlers::ListNiagaraModules(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	TArray<FAssetData> Assets;
	AR.GetAssetsByClass(FTopLevelAssetPath(TEXT("/Script/Niagara"), TEXT("NiagaraScript")), Assets, true);

	TArray<TSharedPtr<FJsonValue>> AssetArray;
	for (const FAssetData& Asset : Assets)
	{
		TSharedPtr<FJsonObject> AssetObj = MakeShared<FJsonObject>();
		AssetObj->SetStringField(TEXT("name"), Asset.AssetName.ToString());
		AssetObj->SetStringField(TEXT("path"), Asset.GetObjectPathString());
		AssetArray.Add(MakeShared<FJsonValueObject>(AssetObj));
	}
	Result->SetArrayField(TEXT("modules"), AssetArray);
	Result->SetNumberField(TEXT("count"), AssetArray.Num());
	Result->SetBoolField(TEXT("success"), true);
	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FNiagaraHandlers::CreateNiagaraSystem(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	FString Name;
	if (!Params->TryGetStringField(TEXT("name"), Name))
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing 'name' parameter"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	FString PackagePath = TEXT("/Game/VFX");
	Params->TryGetStringField(TEXT("packagePath"), PackagePath);

	FString FullAssetPath = PackagePath + TEXT("/") + Name;
	UEditorAssetLibrary::DeleteAsset(FullAssetPath);

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	IAssetTools& AssetTools = AssetToolsModule.Get();

	UObject* NewAsset = AssetTools.CreateAsset(Name, PackagePath, UNiagaraSystem::StaticClass(), nullptr);
	if (!NewAsset)
	{
		Result->SetStringField(TEXT("error"), TEXT("Failed to create NiagaraSystem"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	UEditorAssetLibrary::SaveAsset(NewAsset->GetPathName());

	Result->SetStringField(TEXT("path"), NewAsset->GetPathName());
	Result->SetStringField(TEXT("name"), Name);
	Result->SetBoolField(TEXT("success"), true);
	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FNiagaraHandlers::GetNiagaraInfo(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("assetPath"), AssetPath))
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing 'assetPath' parameter"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *AssetPath);
	if (!System)
	{
		Result->SetStringField(TEXT("error"), FString::Printf(TEXT("NiagaraSystem not found: %s"), *AssetPath));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	Result->SetStringField(TEXT("name"), System->GetName());
	Result->SetStringField(TEXT("path"), AssetPath);

	const TArray<FNiagaraEmitterHandle>& EmitterHandles = System->GetEmitterHandles();
	Result->SetNumberField(TEXT("emitterCount"), EmitterHandles.Num());

	TArray<TSharedPtr<FJsonValue>> EmitterArray;
	for (const FNiagaraEmitterHandle& Handle : EmitterHandles)
	{
		TSharedPtr<FJsonObject> EmitterObj = MakeShared<FJsonObject>();
		EmitterObj->SetStringField(TEXT("name"), Handle.GetName().ToString());
		EmitterObj->SetBoolField(TEXT("enabled"), Handle.GetIsEnabled());
		EmitterArray.Add(MakeShared<FJsonValueObject>(EmitterObj));
	}
	Result->SetArrayField(TEXT("emitters"), EmitterArray);
	Result->SetBoolField(TEXT("success"), true);
	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FNiagaraHandlers::ListEmittersInSystem(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	FString SystemPath;
	if (!Params->TryGetStringField(TEXT("systemPath"), SystemPath))
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing 'systemPath' parameter"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
	if (!System)
	{
		Result->SetStringField(TEXT("error"), FString::Printf(TEXT("NiagaraSystem not found: %s"), *SystemPath));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	const TArray<FNiagaraEmitterHandle>& EmitterHandles = System->GetEmitterHandles();
	TArray<TSharedPtr<FJsonValue>> EmitterArray;
	for (const FNiagaraEmitterHandle& Handle : EmitterHandles)
	{
		TSharedPtr<FJsonObject> EmitterObj = MakeShared<FJsonObject>();
		EmitterObj->SetStringField(TEXT("name"), Handle.GetName().ToString());
		EmitterObj->SetBoolField(TEXT("enabled"), Handle.GetIsEnabled());
		EmitterObj->SetStringField(TEXT("uniqueName"), Handle.GetUniqueInstanceName());
		EmitterArray.Add(MakeShared<FJsonValueObject>(EmitterObj));
	}

	Result->SetArrayField(TEXT("emitters"), EmitterArray);
	Result->SetNumberField(TEXT("count"), EmitterArray.Num());
	Result->SetBoolField(TEXT("success"), true);
	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FNiagaraHandlers::CreateNiagaraEmitter(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	FString Name;
	if (!Params->TryGetStringField(TEXT("name"), Name))
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing 'name' parameter"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	FString PackagePath = TEXT("/Game/VFX");
	Params->TryGetStringField(TEXT("packagePath"), PackagePath);

	// In UE5, standalone NiagaraEmitter creation may not be supported
	// Try to create via AssetTools
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	IAssetTools& AssetTools = AssetToolsModule.Get();

	FString FullAssetPath = PackagePath + TEXT("/") + Name;
	UEditorAssetLibrary::DeleteAsset(FullAssetPath);

	UClass* EmitterClass = FindObject<UClass>(nullptr, TEXT("/Script/Niagara.NiagaraEmitter"));
	if (!EmitterClass)
	{
		Result->SetStringField(TEXT("error"), TEXT("NiagaraEmitter class not found - factory not available"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	UObject* NewAsset = AssetTools.CreateAsset(Name, PackagePath, EmitterClass, nullptr);
	if (!NewAsset)
	{
		Result->SetStringField(TEXT("error"), TEXT("Failed to create NiagaraEmitter - factory not available"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	UEditorAssetLibrary::SaveAsset(NewAsset->GetPathName());

	Result->SetStringField(TEXT("path"), NewAsset->GetPathName());
	Result->SetStringField(TEXT("name"), Name);
	Result->SetBoolField(TEXT("success"), true);
	return MakeShared<FJsonValueObject>(Result);
}
