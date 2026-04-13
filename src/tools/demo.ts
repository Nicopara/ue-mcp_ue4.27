import { z } from "zod";
import { categoryTool, bp, type ToolDef } from "../types.js";

export const demoTool: ToolDef = categoryTool(
  "demo",
  "Neon Shrine demo scene builder and cleanup.",
  {
    step:    bp("Execute demo step. Params: stepIndex?", "demo_step", (p) => p.stepIndex !== undefined ? { step: p.stepIndex } : {}),
    cleanup: bp("Remove demo assets and actors", "demo_cleanup"),
  },
  undefined,
  {
    stepIndex: z.number().optional().describe("Step index to execute. Omit for step list."),
  },
);
