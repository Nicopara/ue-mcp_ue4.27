import * as fs from "node:fs";
import * as path from "node:path";
import { loadConfig, type LoadedConfig } from "flowkit";
import { FlowConfigSchema, type FlowConfig } from "./schema.js";

/**
 * Load ue-mcp.yml if it exists in the given directory.
 * Returns null if no ue-mcp.yml is found (config is optional).
 */
export function loadFlowConfig(configDir?: string): LoadedConfig<FlowConfig> | null {
  const dir = configDir ?? process.cwd();
  const configPath = path.join(dir, "ue-mcp.yml");

  if (!fs.existsSync(configPath)) return null;

  return loadConfig({
    filename: "ue-mcp.yml",
    schema: FlowConfigSchema,
    defaults: { tasks: {}, flows: {} },
    envVar: "UE_MCP_ENV",
    configDir: dir,
  });
}
