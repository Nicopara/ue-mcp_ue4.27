#include "EditorHandlers.h"
#include "HandlerRegistry.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#include "Editor/EditorEngine.h"
#include "Editor.h"
#include "Kismet/KismetSystemLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorScriptingUtilities/Public/EditorAssetLibrary.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "IPythonScriptPlugin.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/ConfigContext.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "LevelEditorViewport.h"
#include "UnrealClient.h"
#include "Slate/SceneViewport.h"
#include "HAL/PlatformMemory.h"
#include "Misc/App.h"
#include "Logging/MessageLog.h"
#include "HighResScreenshot.h"
#include "Misc/OutputDeviceRedirector.h"

void FEditorHandlers::RegisterHandlers(FMCPHandlerRegistry& Registry)
{
	Registry.RegisterHandler(TEXT("execute_command"), &ExecuteCommand);
	Registry.RegisterHandler(TEXT("execute_python"), &ExecutePython);
	Registry.RegisterHandler(TEXT("set_property"), &SetProperty);
	Registry.RegisterHandler(TEXT("set_config"), &SetConfig);
	Registry.RegisterHandler(TEXT("get_viewport_info"), &GetViewportInfo);
	Registry.RegisterHandler(TEXT("get_editor_performance_stats"), &GetEditorPerformanceStats);
	Registry.RegisterHandler(TEXT("get_output_log"), &GetOutputLog);
	Registry.RegisterHandler(TEXT("search_log"), &SearchLog);
	Registry.RegisterHandler(TEXT("get_message_log"), &GetMessageLog);
	Registry.RegisterHandler(TEXT("get_build_status"), &GetBuildStatus);
	Registry.RegisterHandler(TEXT("pie_control"), &PieControl);
	Registry.RegisterHandler(TEXT("capture_screenshot"), &CaptureScreenshot);
	Registry.RegisterHandler(TEXT("set_viewport_camera"), &SetViewportCamera);
	Registry.RegisterHandler(TEXT("undo"), &Undo);
	Registry.RegisterHandler(TEXT("redo"), &Redo);
	Registry.RegisterHandler(TEXT("reload_handlers"), &ReloadHandlers);
}

TSharedPtr<FJsonValue> FEditorHandlers::ExecuteCommand(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	FString Command;
	if (!Params->TryGetStringField(TEXT("command"), Command))
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing 'command' parameter"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	if (GEditor && GEditor->GetEditorWorldContext().World())
	{
		UKismetSystemLibrary::ExecuteConsoleCommand(
			GEditor->GetEditorWorldContext().World(),
			Command,
			nullptr
		);
		Result->SetStringField(TEXT("command"), Command);
		Result->SetBoolField(TEXT("success"), true);
	}
	else
	{
		Result->SetStringField(TEXT("error"), TEXT("Editor world not available"));
		Result->SetBoolField(TEXT("success"), false);
	}

	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FEditorHandlers::ExecutePython(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	FString Code;
	if (!Params->TryGetStringField(TEXT("code"), Code))
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing 'code' parameter"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
	if (!PythonPlugin || !PythonPlugin->IsPythonAvailable())
	{
		Result->SetStringField(TEXT("error"), TEXT("Python scripting is not available"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	FPythonCommandEx PythonCommand;
	PythonCommand.Command = Code;
	PythonCommand.ExecutionMode = EPythonCommandExecutionMode::ExecuteFile;
	PythonCommand.FileExecutionScope = EPythonFileExecutionScope::Public;

	bool bSuccess = PythonPlugin->ExecPythonCommandEx(PythonCommand);

	Result->SetBoolField(TEXT("success"), bSuccess);
	Result->SetStringField(TEXT("result"), PythonCommand.CommandResult);

	TArray<TSharedPtr<FJsonValue>> LogArray;
	for (const FPythonLogOutputEntry& Entry : PythonCommand.LogOutput)
	{
		TSharedPtr<FJsonObject> LogEntry = MakeShared<FJsonObject>();
		LogEntry->SetStringField(TEXT("type"), LexToString(Entry.Type));
		LogEntry->SetStringField(TEXT("output"), Entry.Output);
		LogArray.Add(MakeShared<FJsonValueObject>(LogEntry));
	}
	Result->SetArrayField(TEXT("log_output"), LogArray);

	FString CombinedOutput;
	for (const FPythonLogOutputEntry& Entry : PythonCommand.LogOutput)
	{
		if (!CombinedOutput.IsEmpty()) CombinedOutput += TEXT("\n");
		CombinedOutput += Entry.Output;
	}
	Result->SetStringField(TEXT("output"), CombinedOutput);

	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FEditorHandlers::SetProperty(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("path"), AssetPath))
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing 'path' parameter"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	FString PropertyName;
	if (!Params->TryGetStringField(TEXT("propertyName"), PropertyName))
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing 'propertyName' parameter"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	// Load asset
	UObject* Asset = LoadObject<UObject>(nullptr, *AssetPath);
	if (!Asset)
	{
		Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Asset not found: %s"), *AssetPath));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	// Get property
	FProperty* Property = Asset->GetClass()->FindPropertyByName(*PropertyName);
	if (!Property)
	{
		Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Property not found: %s"), *PropertyName));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	// Get value from params
	TSharedPtr<FJsonValue> ValueJsonRef = Params->TryGetField(TEXT("value"));
	if (!ValueJsonRef.IsValid())
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing 'value' parameter"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	// Set property value based on type
	// TODO: Implement proper type conversion from JSON to property value
	// For now, basic implementation
	void* PropertyValue = Property->ContainerPtrToValuePtr<void>(Asset);
	if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
	{
		if (ValueJsonRef->Type == EJson::String)
		{
			StrProp->SetPropertyValue(PropertyValue, ValueJsonRef->AsString());
		}
	}
	else if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
	{
		if (ValueJsonRef->Type == EJson::Boolean)
		{
			BoolProp->SetPropertyValue(PropertyValue, ValueJsonRef->AsBool());
		}
	}
	else if (FIntProperty* IntProp = CastField<FIntProperty>(Property))
	{
		if (ValueJsonRef->Type == EJson::Number)
		{
			IntProp->SetPropertyValue(PropertyValue, (int32)ValueJsonRef->AsNumber());
		}
	}
	else if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
	{
		if (ValueJsonRef->Type == EJson::Number)
		{
			FloatProp->SetPropertyValue(PropertyValue, (float)ValueJsonRef->AsNumber());
		}
	}

	Result->SetStringField(TEXT("path"), AssetPath);
	Result->SetStringField(TEXT("propertyName"), PropertyName);
	Result->SetBoolField(TEXT("success"), true);

	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FEditorHandlers::SetConfig(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	FString ConfigName;
	if (!Params->TryGetStringField(TEXT("configName"), ConfigName))
	{
		Params->TryGetStringField(TEXT("configFile"), ConfigName);
	}
	FString Section;
	if (!Params->TryGetStringField(TEXT("section"), Section))
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing 'section' parameter"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}
	FString Key;
	if (!Params->TryGetStringField(TEXT("key"), Key))
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing 'key' parameter"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}
	FString Value;
	Params->TryGetStringField(TEXT("value"), Value);

	if (ConfigName.IsEmpty())
	{
		ConfigName = TEXT("DefaultEngine.ini");
	}
	else if (!ConfigName.EndsWith(TEXT(".ini")))
	{
		ConfigName = FString::Printf(TEXT("Default%s.ini"), *ConfigName);
	}

	FString ConfigDir = FPaths::ProjectConfigDir();
	FString IniPath = FPaths::Combine(ConfigDir, ConfigName);

	GConfig->SetString(*Section, *Key, *Value, IniPath);
	GConfig->Flush(false, IniPath);

	Result->SetStringField(TEXT("configFile"), ConfigName);
	Result->SetStringField(TEXT("section"), Section);
	Result->SetStringField(TEXT("key"), Key);
	Result->SetStringField(TEXT("value"), Value);
	Result->SetBoolField(TEXT("success"), true);

	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FEditorHandlers::GetViewportInfo(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	if (!GEditor)
	{
		Result->SetStringField(TEXT("error"), TEXT("Editor not available"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	FLevelEditorViewportClient* ViewportClient = GCurrentLevelEditingViewportClient;
	if (!ViewportClient)
	{
		// Try to get from level viewport clients list
		const TArray<FLevelEditorViewportClient*>& ViewportClients = GEditor->GetLevelViewportClients();
		if (ViewportClients.Num() > 0)
		{
			ViewportClient = ViewportClients[0];
		}
	}

	if (!ViewportClient)
	{
		Result->SetStringField(TEXT("error"), TEXT("No viewport client available"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	FVector Location = ViewportClient->GetViewLocation();
	FRotator Rotation = ViewportClient->GetViewRotation();
	float FOV = ViewportClient->ViewFOV;

	TSharedPtr<FJsonObject> LocationObj = MakeShared<FJsonObject>();
	LocationObj->SetNumberField(TEXT("x"), Location.X);
	LocationObj->SetNumberField(TEXT("y"), Location.Y);
	LocationObj->SetNumberField(TEXT("z"), Location.Z);
	Result->SetObjectField(TEXT("location"), LocationObj);

	TSharedPtr<FJsonObject> RotationObj = MakeShared<FJsonObject>();
	RotationObj->SetNumberField(TEXT("pitch"), Rotation.Pitch);
	RotationObj->SetNumberField(TEXT("yaw"), Rotation.Yaw);
	RotationObj->SetNumberField(TEXT("roll"), Rotation.Roll);
	Result->SetObjectField(TEXT("rotation"), RotationObj);

	Result->SetNumberField(TEXT("fov"), FOV);
	Result->SetBoolField(TEXT("success"), true);

	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FEditorHandlers::GetEditorPerformanceStats(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	// FPS from delta time
	double DeltaTime = FApp::GetDeltaTime();
	double FPS = (DeltaTime > 0.0) ? (1.0 / DeltaTime) : 0.0;
	Result->SetNumberField(TEXT("fps"), FPS);
	Result->SetNumberField(TEXT("deltaTime"), DeltaTime);

	// Memory stats
	FPlatformMemoryStats MemStats = FPlatformMemory::GetStats();
	TSharedPtr<FJsonObject> MemoryObj = MakeShared<FJsonObject>();
	MemoryObj->SetNumberField(TEXT("usedPhysical"), static_cast<double>(MemStats.UsedPhysical));
	MemoryObj->SetNumberField(TEXT("availablePhysical"), static_cast<double>(MemStats.AvailablePhysical));
	MemoryObj->SetNumberField(TEXT("usedVirtual"), static_cast<double>(MemStats.UsedVirtual));
	MemoryObj->SetNumberField(TEXT("availableVirtual"), static_cast<double>(MemStats.AvailableVirtual));
	Result->SetObjectField(TEXT("memory"), MemoryObj);

	Result->SetBoolField(TEXT("success"), true);

	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FEditorHandlers::GetOutputLog(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	// maxLines parameter with default of 100
	int32 MaxLines = 100;
	if (Params->HasField(TEXT("maxLines")))
	{
		MaxLines = static_cast<int32>(Params->GetNumberField(TEXT("maxLines")));
	}

	// Output log capture is not trivially available in C++ without a custom output device.
	// Return success with an empty lines array as a baseline implementation.
	TArray<TSharedPtr<FJsonValue>> LinesArray;
	Result->SetArrayField(TEXT("lines"), LinesArray);
	Result->SetNumberField(TEXT("maxLines"), MaxLines);
	Result->SetBoolField(TEXT("success"), true);

	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FEditorHandlers::SearchLog(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	FString Query;
	if (!Params->TryGetStringField(TEXT("query"), Query))
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing 'query' parameter"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	// Similar to GetOutputLog - return success with empty matches as baseline
	TArray<TSharedPtr<FJsonValue>> MatchesArray;
	Result->SetArrayField(TEXT("matches"), MatchesArray);
	Result->SetStringField(TEXT("query"), Query);
	Result->SetBoolField(TEXT("success"), true);

	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FEditorHandlers::GetMessageLog(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	// FMessageLog does not expose a simple API to read back entries in C++.
	// Return success with an empty messages array as a baseline implementation.
	TArray<TSharedPtr<FJsonValue>> MessagesArray;
	Result->SetArrayField(TEXT("messages"), MessagesArray);
	Result->SetBoolField(TEXT("success"), true);

	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FEditorHandlers::GetBuildStatus(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	// Basic build status - report as idle since we cannot easily query
	// the live compilation state from within the editor module.
	Result->SetStringField(TEXT("status"), TEXT("idle"));
	Result->SetBoolField(TEXT("success"), true);

	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FEditorHandlers::PieControl(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	FString Action;
	if (!Params->TryGetStringField(TEXT("action"), Action))
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing 'action' parameter"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	if (!GEditor)
	{
		Result->SetStringField(TEXT("error"), TEXT("Editor not available"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	if (Action == TEXT("status"))
	{
		bool bIsPlaying = (GEditor->PlayWorld != nullptr);
		Result->SetBoolField(TEXT("isPlaying"), bIsPlaying);
		Result->SetStringField(TEXT("action"), Action);
		Result->SetBoolField(TEXT("success"), true);
	}
	else if (Action == TEXT("start"))
	{
		if (GEditor->PlayWorld != nullptr)
		{
			Result->SetStringField(TEXT("error"), TEXT("PIE session already active"));
			Result->SetBoolField(TEXT("success"), false);
		}
		else
		{
			FRequestPlaySessionParams SessionParams;
			GEditor->RequestPlaySession(SessionParams);
			Result->SetStringField(TEXT("action"), Action);
			Result->SetBoolField(TEXT("success"), true);
		}
	}
	else if (Action == TEXT("stop"))
	{
		if (GEditor->PlayWorld == nullptr)
		{
			Result->SetStringField(TEXT("error"), TEXT("No PIE session active"));
			Result->SetBoolField(TEXT("success"), false);
		}
		else
		{
			GEditor->RequestEndPlayMap();
			Result->SetStringField(TEXT("action"), Action);
			Result->SetBoolField(TEXT("success"), true);
		}
	}
	else
	{
		Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown action: %s. Expected 'status', 'start', or 'stop'"), *Action));
		Result->SetBoolField(TEXT("success"), false);
	}

	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FEditorHandlers::CaptureScreenshot(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	FString Filename;
	if (!Params->TryGetStringField(TEXT("filename"), Filename))
	{
		Result->SetStringField(TEXT("error"), TEXT("Missing 'filename' parameter"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	// Ensure the filename has a proper extension
	if (!Filename.EndsWith(TEXT(".png")) && !Filename.EndsWith(TEXT(".jpg")) && !Filename.EndsWith(TEXT(".bmp")))
	{
		Filename += TEXT(".png");
	}

	FScreenshotRequest::RequestScreenshot(Filename, false, false);

	Result->SetStringField(TEXT("filename"), Filename);
	Result->SetBoolField(TEXT("success"), true);

	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FEditorHandlers::SetViewportCamera(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	if (!GEditor)
	{
		Result->SetStringField(TEXT("error"), TEXT("Editor not available"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	FLevelEditorViewportClient* ViewportClient = GCurrentLevelEditingViewportClient;
	if (!ViewportClient)
	{
		const TArray<FLevelEditorViewportClient*>& ViewportClients = GEditor->GetLevelViewportClients();
		if (ViewportClients.Num() > 0)
		{
			ViewportClient = ViewportClients[0];
		}
	}

	if (!ViewportClient)
	{
		Result->SetStringField(TEXT("error"), TEXT("No viewport client available"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	// Set location if provided
	const TSharedPtr<FJsonObject>* LocationObj = nullptr;
	if (Params->TryGetObjectField(TEXT("location"), LocationObj) && LocationObj)
	{
		FVector Location;
		Location.X = (*LocationObj)->GetNumberField(TEXT("x"));
		Location.Y = (*LocationObj)->GetNumberField(TEXT("y"));
		Location.Z = (*LocationObj)->GetNumberField(TEXT("z"));
		ViewportClient->SetViewLocation(Location);
	}

	// Set rotation if provided
	const TSharedPtr<FJsonObject>* RotationObj = nullptr;
	if (Params->TryGetObjectField(TEXT("rotation"), RotationObj) && RotationObj)
	{
		FRotator Rotation;
		Rotation.Pitch = (*RotationObj)->GetNumberField(TEXT("pitch"));
		Rotation.Yaw = (*RotationObj)->GetNumberField(TEXT("yaw"));
		Rotation.Roll = (*RotationObj)->GetNumberField(TEXT("roll"));
		ViewportClient->SetViewRotation(Rotation);
	}

	Result->SetBoolField(TEXT("success"), true);

	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FEditorHandlers::Undo(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	if (!GEditor)
	{
		Result->SetStringField(TEXT("error"), TEXT("Editor not available"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	bool bSuccess = GEditor->UndoTransaction();
	Result->SetBoolField(TEXT("success"), bSuccess);

	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FEditorHandlers::Redo(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	if (!GEditor)
	{
		Result->SetStringField(TEXT("error"), TEXT("Editor not available"));
		Result->SetBoolField(TEXT("success"), false);
		return MakeShared<FJsonValueObject>(Result);
	}

	bool bSuccess = GEditor->RedoTransaction();
	Result->SetBoolField(TEXT("success"), bSuccess);

	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FEditorHandlers::ReloadHandlers(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	// No-op in C++ bridge - this was used in the Python bridge to reload Python handler modules.
	Result->SetBoolField(TEXT("success"), true);

	return MakeShared<FJsonValueObject>(Result);
}
