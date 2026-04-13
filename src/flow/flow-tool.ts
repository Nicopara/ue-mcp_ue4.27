import { z } from "zod";
import { FlowRunner } from "@db-lyon/flowkit";
import type { TaskRegistry, FlowRunResult, TaskDefinition, FlowDefinition } from "@db-lyon/flowkit";
import type { FlowContext } from "./context.js";
import type { FlowConfig } from "./schema.js";
import type { ToolDef, ToolContext } from "../types.js";

export function createFlowTool(
  registry: TaskRegistry,
  config: FlowConfig,
): ToolDef {
  const flowNames = Object.keys(config.flows);

  return {
    name: "flow",
    description:
      `Run or inspect named flows defined in ue-mcp.yml.\n\n` +
      `Available flows: ${flowNames.length > 0 ? flowNames.join(", ") : "(none — add flows to ue-mcp.yml)"}` +
      `\n\nActions:\n` +
      `- run: Execute a flow. Params: flowName, skip?\n` +
      `- plan: Show execution plan without running. Params: flowName\n` +
      `- list: List available flows`,
    schema: {
      action: z.enum(["run", "plan", "list"]).describe("Action to perform"),
      flowName: z.string().optional().describe("Flow name from ue-mcp.yml"),
      skip: z.array(z.string()).optional().describe("Step names or numbers to skip"),
    },
    actions: {
      run: { handler: async (ctx, params) => runFlow(registry, config, ctx, params) },
      plan: { handler: async (ctx, params) => planFlow(registry, config, ctx, params) },
      list: { handler: async () => listFlows(config) },
    },
    handler: async (ctx, params) => {
      const action = params.action as string;
      if (action === "list") return listFlows(config);
      if (action === "plan") return planFlow(registry, config, ctx, params);
      if (action === "run") return runFlow(registry, config, ctx, params);
      throw new Error(`Unknown flow action: ${action}`);
    },
  };
}

function listFlows(config: FlowConfig): Record<string, unknown> {
  const flows = Object.entries(config.flows).map(([name, def]) => ({
    name,
    description: def.description,
    stepCount: Object.keys(def.steps).length,
  }));
  return { flowCount: flows.length, flows };
}

async function planFlow(
  registry: TaskRegistry,
  config: FlowConfig,
  ctx: ToolContext,
  params: Record<string, unknown>,
): Promise<unknown> {
  const flowName = params.flowName as string;
  if (!flowName) throw new Error("flowName is required");

  const runner = makeRunner(registry, config, ctx);
  return runner.run({ flowName, plan: true });
}

async function runFlow(
  registry: TaskRegistry,
  config: FlowConfig,
  ctx: ToolContext,
  params: Record<string, unknown>,
): Promise<unknown> {
  const flowName = params.flowName as string;
  if (!flowName) throw new Error("flowName is required");
  const skip = (params.skip as string[] | undefined) ?? [];

  const runner = makeRunner(registry, config, ctx);
  const result = await runner.run({ flowName, skip });

  return formatFlowResult(result);
}

function makeRunner(registry: TaskRegistry, config: FlowConfig, ctx: ToolContext): FlowRunner {
  const flowCtx: FlowContext = {
    bridge: ctx.bridge,
    project: ctx.project,
  };

  return new FlowRunner({
    tasks: config.tasks as Record<string, TaskDefinition>,
    flows: config.flows as Record<string, FlowDefinition>,
    registry,
    context: flowCtx,
  });
}

function formatFlowResult(result: FlowRunResult): Record<string, unknown> {
  return {
    success: result.success,
    duration: result.duration,
    error: result.error?.message,
    steps: result.steps.map((s) => ({
      step: s.stepNumber,
      name: s.name,
      type: s.type,
      skipped: s.skipped,
      success: s.result?.success ?? null,
      duration: s.duration,
      error: s.result?.error?.message,
    })),
  };
}
