#include "AudioHandlers.h"
#include "HandlerRegistry.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "EditorScriptingUtilities/Public/EditorAssetLibrary.h"
#include "Sound/SoundCue.h"
#include "Factories/SoundCueFactoryNew.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

void FAudioHandlers::RegisterHandlers(FMCPHandlerRegistry& Registry)
{
	Registry.RegisterHandler(TEXT("list_sound_assets"), &ListSoundAssets);
	Registry.RegisterHandler(TEXT("create_sound_cue"), &CreateSoundCue);
	Registry.RegisterHandler(TEXT("create_metasound_source"), &CreateMetaSoundSource);
}

TSharedPtr<FJsonValue> FAudioHandlers::ListSoundAssets(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	bool bRecursive = true;
	Params->TryGetBoolField(TEXT("recursive"), bRecursive);

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

	TArray<FTopLevelAssetPath> ClassPaths;
	ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("SoundWave")));
	ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("SoundCue")));
	ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/MetasoundEngine"), TEXT("MetaSoundSource")));

	TArray<TSharedPtr<FJsonValue>> AssetsArray;

	for (const FTopLevelAssetPath& ClassPath : ClassPaths)
	{
		TArray<FAssetData> AssetDataList;
		AssetRegistry.GetAssetsByClass(ClassPath, AssetDataList, bRecursive);

		for (const FAssetData& AssetData : AssetDataList)
		{
			TSharedPtr<FJsonObject> AssetObj = MakeShared<FJsonObject>();
			AssetObj->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
			AssetObj->SetStringField(TEXT("path"), AssetData.GetObjectPathString());
			AssetObj->SetStringField(TEXT("class"), AssetData.AssetClassPath.GetAssetName().ToString());
			AssetObj->SetStringField(TEXT("packagePath"), AssetData.PackagePath.ToString());
			AssetsArray.Add(MakeShared<FJsonValueObject>(AssetObj));
		}
	}

	Result->SetArrayField(TEXT("assets"), AssetsArray);
	Result->SetNumberField(TEXT("count"), AssetsArray.Num());
	Result->SetBoolField(TEXT("success"), true);

	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FAudioHandlers::CreateSoundCue(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	FString Name;
	if (!Params->TryGetStringField(TEXT("name"), Name))
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing 'name' parameter"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	FString PackagePath = TEXT("/Game/Audio/SoundCues");
	Params->TryGetStringField(TEXT("packagePath"), PackagePath);

	// Delete existing asset if it exists
	FString FullAssetPath = PackagePath + TEXT("/") + Name;
	UEditorAssetLibrary::DeleteAsset(FullAssetPath);

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	IAssetTools& AssetTools = AssetToolsModule.Get();

	USoundCueFactoryNew* SoundCueFactory = NewObject<USoundCueFactoryNew>();
	UObject* NewAsset = AssetTools.CreateAsset(Name, PackagePath, USoundCue::StaticClass(), SoundCueFactory);
	if (!NewAsset)
	{
		Result->SetStringField(TEXT("error"), TEXT("Failed to create SoundCue"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	UEditorAssetLibrary::SaveAsset(NewAsset->GetPathName());

	Result->SetStringField(TEXT("path"), NewAsset->GetPathName());
	Result->SetStringField(TEXT("name"), Name);
	Result->SetBoolField(TEXT("success"), true);

	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FAudioHandlers::CreateMetaSoundSource(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	FString Name;
	if (!Params->TryGetStringField(TEXT("name"), Name))
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing 'name' parameter"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	FString PackagePath = TEXT("/Game/Audio/MetaSounds");
	Params->TryGetStringField(TEXT("packagePath"), PackagePath);

	// Find MetaSoundSource class
	UClass* MetaSoundSourceClass = FindObject<UClass>(nullptr, TEXT("/Script/MetasoundEngine.MetaSoundSource"));
	if (!MetaSoundSourceClass)
	{
		Result->SetStringField(TEXT("error"), TEXT("MetaSoundSource class not found. Enable MetaSound plugin."));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	// Delete existing asset if it exists
	FString FullAssetPath = PackagePath + TEXT("/") + Name;
	UEditorAssetLibrary::DeleteAsset(FullAssetPath);

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	IAssetTools& AssetTools = AssetToolsModule.Get();

	UObject* NewAsset = AssetTools.CreateAsset(Name, PackagePath, MetaSoundSourceClass, nullptr);
	if (!NewAsset)
	{
		Result->SetStringField(TEXT("error"), TEXT("Failed to create MetaSoundSource"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	UEditorAssetLibrary::SaveAsset(NewAsset->GetPathName());

	Result->SetStringField(TEXT("path"), NewAsset->GetPathName());
	Result->SetStringField(TEXT("name"), Name);
	Result->SetBoolField(TEXT("success"), true);

	return MakeShared<FJsonValueObject>(Result);
}
