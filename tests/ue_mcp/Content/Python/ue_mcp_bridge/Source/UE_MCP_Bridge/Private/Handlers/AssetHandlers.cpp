#include "AssetHandlers.h"
#include "HandlerRegistry.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#include "EditorScriptingUtilities/Public/EditorAssetLibrary.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "UObject/TopLevelAssetPath.h"

void FAssetHandlers::RegisterHandlers(FMCPHandlerRegistry& Registry)
{
	Registry.RegisterHandler(TEXT("list_assets"), &ListAssets);
	Registry.RegisterHandler(TEXT("search_assets"), &SearchAssets);
	Registry.RegisterHandler(TEXT("read_asset"), &ReadAsset);
	Registry.RegisterHandler(TEXT("read_asset_properties"), &ReadAssetProperties);
	Registry.RegisterHandler(TEXT("duplicate_asset"), &DuplicateAsset);
	Registry.RegisterHandler(TEXT("rename_asset"), &RenameAsset);
	Registry.RegisterHandler(TEXT("move_asset"), &MoveAsset);
	Registry.RegisterHandler(TEXT("delete_asset"), &DeleteAsset);
	Registry.RegisterHandler(TEXT("save_asset"), &SaveAsset);
	Registry.RegisterHandler(TEXT("list_textures"), &ListTextures);
}

TSharedPtr<FJsonValue> FAssetHandlers::ListAssets(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	FString Query = TEXT("*");
	Params->TryGetStringField(TEXT("query"), Query);

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAllAssets(AssetDataList);

	TArray<TSharedPtr<FJsonValue>> AssetsArray;
	for (const FAssetData& AssetData : AssetDataList)
	{
		FString AssetPath = AssetData.ObjectPath.ToString();
		if (Query == TEXT("*") || AssetPath.Contains(Query))
		{
			TSharedPtr<FJsonObject> AssetObj = MakeShared<FJsonObject>();
			AssetObj->SetStringField(TEXT("path"), AssetPath);
			AssetObj->SetStringField(TEXT("className"), AssetData.AssetClass.ToString());
			AssetObj->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
			AssetsArray.Add(MakeShared<FJsonValueObject>(AssetObj));
		}
	}

	Result->SetArrayField(TEXT("assets"), AssetsArray);
	Result->SetNumberField(TEXT("count"), AssetsArray.Num());
	Result->SetBoolField(TEXT("success"), true);

	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FAssetHandlers::SearchAssets(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	FString Query;
	Params->TryGetStringField(TEXT("query"), Query);
	FString Directory = TEXT("/Game/");
	Params->TryGetStringField(TEXT("directory"), Directory);
	int32 MaxResults = 50;
	Params->TryGetNumberField(TEXT("maxResults"), MaxResults);

	TArray<FString> AssetPaths = UEditorAssetLibrary::ListAssets(Directory, true, false);

	TArray<TSharedPtr<FJsonValue>> ResultsArray;
	FString QueryLower = Query.ToLower();
	for (const FString& AssetPath : AssetPaths)
	{
		if (ResultsArray.Num() >= MaxResults) break;
		if (!Query.IsEmpty() && !AssetPath.ToLower().Contains(QueryLower)) continue;

		TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetStringField(TEXT("path"), AssetPath);
		FString Name;
		int32 SlashIdx = 0;
		if (AssetPath.FindLastChar(TEXT('/'), SlashIdx))
		{
			Name = AssetPath.Mid(SlashIdx + 1);
			int32 DotIdx = 0;
			if (Name.FindChar(TEXT('.'), DotIdx))
			{
				Name = Name.Left(DotIdx);
			}
		}
		else
		{
			Name = AssetPath;
		}
		Item->SetStringField(TEXT("name"), Name);

		FAssetData AssetData = UEditorAssetLibrary::FindAssetData(AssetPath);
		if (AssetData.IsValid())
		{
			Item->SetStringField(TEXT("className"), AssetData.AssetClassPath.GetAssetName().ToString());
		}
		ResultsArray.Add(MakeShared<FJsonValueObject>(Item));
	}

	Result->SetStringField(TEXT("query"), Query);
	Result->SetStringField(TEXT("directory"), Directory);
	Result->SetNumberField(TEXT("resultCount"), ResultsArray.Num());
	Result->SetArrayField(TEXT("results"), ResultsArray);
	Result->SetBoolField(TEXT("success"), true);

	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FAssetHandlers::ReadAsset(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("path"), AssetPath))
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing 'path' parameter"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	UObject* Asset = LoadObject<UObject>(nullptr, *AssetPath);
	if (!Asset)
	{
		Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Asset not found: %s"), *AssetPath));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	Result->SetStringField(TEXT("path"), AssetPath);
	Result->SetStringField(TEXT("className"), Asset->GetClass()->GetName());
	Result->SetStringField(TEXT("objectName"), Asset->GetName());
	Result->SetBoolField(TEXT("success"), true);

	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FAssetHandlers::ReadAssetProperties(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("path"), AssetPath))
	{
		Params->TryGetStringField(TEXT("assetPath"), AssetPath);
	}
	if (AssetPath.IsEmpty())
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing 'path' or 'assetPath' parameter"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
	if (!Asset)
	{
		Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Asset not found: %s"), *AssetPath));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	FString PropertyName;
	if (Params->TryGetStringField(TEXT("propertyName"), PropertyName) && !PropertyName.IsEmpty())
	{
		FProperty* Prop = Asset->GetClass()->FindPropertyByName(*PropertyName);
		if (!Prop)
		{
			Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Property not found: %s"), *PropertyName));
			Result->SetBoolField(TEXT("success"), false);
			return MakeShared<FJsonValueObject>(Result);
		}
		// Basic serialization - extend as needed
		Result->SetStringField(TEXT("path"), AssetPath);
		Result->SetStringField(TEXT("propertyName"), PropertyName);
		Result->SetStringField(TEXT("type"), Prop->GetCPPType());
		Result->SetBoolField(TEXT("success"), true);
		return MakeShared<FJsonValueObject>(Result);
	}

	TArray<TSharedPtr<FJsonValue>> PropsArray;
	for (TFieldIterator<FProperty> It(Asset->GetClass()); It; ++It)
	{
		TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>();
		P->SetStringField(TEXT("name"), (*It)->GetName());
		P->SetStringField(TEXT("type"), (*It)->GetCPPType());
		PropsArray.Add(MakeShared<FJsonValueObject>(P));
	}
	Result->SetStringField(TEXT("path"), AssetPath);
	Result->SetStringField(TEXT("className"), Asset->GetClass()->GetName());
	Result->SetNumberField(TEXT("propertyCount"), PropsArray.Num());
	Result->SetArrayField(TEXT("properties"), PropsArray);
	Result->SetBoolField(TEXT("success"), true);

	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FAssetHandlers::DuplicateAsset(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	FString SourcePath, DestPath;
	if (!Params->TryGetStringField(TEXT("sourcePath"), SourcePath) || !Params->TryGetStringField(TEXT("destinationPath"), DestPath))
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing 'sourcePath' and 'destinationPath'"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	if (!UEditorAssetLibrary::DoesAssetExist(SourcePath))
	{
		Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Source asset not found: %s"), *SourcePath));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	UObject* Dup = UEditorAssetLibrary::DuplicateAsset(SourcePath, DestPath);
	Result->SetStringField(TEXT("sourcePath"), SourcePath);
	Result->SetStringField(TEXT("destinationPath"), DestPath);
	Result->SetBoolField(TEXT("success"), Dup != nullptr);

	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FAssetHandlers::RenameAsset(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	FString SourcePath, DestPath;
	if (Params->TryGetStringField(TEXT("sourcePath"), SourcePath) && Params->TryGetStringField(TEXT("destinationPath"), DestPath))
	{
		// Use sourcePath/destinationPath directly
	}
	else
	{
		FString AssetPath, NewName;
		if (Params->TryGetStringField(TEXT("assetPath"), AssetPath) && Params->TryGetStringField(TEXT("newName"), NewName))
		{
			SourcePath = AssetPath;
			FString PackageName, AssetName;
			AssetPath.Split(TEXT("."), &PackageName, &AssetName, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
			FString ParentDir = FPaths::GetPath(PackageName);
			if (ParentDir.IsEmpty()) ParentDir = PackageName;
			DestPath = FString::Printf(TEXT("%s/%s.%s"), *ParentDir, *NewName, *NewName);
		}
	}

	if (SourcePath.IsEmpty() || DestPath.IsEmpty())
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing 'sourcePath'+'destinationPath' or 'assetPath'+'newName'"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	if (!UEditorAssetLibrary::DoesAssetExist(SourcePath))
	{
		Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Asset not found: %s"), *SourcePath));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	bool bOk = UEditorAssetLibrary::RenameAsset(SourcePath, DestPath);
	Result->SetStringField(TEXT("sourcePath"), SourcePath);
	Result->SetStringField(TEXT("destinationPath"), DestPath);
	Result->SetBoolField(TEXT("success"), bOk);

	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FAssetHandlers::MoveAsset(const TSharedPtr<FJsonObject>& Params)
{
	// Move is equivalent to Rename in UE
	return RenameAsset(Params);
}

TSharedPtr<FJsonValue> FAssetHandlers::DeleteAsset(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("path"), AssetPath))
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing 'path' parameter"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	bool bSuccess = UEditorAssetLibrary::DeleteAsset(AssetPath);
	Result->SetStringField(TEXT("path"), AssetPath);
	Result->SetBoolField(TEXT("success"), bSuccess);

	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FAssetHandlers::SaveAsset(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	FString AssetPath;
	if (Params->TryGetStringField(TEXT("path"), AssetPath) && !AssetPath.IsEmpty() && AssetPath != TEXT("all"))
	{
		bool bSuccess = UEditorAssetLibrary::SaveAsset(AssetPath);
		Result->SetStringField(TEXT("path"), AssetPath);
		Result->SetBoolField(TEXT("success"), bSuccess);
	}
	else
	{
		// Save all dirty assets
		UEditorAssetLibrary::SaveDirectory(TEXT("/Game"));
		Result->SetStringField(TEXT("message"), TEXT("All modified assets saved"));
		Result->SetBoolField(TEXT("success"), true);
	}

	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FAssetHandlers::ListTextures(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	FString Directory = TEXT("/Game/");
	Params->TryGetStringField(TEXT("directory"), Directory);
	int32 MaxResults = 50;
	Params->TryGetNumberField(TEXT("maxResults"), MaxResults);

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssetsByClass(FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("Texture2D")), AssetDataList, true);

	TArray<TSharedPtr<FJsonValue>> TexturesArray;
	for (const FAssetData& AssetData : AssetDataList)
	{
		if (TexturesArray.Num() >= MaxResults) break;
		FString AssetPath = AssetData.GetObjectPathString();
		if (!AssetPath.StartsWith(Directory)) continue;

		TSharedPtr<FJsonObject> TexObj = MakeShared<FJsonObject>();
		TexObj->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
		TexObj->SetStringField(TEXT("path"), AssetPath);
		TexturesArray.Add(MakeShared<FJsonValueObject>(TexObj));
	}

	Result->SetArrayField(TEXT("textures"), TexturesArray);
	Result->SetNumberField(TEXT("count"), TexturesArray.Num());
	Result->SetBoolField(TEXT("success"), true);
	return MakeShared<FJsonValueObject>(Result);
}
