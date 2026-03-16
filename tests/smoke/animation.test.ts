import { describe, it, expect, beforeAll, afterAll } from "vitest";
import { getBridge, disconnectBridge, callBridge, resultArray } from "../setup.js";
import type { EditorBridge } from "../../src/bridge.js";

let bridge: EditorBridge;

beforeAll(async () => { bridge = await getBridge(); });
afterAll(() => disconnectBridge());

describe("animation — read / list", () => {
  it("list_anim_assets", async () => {
    const r = await callBridge(bridge, "list_anim_assets", { recursive: true });
    expect(r.ok, r.error).toBe(true);
  });

  it("list_skeletal_meshes", async () => {
    const r = await callBridge(bridge, "list_skeletal_meshes", { recursive: true });
    expect(r.ok, r.error).toBe(true);
  });
});

describe("animation — read specific (dynamic)", () => {
  let skelMeshPath: string | undefined;

  beforeAll(async () => {
    const r = await callBridge(bridge, "list_skeletal_meshes", { recursive: true });
    if (r.ok) {
      const items = resultArray(r.result, "assets", "meshes");
      if (items && items.length > 0) {
        const first = items[0] as Record<string, unknown>;
        skelMeshPath = (first.path ?? first.asset_path ?? first.objectPath) as string | undefined;
      }
    }
  });

  it("get_skeleton_info", async ({ skip }) => {
    if (!skelMeshPath) skip();
    const r = await callBridge(bridge, "get_skeleton_info", { assetPath: skelMeshPath });
    expect(r.ok, r.error).toBe(true);
  });

  it("list_sockets", async ({ skip }) => {
    if (!skelMeshPath) skip();
    const r = await callBridge(bridge, "list_sockets", { assetPath: skelMeshPath });
    expect(r.ok, r.error).toBe(true);
  });

  it("get_physics_asset_info", async ({ skip }) => {
    if (!skelMeshPath) skip();
    const r = await callBridge(bridge, "get_physics_asset_info", { assetPath: skelMeshPath });
    expect(r.ok, r.error).toBe(true);
  });
});
