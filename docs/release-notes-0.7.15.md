# v0.7.15 Release Notes

PoseSearch (motion matching) authoring. Agents can now assemble a `UPoseSearchDatabase` from scratch — create the asset, attach a schema, append animation entries, and trigger the async index build.

## New Actions

### animation — PoseSearch
- `create_pose_search_database` — create a `UPoseSearchDatabase` asset. Params: `name`, `packagePath?` (default `/Game/MotionMatching`), `schemaPath?` (optional `/Game/...` path to a `UPoseSearchSchema`).
- `set_pose_search_schema` — attach (or swap) the Schema on an existing database. Params: `assetPath`, `schemaPath`. Emits a rollback record with the previous schema when one was set.
- `add_pose_search_sequence` — append an animation entry. Accepts `UAnimSequence`, `UAnimComposite`, `UAnimMontage`, or `UBlendSpace`. Params: `assetPath`, `sequencePath`. Returns the previous + new asset count and the new entry's index.
- `build_pose_search_index` — trigger the PoseSearch async index build via `FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex`. Params: `assetPath`, `wait?` (default `true` — block until the build resolves). Refuses to run if the database has no schema or no animation entries, surfacing a structured error instead of crashing.
- `read_pose_search_database` — inspect a database: schema path + sampleRate + channel count + skeleton count, every animation entry (index, path, class, looping/root-motion flags), cost biases, kd-tree neighbor count, and tags. Params: `assetPath`.

## Workflow — authoring a motion-matching database

1. `asset(list)` (with `classFilter`) to pick the source `UPoseSearchSchema` — or use an Epic-provided sample schema.
2. `animation(create_pose_search_database, { name, packagePath, schemaPath })` to stand the asset up.
3. `animation(add_pose_search_sequence, { assetPath, sequencePath })` per clip you want indexed.
4. `animation(build_pose_search_index, { assetPath })` to produce the index.
5. `animation(read_pose_search_database, { assetPath })` to verify the final layout.

## Internals
- New handlers in `plugin/ue_mcp_bridge/Source/.../Handlers/AnimationHandlers.cpp` (+ header declarations) wire into the existing `FAnimationHandlers` registry.
- `UE_MCP_Bridge.Build.cs` now depends on the `PoseSearch` module; the `.uplugin` declares the PoseSearch plugin dependency so the MCP bridge loads cleanly even when a project doesn't otherwise enable it.
- `UPoseSearchSchema::Channels` / `Skeletons` are private members; the read handler uses the public `GetChannels()` accessor and reflects over the `Skeletons` `FArrayProperty` to surface counts without needing API changes upstream.
- `build_pose_search_index` passes `ERequestAsyncBuildFlag::NewRequest | WaitForCompletion` by default so the action is effectively synchronous for scripting. Set `wait: false` for fire-and-forget (returns `InProgress`).

## Known constraints
- `add_pose_search_sequence` creates a vanilla `FPoseSearchDatabaseAnimationAsset` with defaults (no mirroring, no sampling range trim, no blendspace grid overrides). Tune the entry in-editor post-creation if you need those settings — a follow-up patch may expose them.
- `build_pose_search_index` with `wait: true` ticks synchronously on the game thread; very large databases may hold the tick for multiple seconds.
