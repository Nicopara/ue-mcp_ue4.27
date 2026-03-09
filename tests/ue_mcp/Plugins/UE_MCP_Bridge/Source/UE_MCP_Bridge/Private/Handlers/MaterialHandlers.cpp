#include "MaterialHandlers.h"
#include "HandlerRegistry.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "Factories/MaterialFactoryNew.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "EditorScriptingUtilities/Public/EditorAssetLibrary.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

void FMaterialHandlers::RegisterHandlers(FMCPHandlerRegistry& Registry)
{
	Registry.RegisterHandler(TEXT("list_expression_types"), &ListExpressionTypes);
	Registry.RegisterHandler(TEXT("create_material"), &CreateMaterial);
	Registry.RegisterHandler(TEXT("read_material"), &ReadMaterial);
	Registry.RegisterHandler(TEXT("set_material_shading_model"), &SetMaterialShadingModel);
	Registry.RegisterHandler(TEXT("set_material_base_color"), &SetMaterialBaseColor);
	Registry.RegisterHandler(TEXT("add_material_expression"), &AddMaterialExpression);
	Registry.RegisterHandler(TEXT("list_material_expressions"), &ListMaterialExpressions);
	Registry.RegisterHandler(TEXT("list_material_parameters"), &ListMaterialParameters);
	Registry.RegisterHandler(TEXT("recompile_material"), &RecompileMaterial);
	Registry.RegisterHandler(TEXT("create_material_instance"), &CreateMaterialInstance);
}

UMaterial* FMaterialHandlers::LoadMaterialFromPath(const FString& AssetPath)
{
	UObject* LoadedObject = StaticLoadObject(UMaterial::StaticClass(), nullptr, *AssetPath);
	if (!LoadedObject)
	{
		// Try with explicit class prefix
		LoadedObject = StaticLoadObject(UMaterial::StaticClass(), nullptr, *(TEXT("Material'") + AssetPath + TEXT("'")));
	}
	return Cast<UMaterial>(LoadedObject);
}

EMaterialShadingModel FMaterialHandlers::ParseShadingModel(const FString& ShadingModelStr)
{
	FString Lower = ShadingModelStr.ToLower();
	if (Lower == TEXT("unlit"))                return MSM_Unlit;
	if (Lower == TEXT("defaultlit"))           return MSM_DefaultLit;
	if (Lower == TEXT("subsurface"))           return MSM_Subsurface;
	if (Lower == TEXT("subsurfaceprofile"))    return MSM_SubsurfaceProfile;
	if (Lower == TEXT("preintegratedskin"))    return MSM_PreintegratedSkin;
	if (Lower == TEXT("clearcoa") || Lower == TEXT("clearcoat")) return MSM_ClearCoat;
	if (Lower == TEXT("cloth"))                return MSM_Cloth;
	if (Lower == TEXT("eye"))                  return MSM_Eye;
	if (Lower == TEXT("twosidedfoliage"))      return MSM_TwoSidedFoliage;
	return MSM_DefaultLit;
}

FString FMaterialHandlers::ShadingModelToString(EMaterialShadingModel ShadingModel)
{
	switch (ShadingModel)
	{
	case MSM_Unlit:              return TEXT("Unlit");
	case MSM_DefaultLit:         return TEXT("DefaultLit");
	case MSM_Subsurface:         return TEXT("Subsurface");
	case MSM_SubsurfaceProfile:  return TEXT("SubsurfaceProfile");
	case MSM_PreintegratedSkin:  return TEXT("PreintegratedSkin");
	case MSM_ClearCoat:          return TEXT("ClearCoat");
	case MSM_Cloth:              return TEXT("Cloth");
	case MSM_Eye:                return TEXT("Eye");
	case MSM_TwoSidedFoliage:   return TEXT("TwoSidedFoliage");
	default:                     return TEXT("Unknown");
	}
}

TSharedPtr<FJsonValue> FMaterialHandlers::ListExpressionTypes(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> TypesArray;

	// Common material expression types
	TArray<FString> ExpressionTypes = {
		TEXT("MaterialExpressionConstant"),
		TEXT("MaterialExpressionConstant2Vector"),
		TEXT("MaterialExpressionConstant3Vector"),
		TEXT("MaterialExpressionConstant4Vector"),
		TEXT("MaterialExpressionTextureSample"),
		TEXT("MaterialExpressionTextureCoordinate"),
		TEXT("MaterialExpressionScalarParameter"),
		TEXT("MaterialExpressionVectorParameter"),
		TEXT("MaterialExpressionTextureObjectParameter"),
		TEXT("MaterialExpressionStaticSwitchParameter"),
		TEXT("MaterialExpressionAdd"),
		TEXT("MaterialExpressionMultiply"),
		TEXT("MaterialExpressionSubtract"),
		TEXT("MaterialExpressionDivide"),
		TEXT("MaterialExpressionLinearInterpolate"),
		TEXT("MaterialExpressionPower"),
		TEXT("MaterialExpressionClamp"),
		TEXT("MaterialExpressionAppendVector"),
		TEXT("MaterialExpressionComponentMask"),
		TEXT("MaterialExpressionDotProduct"),
		TEXT("MaterialExpressionCrossProduct"),
		TEXT("MaterialExpressionNormalize"),
		TEXT("MaterialExpressionOneMinus"),
		TEXT("MaterialExpressionAbs"),
		TEXT("MaterialExpressionTime"),
		TEXT("MaterialExpressionWorldPosition"),
		TEXT("MaterialExpressionVertexNormalWS"),
		TEXT("MaterialExpressionCameraPositionWS"),
		TEXT("MaterialExpressionFresnel"),
		TEXT("MaterialExpressionPanner"),
		TEXT("MaterialExpressionRotator"),
		TEXT("MaterialExpressionDesaturation"),
		TEXT("MaterialExpressionNoise"),
		TEXT("MaterialExpressionParticleColor"),
		TEXT("MaterialExpressionObjectPositionWS"),
		TEXT("MaterialExpressionActorPositionWS")
	};

	for (const FString& TypeName : ExpressionTypes)
	{
		TypesArray.Add(MakeShared<FJsonValueString>(TypeName));
	}

	Result->SetArrayField(TEXT("expressionTypes"), TypesArray);
	Result->SetNumberField(TEXT("count"), ExpressionTypes.Num());
	Result->SetBoolField(TEXT("success"), true);

	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FMaterialHandlers::CreateMaterial(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	FString Name;
	if (!Params->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty())
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing or empty 'name' parameter"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	FString PackagePath = TEXT("/Game/Materials");
	Params->TryGetStringField(TEXT("packagePath"), PackagePath);

	UE_LOG(LogTemp, Log, TEXT("[UE-MCP] CreateMaterial: name=%s packagePath=%s"), *Name, *PackagePath);

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	IAssetTools& AssetTools = AssetToolsModule.Get();

	UMaterialFactoryNew* MaterialFactory = NewObject<UMaterialFactoryNew>();
	UObject* NewAsset = AssetTools.CreateAsset(Name, PackagePath, UMaterial::StaticClass(), MaterialFactory);

	if (!NewAsset)
	{
		Result->SetStringField(TEXT("error"), TEXT("Failed to create material asset"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	UMaterial* NewMaterial = Cast<UMaterial>(NewAsset);
	if (!NewMaterial)
	{
		Result->SetStringField(TEXT("error"), TEXT("Created asset is not a material"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	// Save the package
	UPackage* Package = NewMaterial->GetOutermost();
	if (Package)
	{
		Package->MarkPackageDirty();
		FString PackageFileName = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Standalone;
		UPackage::SavePackage(Package, nullptr, *PackageFileName, SaveArgs);
	}

	FString AssetPath = NewMaterial->GetPathName();
	Result->SetStringField(TEXT("path"), AssetPath);
	Result->SetStringField(TEXT("name"), Name);
	Result->SetStringField(TEXT("packagePath"), PackagePath);
	Result->SetBoolField(TEXT("success"), true);

	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FMaterialHandlers::ReadMaterial(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty())
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing or empty 'assetPath' parameter"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	UMaterial* Material = LoadMaterialFromPath(AssetPath);
	if (!Material)
	{
		Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to load material at '%s'"), *AssetPath));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	Result->SetStringField(TEXT("name"), Material->GetName());
	Result->SetStringField(TEXT("path"), Material->GetPathName());
	Result->SetStringField(TEXT("shadingModel"), ShadingModelToString(Material->GetShadingModels().GetFirstShadingModel()));
	Result->SetStringField(TEXT("blendMode"), StaticEnum<EBlendMode>()->GetNameStringByValue((int64)Material->BlendMode));
	Result->SetBoolField(TEXT("twoSided"), Material->IsTwoSided());

	// Expression count
	int32 ExpressionCount = Material->GetExpressions().Num();
	Result->SetNumberField(TEXT("expressionCount"), ExpressionCount);

	Result->SetBoolField(TEXT("success"), true);

	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FMaterialHandlers::SetMaterialShadingModel(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty())
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing or empty 'assetPath' parameter"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	FString ShadingModelStr;
	if (!Params->TryGetStringField(TEXT("shadingModel"), ShadingModelStr) || ShadingModelStr.IsEmpty())
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing or empty 'shadingModel' parameter"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	UMaterial* Material = LoadMaterialFromPath(AssetPath);
	if (!Material)
	{
		Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to load material at '%s'"), *AssetPath));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	EMaterialShadingModel NewShadingModel = ParseShadingModel(ShadingModelStr);

	Material->PreEditChange(nullptr);
	Material->SetShadingModel(NewShadingModel);
	Material->PostEditChange();

	// Mark dirty and save
	Material->MarkPackageDirty();

	Result->SetStringField(TEXT("path"), Material->GetPathName());
	Result->SetStringField(TEXT("shadingModel"), ShadingModelToString(NewShadingModel));
	Result->SetBoolField(TEXT("success"), true);

	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FMaterialHandlers::SetMaterialBaseColor(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty())
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing or empty 'assetPath' parameter"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	const TSharedPtr<FJsonObject>* ColorObj = nullptr;
	if (!Params->TryGetObjectField(TEXT("color"), ColorObj))
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing 'color' parameter (object with r,g,b,a)"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	double R = 1.0, G = 1.0, B = 1.0, A = 1.0;
	(*ColorObj)->TryGetNumberField(TEXT("r"), R);
	(*ColorObj)->TryGetNumberField(TEXT("g"), G);
	(*ColorObj)->TryGetNumberField(TEXT("b"), B);
	(*ColorObj)->TryGetNumberField(TEXT("a"), A);

	UMaterial* Material = LoadMaterialFromPath(AssetPath);
	if (!Material)
	{
		Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to load material at '%s'"), *AssetPath));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	Material->PreEditChange(nullptr);

	// Create a Constant3Vector expression for the base color
	UMaterialExpressionConstant3Vector* ColorExpression = NewObject<UMaterialExpressionConstant3Vector>(Material);
	ColorExpression->Constant = FLinearColor(R, G, B, A);

	// Add expression to material
	Material->GetExpressionCollection().AddExpression(ColorExpression);

	// Connect to base color input
	Material->GetEditorOnlyData()->BaseColor.Connect(0, ColorExpression);

	Material->PostEditChange();
	Material->MarkPackageDirty();

	TSharedPtr<FJsonObject> ColorResult = MakeShared<FJsonObject>();
	ColorResult->SetNumberField(TEXT("r"), R);
	ColorResult->SetNumberField(TEXT("g"), G);
	ColorResult->SetNumberField(TEXT("b"), B);
	ColorResult->SetNumberField(TEXT("a"), A);
	Result->SetObjectField(TEXT("color"), ColorResult);
	Result->SetStringField(TEXT("path"), Material->GetPathName());
	Result->SetBoolField(TEXT("success"), true);

	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FMaterialHandlers::AddMaterialExpression(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	FString MaterialPath;
	if (!Params->TryGetStringField(TEXT("materialPath"), MaterialPath) || MaterialPath.IsEmpty())
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing or empty 'materialPath' parameter"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	FString ExpressionType;
	if (!Params->TryGetStringField(TEXT("expressionType"), ExpressionType) || ExpressionType.IsEmpty())
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing or empty 'expressionType' parameter"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	UMaterial* Material = LoadMaterialFromPath(MaterialPath);
	if (!Material)
	{
		Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to load material at '%s'"), *MaterialPath));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	// Ensure expression type has the U prefix for class lookup
	FString ClassName = ExpressionType;
	if (!ClassName.StartsWith(TEXT("U")))
	{
		ClassName = TEXT("U") + ClassName;
	}

	// Find the expression class
	UClass* ExpressionClass = FindFirstObject<UClass>(*ClassName, EFindFirstObjectOptions::ExactClass);
	if (!ExpressionClass)
	{
		// Try with /Script/Engine prefix
		FString FullPath = FString::Printf(TEXT("/Script/Engine.%s"), *ExpressionType);
		ExpressionClass = FindObject<UClass>(nullptr, *FullPath);
	}

	if (!ExpressionClass || !ExpressionClass->IsChildOf(UMaterialExpression::StaticClass()))
	{
		Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown expression type: '%s'"), *ExpressionType));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	Material->PreEditChange(nullptr);

	UMaterialExpression* NewExpression = NewObject<UMaterialExpression>(Material, ExpressionClass);
	Material->GetExpressionCollection().AddExpression(NewExpression);

	Material->PostEditChange();
	Material->MarkPackageDirty();

	Result->SetStringField(TEXT("expressionType"), ExpressionType);
	Result->SetStringField(TEXT("expressionClass"), NewExpression->GetClass()->GetName());
	Result->SetStringField(TEXT("materialPath"), Material->GetPathName());
	Result->SetNumberField(TEXT("expressionCount"), Material->GetExpressions().Num());
	Result->SetBoolField(TEXT("success"), true);

	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FMaterialHandlers::ListMaterialExpressions(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	FString MaterialPath;
	if (!Params->TryGetStringField(TEXT("materialPath"), MaterialPath) || MaterialPath.IsEmpty())
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing or empty 'materialPath' parameter"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	UMaterial* Material = LoadMaterialFromPath(MaterialPath);
	if (!Material)
	{
		Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to load material at '%s'"), *MaterialPath));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	TArray<TSharedPtr<FJsonValue>> ExpressionsArray;
	for (UMaterialExpression* Expression : Material->GetExpressions())
	{
		if (!Expression) continue;

		TSharedPtr<FJsonObject> ExprObj = MakeShared<FJsonObject>();
		ExprObj->SetStringField(TEXT("class"), Expression->GetClass()->GetName());
		ExprObj->SetStringField(TEXT("description"), Expression->GetDescription());
		ExprObj->SetNumberField(TEXT("positionX"), Expression->MaterialExpressionEditorX);
		ExprObj->SetNumberField(TEXT("positionY"), Expression->MaterialExpressionEditorY);

		ExpressionsArray.Add(MakeShared<FJsonValueObject>(ExprObj));
	}

	Result->SetArrayField(TEXT("expressions"), ExpressionsArray);
	Result->SetNumberField(TEXT("count"), ExpressionsArray.Num());
	Result->SetStringField(TEXT("materialPath"), Material->GetPathName());
	Result->SetBoolField(TEXT("success"), true);

	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FMaterialHandlers::ListMaterialParameters(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty())
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing or empty 'assetPath' parameter"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	UMaterial* Material = LoadMaterialFromPath(AssetPath);
	if (!Material)
	{
		Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to load material at '%s'"), *AssetPath));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	TArray<TSharedPtr<FJsonValue>> ScalarParams;
	TArray<TSharedPtr<FJsonValue>> VectorParams;
	TArray<TSharedPtr<FJsonValue>> TextureParams;

	for (UMaterialExpression* Expression : Material->GetExpressions())
	{
		if (!Expression) continue;

		if (UMaterialExpressionScalarParameter* ScalarParam = Cast<UMaterialExpressionScalarParameter>(Expression))
		{
			TSharedPtr<FJsonObject> ParamObj = MakeShared<FJsonObject>();
			ParamObj->SetStringField(TEXT("name"), ScalarParam->ParameterName.ToString());
			ParamObj->SetNumberField(TEXT("defaultValue"), ScalarParam->DefaultValue);
			ScalarParams.Add(MakeShared<FJsonValueObject>(ParamObj));
		}
		else if (UMaterialExpressionVectorParameter* VectorParam = Cast<UMaterialExpressionVectorParameter>(Expression))
		{
			TSharedPtr<FJsonObject> ParamObj = MakeShared<FJsonObject>();
			ParamObj->SetStringField(TEXT("name"), VectorParam->ParameterName.ToString());

			TSharedPtr<FJsonObject> DefaultColor = MakeShared<FJsonObject>();
			DefaultColor->SetNumberField(TEXT("r"), VectorParam->DefaultValue.R);
			DefaultColor->SetNumberField(TEXT("g"), VectorParam->DefaultValue.G);
			DefaultColor->SetNumberField(TEXT("b"), VectorParam->DefaultValue.B);
			DefaultColor->SetNumberField(TEXT("a"), VectorParam->DefaultValue.A);
			ParamObj->SetObjectField(TEXT("defaultValue"), DefaultColor);

			VectorParams.Add(MakeShared<FJsonValueObject>(ParamObj));
		}
		else if (UMaterialExpressionTextureSample* TextureParam = Cast<UMaterialExpressionTextureSample>(Expression))
		{
			TSharedPtr<FJsonObject> ParamObj = MakeShared<FJsonObject>();
			ParamObj->SetStringField(TEXT("class"), TEXT("TextureSample"));
			if (TextureParam->Texture)
			{
				ParamObj->SetStringField(TEXT("texture"), TextureParam->Texture->GetPathName());
			}
			TextureParams.Add(MakeShared<FJsonValueObject>(ParamObj));
		}
	}

	Result->SetArrayField(TEXT("scalarParameters"), ScalarParams);
	Result->SetArrayField(TEXT("vectorParameters"), VectorParams);
	Result->SetArrayField(TEXT("textureParameters"), TextureParams);
	Result->SetStringField(TEXT("path"), Material->GetPathName());
	Result->SetBoolField(TEXT("success"), true);

	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FMaterialHandlers::RecompileMaterial(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	FString MaterialPath;
	if (!Params->TryGetStringField(TEXT("materialPath"), MaterialPath) || MaterialPath.IsEmpty())
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing or empty 'materialPath' parameter"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	UMaterial* Material = LoadMaterialFromPath(MaterialPath);
	if (!Material)
	{
		Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to load material at '%s'"), *MaterialPath));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	UE_LOG(LogTemp, Log, TEXT("[UE-MCP] Recompiling material: %s"), *MaterialPath);

	Material->PreEditChange(nullptr);
	Material->PostEditChange();
	Material->MarkPackageDirty();

	Result->SetStringField(TEXT("path"), Material->GetPathName());
	Result->SetBoolField(TEXT("success"), true);

	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FMaterialHandlers::CreateMaterialInstance(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	FString ParentPath;
	if (!Params->TryGetStringField(TEXT("parentPath"), ParentPath) || ParentPath.IsEmpty())
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing or empty 'parentPath' parameter"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	FString Name;
	if (!Params->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty())
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing or empty 'name' parameter"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	FString PackagePath = TEXT("/Game/Materials");
	Params->TryGetStringField(TEXT("packagePath"), PackagePath);

	// Load the parent material
	UMaterialInterface* ParentMaterial = Cast<UMaterialInterface>(
		StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *ParentPath));
	if (!ParentMaterial)
	{
		// Try with class prefix
		ParentMaterial = Cast<UMaterialInterface>(
			StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *(TEXT("Material'") + ParentPath + TEXT("'"))));
	}
	if (!ParentMaterial)
	{
		Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to load parent material at '%s'"), *ParentPath));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	UE_LOG(LogTemp, Log, TEXT("[UE-MCP] CreateMaterialInstance: name=%s parent=%s packagePath=%s"), *Name, *ParentPath, *PackagePath);

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	IAssetTools& AssetTools = AssetToolsModule.Get();

	UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
	Factory->InitialParent = ParentMaterial;

	UObject* NewAsset = AssetTools.CreateAsset(Name, PackagePath, UMaterialInstanceConstant::StaticClass(), Factory);
	if (!NewAsset)
	{
		Result->SetStringField(TEXT("error"), TEXT("Failed to create material instance asset"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	UMaterialInstanceConstant* MaterialInstance = Cast<UMaterialInstanceConstant>(NewAsset);
	if (!MaterialInstance)
	{
		Result->SetStringField(TEXT("error"), TEXT("Created asset is not a material instance"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	// Save the package
	UPackage* Package = MaterialInstance->GetOutermost();
	if (Package)
	{
		Package->MarkPackageDirty();
		FString PackageFileName = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Standalone;
		UPackage::SavePackage(Package, nullptr, *PackageFileName, SaveArgs);
	}

	Result->SetStringField(TEXT("path"), MaterialInstance->GetPathName());
	Result->SetStringField(TEXT("name"), Name);
	Result->SetStringField(TEXT("parentPath"), ParentMaterial->GetPathName());
	Result->SetStringField(TEXT("packagePath"), PackagePath);
	Result->SetBoolField(TEXT("success"), true);

	return MakeShared<FJsonValueObject>(Result);
}
