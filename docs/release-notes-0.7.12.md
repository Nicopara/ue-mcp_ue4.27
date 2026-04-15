# v0.7.12 Release Notes

A bug-fix release closing the last two open agent-feedback issues (#127, #128) plus the five sub-items #128 bundled together.

## Behaviour changes

### Bridge deployment no longer overwrites local source on server startup (#127)
Previously, every `ue-mcp <uproject>` invocation (including normal MCP-server startup) copied the packaged bridge source into `Plugins/UE_MCP_Bridge/Source/`, clobbering any local fork or project-specific edits. As of 0.7.12, startup runs a non-destructive `attach()` that:

- ensures `PythonScriptPlugin` and `UE_MCP_Bridge` are listed in the `.uproject`,
- checks the installed bridge version against the packaged one, and
- logs a clear warning if the bridge is missing or version-mismatched — **without ever touching `Source/`**.

Source deployment is now reserved for the explicit `ue-mcp init <uproject>` and `ue-mcp update <uproject>` commands, as documented. Existing forks and project-tracked bridge revisions are safe.

## New actions

### blueprint
- `get_component_property` — single-property read on an SCS or inherited component template. For child BPs, returns the `InheritableComponentHandler` override value if one exists (so reads reflect what `set_component_property` would mutate), otherwise falls back to the parent template's default. Closes #128 (item 2).

### asset
- `bulk_rename` — batched rename using `IAssetTools::RenameAssets(TArray<FAssetRenameData>)`. All renames go through one transaction with a single redirector-fixup pass, matching the Content Browser drag-move behaviour. Use this over looped `rename` for scene-referenced assets — individual renames were crashing the plugin at batches as low as 10–15. Closes #128 (item 6).

## Bug fixes

- `blueprint(set_component_property)` — inherited components in child Blueprints now route through the child's `UInheritableComponentHandler` override template (equivalent to Python's `SubobjectDataBlueprintFunctionLibrary.get_object_for_blueprint`). Previously, writes either failed with `Available: []` or, worse, mutated the parent's shared template and corrupted every descendant. The response now includes an `inherited: true|false` flag. Closes #128 (items 1 & 3).
- `blueprint(read_component_properties)` — now uses the same inherited-aware resolver and reports `inherited` in the response.
- `asset(set_texture_settings)` — every key returned "No valid properties specified" because the TS tool shipped them inside a `settings` object but the C++ handler read top-level keys. The transform now flattens `settings` and also accepts the keys at the top level for convenience. Closes #128 (item 4).
- `editor(execute_python)` — long-running scripts (>~30s) were crashing the editor with `EXCEPTION_ACCESS_VIOLATION` at `FMCPGameThreadExecutor::ExecuteOnGameThread`. The ticker lambda captured the caller's stack by reference; when the MCP handler timed out and unwound, the ticker eventually fired and wrote through dangling references while triggering a returned-to-pool event. The executor now captures shared state by value, synchronises event ownership under a mutex, and skips the work entirely when the caller has abandoned the wait. Closes #128 (item 5).

## Engine compatibility (UE 5.7)
- PCG: `UPCGGraph::NotifyGraphChanged` became private in 5.7. Replaced with `PostEditChange()`; save/dirty behaviour is unchanged, but open PCG graph tabs may need a reopen to refresh.
- Sequencer: `FMovieSceneChannelProxy::GetChannel<T>(FName)` was removed. Replaced with `GetChannelByName<T>(FName).Get()`, and `GetData().GetValues()` now yields a `TArrayView` rather than a `TArray&`.
