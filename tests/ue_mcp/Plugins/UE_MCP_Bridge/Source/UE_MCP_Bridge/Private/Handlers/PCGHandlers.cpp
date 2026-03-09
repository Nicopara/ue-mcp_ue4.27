#include "PCGHandlers.h"
#include "HandlerRegistry.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "EditorScriptingUtilities/Public/EditorAssetLibrary.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/TopLevelAssetPath.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "PCGGraph.h"
#include "PCGComponent.h"
#include "PCGNode.h"

void FPCGHandlers::RegisterHandlers(FMCPHandlerRegistry& Registry)
{
	Registry.RegisterHandler(TEXT("list_pcg_graphs"), &ListPCGGraphs);
	Registry.RegisterHandler(TEXT("get_pcg_components"), &GetPCGComponents);
	Registry.RegisterHandler(TEXT("create_pcg_graph"), &CreatePCGGraph);
	Registry.RegisterHandler(TEXT("read_pcg_graph"), &ReadPCGGraph);
}

TSharedPtr<FJsonValue> FPCGHandlers::ListPCGGraphs(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	TArray<FAssetData> Assets;
	AR.GetAssetsByClass(FTopLevelAssetPath(TEXT("/Script/PCG"), TEXT("PCGGraph")), Assets, true);

	TArray<TSharedPtr<FJsonValue>> AssetArray;
	for (const FAssetData& Asset : Assets)
	{
		TSharedPtr<FJsonObject> AssetObj = MakeShared<FJsonObject>();
		AssetObj->SetStringField(TEXT("name"), Asset.AssetName.ToString());
		AssetObj->SetStringField(TEXT("path"), Asset.GetObjectPathString());
		AssetArray.Add(MakeShared<FJsonValueObject>(AssetObj));
	}
	Result->SetArrayField(TEXT("graphs"), AssetArray);
	Result->SetNumberField(TEXT("count"), AssetArray.Num());
	Result->SetBoolField(TEXT("success"), true);
	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FPCGHandlers::GetPCGComponents(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		Result->SetStringField(TEXT("error"), TEXT("No editor world available"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	TArray<TSharedPtr<FJsonValue>> CompArray;
	for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
	{
		AActor* Actor = *ActorIt;
		if (!Actor) continue;

		TArray<UPCGComponent*> PCGComps;
		Actor->GetComponents<UPCGComponent>(PCGComps);
		for (UPCGComponent* PCGComp : PCGComps)
		{
			if (!PCGComp) continue;
			TSharedPtr<FJsonObject> CompObj = MakeShared<FJsonObject>();
			CompObj->SetStringField(TEXT("actorName"), Actor->GetActorLabel());
			CompObj->SetStringField(TEXT("actorClass"), Actor->GetClass()->GetName());
			CompObj->SetStringField(TEXT("componentName"), PCGComp->GetName());
			if (PCGComp->GetGraph())
			{
				CompObj->SetStringField(TEXT("graphName"), PCGComp->GetGraph()->GetName());
			}
			CompArray.Add(MakeShared<FJsonValueObject>(CompObj));
		}
	}

	Result->SetArrayField(TEXT("components"), CompArray);
	Result->SetNumberField(TEXT("count"), CompArray.Num());
	Result->SetBoolField(TEXT("success"), true);
	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FPCGHandlers::CreatePCGGraph(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	FString Name;
	if (!Params->TryGetStringField(TEXT("name"), Name))
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing 'name' parameter"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	FString PackagePath = TEXT("/Game/PCG");
	Params->TryGetStringField(TEXT("packagePath"), PackagePath);

	FString FullAssetPath = PackagePath + TEXT("/") + Name;
	UEditorAssetLibrary::DeleteAsset(FullAssetPath);

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	IAssetTools& AssetTools = AssetToolsModule.Get();

	UObject* NewAsset = AssetTools.CreateAsset(Name, PackagePath, UPCGGraph::StaticClass(), nullptr);
	if (!NewAsset)
	{
		Result->SetStringField(TEXT("error"), TEXT("Failed to create PCGGraph"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	UEditorAssetLibrary::SaveAsset(NewAsset->GetPathName());

	Result->SetStringField(TEXT("path"), NewAsset->GetPathName());
	Result->SetStringField(TEXT("name"), Name);
	Result->SetBoolField(TEXT("success"), true);
	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FPCGHandlers::ReadPCGGraph(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("assetPath"), AssetPath))
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing 'assetPath' parameter"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	UPCGGraph* Graph = LoadObject<UPCGGraph>(nullptr, *AssetPath);
	if (!Graph)
	{
		Result->SetStringField(TEXT("error"), FString::Printf(TEXT("PCGGraph not found: %s"), *AssetPath));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	Result->SetStringField(TEXT("name"), Graph->GetName());
	Result->SetStringField(TEXT("path"), AssetPath);

	const TArray<UPCGNode*>& Nodes = Graph->GetNodes();
	TArray<TSharedPtr<FJsonValue>> NodeArray;
	for (const UPCGNode* Node : Nodes)
	{
		if (!Node) continue;
		TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
		NodeObj->SetStringField(TEXT("name"), Node->GetName());
		NodeObj->SetStringField(TEXT("title"), Node->GetNodeTitle(EPCGNodeTitleType::ListView).ToString());
		NodeArray.Add(MakeShared<FJsonValueObject>(NodeObj));
	}

	Result->SetArrayField(TEXT("nodes"), NodeArray);
	Result->SetNumberField(TEXT("nodeCount"), NodeArray.Num());
	Result->SetBoolField(TEXT("success"), true);
	return MakeShared<FJsonValueObject>(Result);
}
