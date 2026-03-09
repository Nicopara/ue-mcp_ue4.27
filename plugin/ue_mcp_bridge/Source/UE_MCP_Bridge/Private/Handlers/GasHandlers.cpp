#include "GasHandlers.h"
#include "HandlerRegistry.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Engine/Blueprint.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Factories/BlueprintFactory.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "EditorScriptingUtilities/Public/EditorAssetLibrary.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

void FGasHandlers::RegisterHandlers(FMCPHandlerRegistry& Registry)
{
	Registry.RegisterHandler(TEXT("create_gameplay_effect"), &CreateGameplayEffect);
	Registry.RegisterHandler(TEXT("get_gas_info"), &GetGasInfo);
	Registry.RegisterHandler(TEXT("create_gameplay_ability"), &CreateGameplayAbility);
	Registry.RegisterHandler(TEXT("create_attribute_set"), &CreateAttributeSet);
	Registry.RegisterHandler(TEXT("create_gameplay_cue"), &CreateGameplayCue);
}

TSharedPtr<FJsonValue> FGasHandlers::CreateGameplayEffect(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	FString Name;
	if (!Params->TryGetStringField(TEXT("name"), Name))
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing 'name' parameter"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	UE_LOG(LogTemp, Log, TEXT("[UE-MCP] CreateGameplayEffect called with name: %s"), *Name);

	FString PackagePath = TEXT("/Game/GAS/Effects");
	Params->TryGetStringField(TEXT("packagePath"), PackagePath);

	FString DurationPolicy = TEXT("Instant");
	Params->TryGetStringField(TEXT("durationPolicy"), DurationPolicy);

	// Find GameplayEffect class
	UClass* GameplayEffectClass = FindObject<UClass>(nullptr, TEXT("/Script/GameplayAbilities.GameplayEffect"));
	if (!GameplayEffectClass)
	{
		Result->SetStringField(TEXT("error"), TEXT("GameplayEffect class not found. Enable GameplayAbilities plugin."));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	// Create blueprint
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	IAssetTools& AssetTools = AssetToolsModule.Get();

	FString PackageName;
	FString AssetName;
	PackagePath.Split(TEXT("/"), &PackageName, &AssetName, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
	if (AssetName.IsEmpty())
	{
		AssetName = Name;
	}
	else
	{
		PackageName = PackagePath;
		AssetName = Name;
	}
	PackageName = PackageName.LeftChop(1); // Remove trailing /

	// Create asset path
	FString FullAssetPath = PackagePath + TEXT("/") + Name;

	// Delete existing asset if it exists
	UEditorAssetLibrary::DeleteAsset(FullAssetPath);

	UBlueprintFactory* BlueprintFactory = NewObject<UBlueprintFactory>();
	UBlueprint* NewBlueprint = Cast<UBlueprint>(AssetTools.CreateAsset(AssetName, PackageName, UBlueprint::StaticClass(), BlueprintFactory));
	if (!NewBlueprint)
	{
		Result->SetStringField(TEXT("error"), TEXT("Failed to create GameplayEffect Blueprint"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	NewBlueprint->ParentClass = GameplayEffectClass;
	FKismetEditorUtilities::CompileBlueprint(NewBlueprint);

	// Save asset
	UPackage* Package = NewBlueprint->GetOutermost();
	if (Package)
	{
		Package->MarkPackageDirty();
		FString PackageFileName = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Standalone;
		UPackage::SavePackage(Package, nullptr, *PackageFileName, SaveArgs);
	}

	FString AssetPath = NewBlueprint->GetPathName();
	Result->SetStringField(TEXT("path"), AssetPath);
	Result->SetStringField(TEXT("name"), Name);
	Result->SetStringField(TEXT("durationPolicy"), DurationPolicy);
	Result->SetBoolField(TEXT("success"), true);

	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FGasHandlers::GetGasInfo(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	FString BlueprintPath;
	if (!Params->TryGetStringField(TEXT("blueprintPath"), BlueprintPath))
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing 'blueprintPath' parameter"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
	if (!Blueprint || !Blueprint->GeneratedClass)
	{
		// Return success with empty info rather than crashing
		Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
		Result->SetBoolField(TEXT("hasGasComponents"), false);
		Result->SetStringField(TEXT("info"), TEXT("Blueprint not found or has no generated class"));
		Result->SetBoolField(TEXT("success"), true);
		return MakeShared<FJsonValueObject>(Result);
	}

	UObject* CDO = Blueprint->GeneratedClass->GetDefaultObject();
	if (!CDO)
	{
		Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
		Result->SetBoolField(TEXT("hasGasComponents"), false);
		Result->SetStringField(TEXT("info"), TEXT("No CDO available"));
		Result->SetBoolField(TEXT("success"), true);
		return MakeShared<FJsonValueObject>(Result);
	}

	Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
	Result->SetStringField(TEXT("className"), Blueprint->GeneratedClass->GetName());
	Result->SetStringField(TEXT("parentClass"), Blueprint->ParentClass ? Blueprint->ParentClass->GetName() : TEXT("None"));

	// Check for GAS-related components
	bool bHasGasComponents = false;
	TArray<TSharedPtr<FJsonValue>> ComponentArray;

	// Check if the class has an AbilitySystemComponent
	UClass* ASCClass = FindObject<UClass>(nullptr, TEXT("/Script/GameplayAbilities.AbilitySystemComponent"));
	if (ASCClass && CDO->IsA(AActor::StaticClass()))
	{
		AActor* ActorCDO = Cast<AActor>(CDO);
		if (ActorCDO)
		{
			TArray<UActorComponent*> Components;
			ActorCDO->GetComponents(Components);
			for (UActorComponent* Comp : Components)
			{
				if (Comp && Comp->IsA(ASCClass))
				{
					bHasGasComponents = true;
					TSharedPtr<FJsonObject> CompObj = MakeShared<FJsonObject>();
					CompObj->SetStringField(TEXT("name"), Comp->GetName());
					CompObj->SetStringField(TEXT("class"), Comp->GetClass()->GetName());
					ComponentArray.Add(MakeShared<FJsonValueObject>(CompObj));
				}
			}
		}
	}

	Result->SetBoolField(TEXT("hasGasComponents"), bHasGasComponents);
	Result->SetArrayField(TEXT("gasComponents"), ComponentArray);

	// Check if this is a GameplayEffect subclass
	UClass* GEClass = FindObject<UClass>(nullptr, TEXT("/Script/GameplayAbilities.GameplayEffect"));
	Result->SetBoolField(TEXT("isGameplayEffect"), GEClass && Blueprint->GeneratedClass->IsChildOf(GEClass));

	// Check if this is a GameplayAbility subclass
	UClass* GAClass = FindObject<UClass>(nullptr, TEXT("/Script/GameplayAbilities.GameplayAbility"));
	Result->SetBoolField(TEXT("isGameplayAbility"), GAClass && Blueprint->GeneratedClass->IsChildOf(GAClass));

	// Check if this is an AttributeSet subclass
	UClass* AttrSetClass = FindObject<UClass>(nullptr, TEXT("/Script/GameplayAbilities.AttributeSet"));
	Result->SetBoolField(TEXT("isAttributeSet"), AttrSetClass && Blueprint->GeneratedClass->IsChildOf(AttrSetClass));

	Result->SetBoolField(TEXT("success"), true);
	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FGasHandlers::CreateGameplayAbility(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	FString Name;
	if (!Params->TryGetStringField(TEXT("name"), Name))
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing 'name' parameter"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	FString PackagePath = TEXT("/Game/GAS/Abilities");
	Params->TryGetStringField(TEXT("packagePath"), PackagePath);

	UClass* GAClass = FindObject<UClass>(nullptr, TEXT("/Script/GameplayAbilities.GameplayAbility"));
	if (!GAClass)
	{
		Result->SetStringField(TEXT("error"), TEXT("GameplayAbility class not found. Enable GameplayAbilities plugin."));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	IAssetTools& AssetTools = AssetToolsModule.Get();

	// Delete existing asset if it exists
	FString FullAssetPath = PackagePath + TEXT("/") + Name;
	UEditorAssetLibrary::DeleteAsset(FullAssetPath);

	UBlueprintFactory* BlueprintFactory = NewObject<UBlueprintFactory>();
	BlueprintFactory->ParentClass = GAClass;

	UBlueprint* NewBlueprint = Cast<UBlueprint>(AssetTools.CreateAsset(Name, PackagePath, UBlueprint::StaticClass(), BlueprintFactory));
	if (!NewBlueprint)
	{
		Result->SetStringField(TEXT("error"), TEXT("Failed to create GameplayAbility Blueprint"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	NewBlueprint->ParentClass = GAClass;
	FKismetEditorUtilities::CompileBlueprint(NewBlueprint);

	UPackage* Package = NewBlueprint->GetOutermost();
	if (Package)
	{
		Package->MarkPackageDirty();
		FString PackageFileName = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Standalone;
		UPackage::SavePackage(Package, nullptr, *PackageFileName, SaveArgs);
	}

	Result->SetStringField(TEXT("path"), NewBlueprint->GetPathName());
	Result->SetStringField(TEXT("name"), Name);
	Result->SetBoolField(TEXT("success"), true);
	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FGasHandlers::CreateAttributeSet(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	FString Name;
	if (!Params->TryGetStringField(TEXT("name"), Name))
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing 'name' parameter"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	FString PackagePath = TEXT("/Game/GAS/Attributes");
	Params->TryGetStringField(TEXT("packagePath"), PackagePath);

	UClass* AttrSetClass = FindObject<UClass>(nullptr, TEXT("/Script/GameplayAbilities.AttributeSet"));
	if (!AttrSetClass)
	{
		Result->SetStringField(TEXT("error"), TEXT("AttributeSet class not found. Enable GameplayAbilities plugin."));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	IAssetTools& AssetTools = AssetToolsModule.Get();

	FString FullAssetPath = PackagePath + TEXT("/") + Name;
	UEditorAssetLibrary::DeleteAsset(FullAssetPath);

	UBlueprintFactory* BlueprintFactory = NewObject<UBlueprintFactory>();
	BlueprintFactory->ParentClass = AttrSetClass;

	UBlueprint* NewBlueprint = Cast<UBlueprint>(AssetTools.CreateAsset(Name, PackagePath, UBlueprint::StaticClass(), BlueprintFactory));
	if (!NewBlueprint)
	{
		Result->SetStringField(TEXT("error"), TEXT("Failed to create AttributeSet Blueprint"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	NewBlueprint->ParentClass = AttrSetClass;
	FKismetEditorUtilities::CompileBlueprint(NewBlueprint);

	UPackage* Package = NewBlueprint->GetOutermost();
	if (Package)
	{
		Package->MarkPackageDirty();
		FString PackageFileName = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Standalone;
		UPackage::SavePackage(Package, nullptr, *PackageFileName, SaveArgs);
	}

	Result->SetStringField(TEXT("path"), NewBlueprint->GetPathName());
	Result->SetStringField(TEXT("name"), Name);
	Result->SetBoolField(TEXT("success"), true);
	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FGasHandlers::CreateGameplayCue(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	FString Name;
	if (!Params->TryGetStringField(TEXT("name"), Name))
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing 'name' parameter"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	FString PackagePath = TEXT("/Game/GAS/Cues");
	Params->TryGetStringField(TEXT("packagePath"), PackagePath);

	FString CueType = TEXT("Static");
	Params->TryGetStringField(TEXT("cueType"), CueType);

	// Determine parent class based on cue type
	FString ParentClassPath;
	if (CueType == TEXT("Actor"))
	{
		ParentClassPath = TEXT("/Script/GameplayAbilities.GameplayCueNotify_Actor");
	}
	else
	{
		ParentClassPath = TEXT("/Script/GameplayAbilities.GameplayCueNotify_Static");
	}

	UClass* ParentClass = FindObject<UClass>(nullptr, *ParentClassPath);
	if (!ParentClass)
	{
		Result->SetStringField(TEXT("error"), FString::Printf(TEXT("GameplayCue parent class not found: %s. Enable GameplayAbilities plugin."), *ParentClassPath));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	IAssetTools& AssetTools = AssetToolsModule.Get();

	FString FullAssetPath = PackagePath + TEXT("/") + Name;
	UEditorAssetLibrary::DeleteAsset(FullAssetPath);

	UBlueprintFactory* BlueprintFactory = NewObject<UBlueprintFactory>();
	BlueprintFactory->ParentClass = ParentClass;

	UBlueprint* NewBlueprint = Cast<UBlueprint>(AssetTools.CreateAsset(Name, PackagePath, UBlueprint::StaticClass(), BlueprintFactory));
	if (!NewBlueprint)
	{
		Result->SetStringField(TEXT("error"), TEXT("Failed to create GameplayCue Blueprint"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	NewBlueprint->ParentClass = ParentClass;
	FKismetEditorUtilities::CompileBlueprint(NewBlueprint);

	UPackage* Package = NewBlueprint->GetOutermost();
	if (Package)
	{
		Package->MarkPackageDirty();
		FString PackageFileName = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Standalone;
		UPackage::SavePackage(Package, nullptr, *PackageFileName, SaveArgs);
	}

	Result->SetStringField(TEXT("path"), NewBlueprint->GetPathName());
	Result->SetStringField(TEXT("name"), Name);
	Result->SetStringField(TEXT("cueType"), CueType);
	Result->SetBoolField(TEXT("success"), true);
	return MakeShared<FJsonValueObject>(Result);
}
