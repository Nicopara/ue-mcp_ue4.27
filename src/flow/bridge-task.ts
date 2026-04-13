import { BaseTask, type TaskResult } from "flowkit";
import type { FlowContext } from "./context.js";

/**
 * Generic task for bridge-delegation actions.
 *
 * Used two ways:
 *
 * 1. **YAML-defined tasks** (`class_path: flow.bridge`):
 *    The `method` option specifies the bridge method to call.
 *    Remaining options are passed as bridge params.
 *
 * 2. **Built-in tasks** via `bridgeTaskClass()` factory:
 *    The bridge method is baked into the class closure.
 *    Options are passed through as bridge params.
 */
export class BridgeTask extends BaseTask {
  get taskName() {
    return `bridge:${(this.options as Record<string, unknown>).method ?? "unknown"}`;
  }

  async execute(): Promise<TaskResult> {
    const { method, ...params } = this.options as Record<string, unknown>;
    if (!method || typeof method !== "string") {
      throw new Error('BridgeTask requires a "method" option');
    }
    const ctx = this.ctx as FlowContext;
    const data = await ctx.bridge.call(method as string, params);
    return {
      success: true,
      data: typeof data === "object" && data !== null
        ? (data as Record<string, unknown>)
        : { result: data },
    };
  }
}
