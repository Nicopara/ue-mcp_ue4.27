#include "LandscapeHandlers.h"
#include "HandlerRegistry.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Landscape/Classes/Landscape.h"
#include "Landscape/Classes/LandscapeProxy.h"
#include "Landscape/Classes/LandscapeInfo.h"
#include "Landscape/Classes/LandscapeComponent.h"
#include "LandscapeSplineActor.h"
#include "LandscapeSplinesComponent.h"
#include "LandscapeSplineControlPoint.h"
#include "LandscapeSplineSegment.h"

void FLandscapeHandlers::RegisterHandlers(FMCPHandlerRegistry& Registry)
{
	Registry.RegisterHandler(TEXT("get_landscape_info"), &GetLandscapeInfo);
	Registry.RegisterHandler(TEXT("list_landscape_layers"), &ListLandscapeLayers);
	Registry.RegisterHandler(TEXT("sample_landscape"), &SampleLandscape);
	Registry.RegisterHandler(TEXT("list_landscape_splines"), &ListLandscapeSplines);
	Registry.RegisterHandler(TEXT("get_landscape_component"), &GetLandscapeComponent);
}

TSharedPtr<FJsonValue> FLandscapeHandlers::GetLandscapeInfo(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		Result->SetStringField(TEXT("error"), TEXT("No editor world available"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	// Find landscape proxies in the world
	TArray<TSharedPtr<FJsonValue>> LandscapeArray;
	for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
	{
		ALandscapeProxy* Landscape = *It;
		if (!Landscape) continue;

		TSharedPtr<FJsonObject> LandscapeObj = MakeShared<FJsonObject>();
		LandscapeObj->SetStringField(TEXT("name"), Landscape->GetName());
		LandscapeObj->SetStringField(TEXT("class"), Landscape->GetClass()->GetName());

		// Get component count
		TArray<ULandscapeComponent*> LandscapeComponents;
		Landscape->GetComponents<ULandscapeComponent>(LandscapeComponents);
		LandscapeObj->SetNumberField(TEXT("componentCount"), LandscapeComponents.Num());

		// Get bounds
		FBox Bounds = Landscape->GetComponentsBoundingBox();
		if (Bounds.IsValid)
		{
			TSharedPtr<FJsonObject> BoundsObj = MakeShared<FJsonObject>();
			BoundsObj->SetNumberField(TEXT("minX"), Bounds.Min.X);
			BoundsObj->SetNumberField(TEXT("minY"), Bounds.Min.Y);
			BoundsObj->SetNumberField(TEXT("minZ"), Bounds.Min.Z);
			BoundsObj->SetNumberField(TEXT("maxX"), Bounds.Max.X);
			BoundsObj->SetNumberField(TEXT("maxY"), Bounds.Max.Y);
			BoundsObj->SetNumberField(TEXT("maxZ"), Bounds.Max.Z);

			FVector Size = Bounds.GetSize();
			BoundsObj->SetNumberField(TEXT("sizeX"), Size.X);
			BoundsObj->SetNumberField(TEXT("sizeY"), Size.Y);
			BoundsObj->SetNumberField(TEXT("sizeZ"), Size.Z);
			LandscapeObj->SetObjectField(TEXT("bounds"), BoundsObj);
		}

		// Get location
		FVector Location = Landscape->GetActorLocation();
		LandscapeObj->SetNumberField(TEXT("locationX"), Location.X);
		LandscapeObj->SetNumberField(TEXT("locationY"), Location.Y);
		LandscapeObj->SetNumberField(TEXT("locationZ"), Location.Z);

		LandscapeArray.Add(MakeShared<FJsonValueObject>(LandscapeObj));
	}

	if (LandscapeArray.Num() == 0)
	{
		Result->SetStringField(TEXT("landscape"), TEXT("none"));
	}
	else
	{
		Result->SetArrayField(TEXT("landscapes"), LandscapeArray);
	}

	Result->SetNumberField(TEXT("count"), LandscapeArray.Num());
	Result->SetBoolField(TEXT("success"), true);
	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FLandscapeHandlers::ListLandscapeLayers(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		Result->SetStringField(TEXT("error"), TEXT("No editor world available"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	TArray<TSharedPtr<FJsonValue>> LayerArray;
	for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
	{
		ALandscapeProxy* Landscape = *It;
		if (!Landscape) continue;

		for (const FLandscapeInfoLayerSettings& LayerSettings : Landscape->EditorLayerSettings)
		{
			if (LayerSettings.LayerInfoObj)
			{
				TSharedPtr<FJsonObject> LayerObj = MakeShared<FJsonObject>();
				LayerObj->SetStringField(TEXT("name"), LayerSettings.LayerInfoObj->LayerName.ToString());
				LayerObj->SetStringField(TEXT("landscapeName"), Landscape->GetName());
				LayerArray.Add(MakeShared<FJsonValueObject>(LayerObj));
			}
		}
	}

	Result->SetArrayField(TEXT("layers"), LayerArray);
	Result->SetNumberField(TEXT("count"), LayerArray.Num());
	Result->SetBoolField(TEXT("success"), true);
	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FLandscapeHandlers::SampleLandscape(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	const TSharedPtr<FJsonObject>* PointObj = nullptr;
	if (!Params->TryGetObjectField(TEXT("point"), PointObj))
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing 'point' parameter"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	FVector Point;
	Point.X = (*PointObj)->GetNumberField(TEXT("x"));
	Point.Y = (*PointObj)->GetNumberField(TEXT("y"));
	Point.Z = (*PointObj)->GetNumberField(TEXT("z"));

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		Result->SetStringField(TEXT("error"), TEXT("No editor world available"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	// Find the first landscape and sample height
	for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
	{
		ALandscapeProxy* Landscape = *It;
		if (!Landscape) continue;

		// Use line trace to get the landscape height at the given point
		FVector TraceStart(Point.X, Point.Y, Point.Z + 100000.0f);
		FVector TraceEnd(Point.X, Point.Y, Point.Z - 100000.0f);

		FHitResult HitResult;
		FCollisionQueryParams QueryParams;
		QueryParams.bTraceComplex = true;

		if (World->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_WorldStatic, QueryParams))
		{
			if (HitResult.GetActor() && HitResult.GetActor()->IsA(ALandscapeProxy::StaticClass()))
			{
				Result->SetNumberField(TEXT("height"), HitResult.Location.Z);
				TSharedPtr<FJsonObject> HitPoint = MakeShared<FJsonObject>();
				HitPoint->SetNumberField(TEXT("x"), HitResult.Location.X);
				HitPoint->SetNumberField(TEXT("y"), HitResult.Location.Y);
				HitPoint->SetNumberField(TEXT("z"), HitResult.Location.Z);
				Result->SetObjectField(TEXT("hitLocation"), HitPoint);

				TSharedPtr<FJsonObject> Normal = MakeShared<FJsonObject>();
				Normal->SetNumberField(TEXT("x"), HitResult.Normal.X);
				Normal->SetNumberField(TEXT("y"), HitResult.Normal.Y);
				Normal->SetNumberField(TEXT("z"), HitResult.Normal.Z);
				Result->SetObjectField(TEXT("normal"), Normal);

				Result->SetBoolField(TEXT("hit"), true);
				Result->SetBoolField(TEXT("success"), true);
				return MakeShared<FJsonValueObject>(Result);
			}
		}
	}

	Result->SetBoolField(TEXT("hit"), false);
	Result->SetBoolField(TEXT("success"), true);
	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FLandscapeHandlers::ListLandscapeSplines(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		Result->SetStringField(TEXT("error"), TEXT("No editor world available"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	TArray<TSharedPtr<FJsonValue>> SplineArray;

	for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
	{
		ALandscapeProxy* Landscape = *It;
		if (!Landscape) continue;

		ULandscapeSplinesComponent* SplinesComp = Landscape->GetSplinesComponent();
		if (!SplinesComp) continue;

		const TArray<TObjectPtr<ULandscapeSplineControlPoint>>& ControlPoints = SplinesComp->GetControlPoints();
		for (const TObjectPtr<ULandscapeSplineControlPoint>& CP : ControlPoints)
		{
			if (!CP) continue;

			TSharedPtr<FJsonObject> PointObj = MakeShared<FJsonObject>();
			FVector Location = CP->Location;
			PointObj->SetNumberField(TEXT("x"), Location.X);
			PointObj->SetNumberField(TEXT("y"), Location.Y);
			PointObj->SetNumberField(TEXT("z"), Location.Z);
			PointObj->SetStringField(TEXT("landscapeName"), Landscape->GetName());
			SplineArray.Add(MakeShared<FJsonValueObject>(PointObj));
		}
	}

	Result->SetArrayField(TEXT("controlPoints"), SplineArray);
	Result->SetNumberField(TEXT("count"), SplineArray.Num());
	Result->SetBoolField(TEXT("success"), true);
	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FLandscapeHandlers::GetLandscapeComponent(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	int32 ComponentIndex = 0;
	if (Params->HasField(TEXT("componentIndex")))
	{
		ComponentIndex = static_cast<int32>(Params->GetNumberField(TEXT("componentIndex")));
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		Result->SetStringField(TEXT("error"), TEXT("No editor world available"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	// Collect all landscape components across all landscape proxies
	TArray<ULandscapeComponent*> AllComponents;
	for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
	{
		ALandscapeProxy* Landscape = *It;
		if (!Landscape) continue;

		TArray<ULandscapeComponent*> LandscapeComponents;
		Landscape->GetComponents<ULandscapeComponent>(LandscapeComponents);
		AllComponents.Append(LandscapeComponents);
	}

	if (ComponentIndex < 0 || ComponentIndex >= AllComponents.Num())
	{
		Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Component index %d out of range (0-%d)"), ComponentIndex, AllComponents.Num() - 1));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	ULandscapeComponent* Comp = AllComponents[ComponentIndex];
	if (!Comp)
	{
		Result->SetStringField(TEXT("error"), TEXT("Component is null"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	Result->SetNumberField(TEXT("componentIndex"), ComponentIndex);
	Result->SetStringField(TEXT("name"), Comp->GetName());

	FVector CompLocation = Comp->GetComponentLocation();
	Result->SetNumberField(TEXT("locationX"), CompLocation.X);
	Result->SetNumberField(TEXT("locationY"), CompLocation.Y);
	Result->SetNumberField(TEXT("locationZ"), CompLocation.Z);

	Result->SetNumberField(TEXT("sectionBaseX"), Comp->SectionBaseX);
	Result->SetNumberField(TEXT("sectionBaseY"), Comp->SectionBaseY);
	Result->SetNumberField(TEXT("componentSizeQuads"), Comp->ComponentSizeQuads);
	Result->SetNumberField(TEXT("subSections"), Comp->NumSubsections);

	FBox CompBounds = Comp->Bounds.GetBox();
	if (CompBounds.IsValid)
	{
		TSharedPtr<FJsonObject> BoundsObj = MakeShared<FJsonObject>();
		BoundsObj->SetNumberField(TEXT("minX"), CompBounds.Min.X);
		BoundsObj->SetNumberField(TEXT("minY"), CompBounds.Min.Y);
		BoundsObj->SetNumberField(TEXT("minZ"), CompBounds.Min.Z);
		BoundsObj->SetNumberField(TEXT("maxX"), CompBounds.Max.X);
		BoundsObj->SetNumberField(TEXT("maxY"), CompBounds.Max.Y);
		BoundsObj->SetNumberField(TEXT("maxZ"), CompBounds.Max.Z);
		Result->SetObjectField(TEXT("bounds"), BoundsObj);
	}

	Result->SetBoolField(TEXT("success"), true);
	return MakeShared<FJsonValueObject>(Result);
}
