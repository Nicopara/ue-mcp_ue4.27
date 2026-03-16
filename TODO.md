# TODO — ue-mcp

> Remaining work to finish and ship v1.0.
> Current state: v0.3.0, builds clean, strict TS, 16 smoke test suites, CI/CD in place.

---

## C++ Bridge — Port & Ship (Python Bridge Removal)

> **Decision: C++ only.** Everything Python does, C++ can do — plus things Python can't
> (blueprint variable ops on 5.7+, FCoreDelegates dialog hooks, direct FProperty access).
> `execute_python` stays as an escape hatch via `IPythonScriptPlugin` in EditorHandlers.cpp.
> The Python WebSocket bridge server is retired.

### Handler Parity (~250 methods to port)

C++ has ~100 methods today. Python has ~350-400. The gap, grouped by handler:

- [ ] **Animation** — port: anim BP creation, montage creation, blendspace creation, sequence reading
- [ ] **Asset** — port: DataTable JSON import/export, texture property mutation, FBX mesh import (static/skeletal/anim)
- [ ] **Audio** — port: runtime playback (`play_sound_at_location`), AmbientSound spawning, MetaSound creation
- [ ] **Behavior Tree** — port: blackboard key creation, BT↔Blackboard binding
- [ ] **Blueprint** — audit parity: Python has ~25 methods with multi-fallback chains; C++ has 17. Port remaining (interface impl, event dispatcher details, graph reading depth)
- [ ] **Demo** — port the 19-step procedural scene builder (low priority — nice for showcase, not critical)
- [ ] **Editor** — port: INI read/write (use `GConfig`/`FConfigCacheIni`), save-all, undo/redo transaction control
- [ ] **Foliage** — port: settings inspection, paint/erase (`FoliageEditorLibrary`), instance sampling, layer creation
- [ ] **Game Framework** — port: CDO access/defaults, world settings override
- [ ] **GAS** — port: AttributeSet population, ability tag configuration, GameplayCue notify variants
- [ ] **Input** — already covered in GameplayHandlers; verify feature parity
- [ ] **Landscape** — port: sculpt/paint operations, heightmap import (use `FFileHelper`), material binding
- [ ] **Level Management** — port: sublevel enumeration, level loading/saving
- [ ] **Lighting** — port: full light property set (temperature, attenuation, shadow params)
- [ ] **Logs** — port: editor log reading (`FFileHelper::LoadFileToStringArray`), crash report scanning, log search
- [ ] **Material Authoring** — port: texture sample expression creation, expression connection, property binding
- [ ] **Navigation** — port: NavModifier volume spawning, navmesh rebuild
- [ ] **Networking** — audit: C++ has 7 methods, Python has 10. Port variable replication type, replication info query
- [ ] **Niagara** — port: emitter creation from template, parameter mutation on running components, system assembly
- [ ] **PCG** — port: node manipulation (add/connect/remove/set params), graph execution with seed, PCG volume placement
- [ ] **Performance** — port: actor class histogram, stat commands, scalability presets, viewport camera query
- [ ] **PIE** — port: runtime value query during play (`get_game_world` → actor → property)
- [ ] **Pipeline** — port: lighting build (quality-gated), HLOD generation, content cooking, asset validation
- [ ] **Reflection** — audit: verify gameplay tag creation parity (C++ has it; confirm INI fallback equivalent via `GConfig`)
- [ ] **Sequencer** — port: level sequence creation, track addition, actor binding, playback control
- [ ] **Skeleton** — port: physics asset body setup detail, socket transform data
- [ ] **Spline** — port: spline actor creation, point read/write
- [ ] **Texture** — port: texture listing, property inspection, settings mutation, file import
- [ ] **Volume** — port: volume type mapping (10 types: Trigger, Blocking, PainCausing, KillZ, Audio, PostProcess, etc.)
- [ ] **Widget/UMG** — port: widget tree traversal, property inspection/mutation, animation reading, EUW/EUB execution

### Infrastructure

- [ ] Remove Python bridge server code (`plugin/ue_mcp_bridge/*.py` handlers + `bridge_server.py`)
- [ ] Keep `execute_python` in EditorHandlers.cpp (escape hatch — requires PythonScriptPlugin at runtime, but bridge no longer depends on it)
- [ ] Update deployer (`deployer.ts`) — stop deploying Python bridge files, stop patching `DefaultEngine.ini` startup script, stop `pip install websockets`
- [ ] Update deployer to handle C++ plugin compilation or ship prebuilt binaries per UE version
- [ ] Remove `websockets` Python dependency
- [ ] Merge `feature/tests__cpp` → `main` once handler parity is achieved

### Multi-Version Support

- [ ] Add `#if ENGINE_MAJOR_VERSION` / `ENGINE_MINOR_VERSION` guards for API differences across 5.4–5.7
- [ ] Test C++ plugin compilation against UE 5.4, 5.5, 5.6, 5.7
- [ ] Document minimum supported UE version for C++ bridge

---

## Type Safety

- [ ] Eliminate `as any` casts in test files (`setup.ts`, `asset.test.ts`, `animation.test.ts`, `landscape.test.ts`, `level.test.ts`)
- [ ] Replace `catch (e: any)` with `catch (e: unknown)` + type narrowing in `tests/setup.ts` and `tests/reload-bridge.ts`
- [ ] Type bridge response payloads — define interfaces for each handler's return shape instead of using `Record<string, unknown>` / untyped `.result`
- [ ] Add typed action parameter maps per tool (currently all params merge into one loose `Record<string, unknown>`)

## Testing

- [ ] Add offline unit tests (no editor required) for core modules:
  - [ ] `project.ts` — path resolution, INI parsing, C++ header parsing
  - [ ] `bridge.ts` — connection lifecycle, message framing, timeout/reconnect
  - [ ] `deployer.ts` — .uproject mutation, file deployment logic
  - [ ] `types.ts` / `categoryTool()` — action dispatch, param mapping
- [ ] Add unit tests for `editor-control.ts` (mock process spawning)
- [ ] Set up test coverage reporting (vitest `--coverage`)
- [ ] Add CI-runnable test job (unit tests that don't need a live editor)
- [ ] Document smoke test prerequisites (which UE project, required plugins, expected test assets)

## Documentation

- [ ] Add troubleshooting section to README (common connection issues, port conflicts, C++ plugin build failures)
- [ ] Document error codes / bridge error response format
- [ ] Add per-action parameter docs (types, required vs optional, defaults)
- [ ] Add example workflows (e.g., "create a blueprint actor from scratch", "set up a material instance")
- [ ] Add CONTRIBUTING.md with dev setup, testing guide, PR conventions
- [ ] Update README to reflect C++-only architecture

## Error Handling & Resilience

- [ ] Validate bridge is connected before dispatching tool calls (return clear error instead of hang/crash)
- [ ] Add structured error types (connection lost, timeout, handler not found, UE exception)
- [ ] Surface C++ handler errors with context in dev mode
- [ ] Handle editor crash / unexpected disconnect gracefully (auto-reconnect + notify user)

## Security

- [ ] Add optional auth token for WebSocket bridge (prevent unintended connections)
- [ ] Validate/sanitize file paths in deployer to prevent path traversal
- [ ] Audit `execute_python` and `execute_console_command` for injection risks

## Pre-Release Polish

- [ ] Bump to v1.0.0 and update CHANGELOG
- [ ] Audit `package.json` — verify `files` field, `engines`, `bin`, `repository`, `keywords`
- [ ] Test fresh install flow end-to-end on a clean machine (npm install → first editor launch → tool invocation)
- [ ] Test against UE 5.4, 5.5, 5.6, 5.7 matrix
- [ ] Review and trim `instructions.ts` — ensure AI-facing docs match actual tool surface
- [ ] Remove `debug.log` from repo (add to .gitignore if not already)
- [ ] Clean up local-only branches (`feature/bak`, `feature/let-it-rip`, etc.)

## Nice-to-Have (Post v1)

- [ ] Configurable bridge port (currently hardcoded to 9877)
- [ ] Tool-level timeout configuration
- [ ] Batch action support (multiple actions in one tool call)
- [ ] WebSocket reconnect backoff strategy (exponential instead of fixed 15s)
- [ ] Publish to npm registry
- [ ] UE Marketplace listing for the C++ bridge plugin
