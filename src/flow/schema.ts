import { z } from "zod";
import { EngineConfigSchema } from "@db-lyon/flowkit";

export const FlowVersionSchema = z.object({
  version: z.literal(1),
});

export const FlowProjectSchema = z.object({
  name: z.string().optional(),
  engine: z.string().optional(),
}).optional();

export const FlowConfigSchema = EngineConfigSchema.extend({
  "ue-mcp": FlowVersionSchema.optional(),
  project: FlowProjectSchema,
});

export type FlowConfig = z.infer<typeof FlowConfigSchema>;
