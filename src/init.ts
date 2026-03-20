#!/usr/bin/env node
import * as fs from "node:fs";
import * as path from "node:path";
import * as readline from "node:readline";
import { ProjectContext } from "./project.js";
import { deploy, deploySummary } from "./deployer.js";

/* ------------------------------------------------------------------ */
/*  Terminal helpers                                                    */
/* ------------------------------------------------------------------ */

const RESET = "\x1b[0m";
const BOLD = "\x1b[1m";
const DIM = "\x1b[2m";
const GREEN = "\x1b[32m";
const YELLOW = "\x1b[33m";
const CYAN = "\x1b[36m";
const RED = "\x1b[31m";

const ok = (msg: string) => console.log(`  ${GREEN}✓${RESET} ${msg}`);
const warn = (msg: string) => console.log(`  ${YELLOW}!${RESET} ${msg}`);
const fail = (msg: string) => console.log(`  ${RED}✗${RESET} ${msg}`);
const info = (msg: string) => console.log(`  ${DIM}${msg}${RESET}`);

function createRL(): readline.Interface {
  return readline.createInterface({
    input: process.stdin,
    output: process.stdout,
  });
}

function ask(rl: readline.Interface, question: string): Promise<string> {
  return new Promise((resolve) => rl.question(question, resolve));
}

/* ------------------------------------------------------------------ */
/*  Tool categories                                                    */
/* ------------------------------------------------------------------ */

interface ToolCategory {
  name: string;
  label: string;
  requiredPlugins?: string[];
  alwaysOn?: boolean;
}

const CATEGORIES: ToolCategory[] = [
  { name: "project", label: "project", alwaysOn: true },
  { name: "editor", label: "editor", alwaysOn: true },
  { name: "reflection", label: "reflection", alwaysOn: true },
  { name: "level", label: "levels" },
  { name: "blueprint", label: "blueprints" },
  { name: "material", label: "materials" },
  { name: "asset", label: "assets" },
  { name: "animation", label: "animation" },
  { name: "niagara", label: "vfx (niagara)", requiredPlugins: ["Niagara"] },
  { name: "landscape", label: "landscape" },
  { name: "pcg", label: "pcg", requiredPlugins: ["PCG"] },
  { name: "foliage", label: "foliage" },
  { name: "audio", label: "audio" },
  { name: "widget", label: "ui (widgets)" },
  { name: "gameplay", label: "gameplay / ai", requiredPlugins: ["EnhancedInput"] },
  { name: "gas", label: "gas", requiredPlugins: ["GameplayAbilities"] },
  { name: "networking", label: "networking" },
  { name: "demo", label: "demo" },
  { name: "feedback", label: "feedback", alwaysOn: true },
];

/* ------------------------------------------------------------------ */
/*  MCP client detection                                               */
/* ------------------------------------------------------------------ */

interface McpClient {
  name: string;
  configPath: string;
  detected: boolean;
}

function detectMcpClients(projectDir: string): McpClient[] {
  const home = process.env.HOME || process.env.USERPROFILE || "";
  const clients: McpClient[] = [];

  // Claude Code — .mcp.json in project root or ~/.claude/
  const claudeProjectMcp = path.join(projectDir, ".mcp.json");
  const claudeGlobalMcp = path.join(home, ".claude", ".mcp.json");
  clients.push({
    name: "Claude Code (project)",
    configPath: claudeProjectMcp,
    detected: fs.existsSync(claudeProjectMcp),
  });
  clients.push({
    name: "Claude Code (global)",
    configPath: claudeGlobalMcp,
    detected: fs.existsSync(path.dirname(claudeGlobalMcp)),
  });

  // Claude Desktop
  const appData = process.env.APPDATA || path.join(home, "AppData", "Roaming");
  const claudeDesktop = path.join(appData, "Claude", "claude_desktop_config.json");
  clients.push({
    name: "Claude Desktop",
    configPath: claudeDesktop,
    detected: fs.existsSync(path.dirname(claudeDesktop)),
  });

  // Cursor — .cursor/mcp.json in project root
  const cursorMcp = path.join(projectDir, ".cursor", "mcp.json");
  clients.push({
    name: "Cursor",
    configPath: cursorMcp,
    detected: fs.existsSync(path.join(projectDir, ".cursor")),
  });

  return clients;
}

function writeMcpConfig(configPath: string, uprojectPath: string): void {
  let existing: Record<string, unknown> = {};
  if (fs.existsSync(configPath)) {
    try {
      existing = JSON.parse(fs.readFileSync(configPath, "utf-8"));
    } catch {
      // corrupt file, overwrite
    }
  }

  const mcpServers = (existing.mcpServers ?? {}) as Record<string, unknown>;
  mcpServers["ue-mcp"] = {
    command: "npx",
    args: ["ue-mcp", uprojectPath.replace(/\\/g, "/")],
  };
  existing.mcpServers = mcpServers;

  const dir = path.dirname(configPath);
  if (!fs.existsSync(dir)) fs.mkdirSync(dir, { recursive: true });
  fs.writeFileSync(configPath, JSON.stringify(existing, null, 2));
}

/* ------------------------------------------------------------------ */
/*  .ue-mcp.json config                                                */
/* ------------------------------------------------------------------ */

interface UeMcpInitConfig {
  contentRoots?: string[];
  disable?: string[];
}

function writeProjectConfig(projectDir: string, disabled: string[]): void {
  const configPath = path.join(projectDir, ".ue-mcp.json");
  let existing: UeMcpInitConfig = {};
  if (fs.existsSync(configPath)) {
    try {
      existing = JSON.parse(fs.readFileSync(configPath, "utf-8"));
    } catch {
      // overwrite
    }
  }

  if (disabled.length > 0) {
    existing.disable = disabled;
  } else {
    delete existing.disable;
  }

  if (!existing.contentRoots) {
    existing.contentRoots = ["/Game/"];
  }

  fs.writeFileSync(configPath, JSON.stringify(existing, null, 2));
}

/* ------------------------------------------------------------------ */
/*  Plugin enablement                                                  */
/* ------------------------------------------------------------------ */

function ensurePluginsEnabled(uprojectPath: string, pluginNames: string[]): string[] {
  const raw = fs.readFileSync(uprojectPath, "utf-8");
  const root = JSON.parse(raw);
  if (!root.Plugins) root.Plugins = [];

  const enabled: string[] = [];
  for (const name of pluginNames) {
    const existing = root.Plugins.find(
      (p: { Name?: string }) => p.Name?.toLowerCase() === name.toLowerCase(),
    );
    if (!existing) {
      root.Plugins.push({ Name: name, Enabled: true });
      enabled.push(name);
    } else if (!existing.Enabled) {
      existing.Enabled = true;
      enabled.push(name);
    }
  }

  if (enabled.length > 0) {
    fs.writeFileSync(uprojectPath, JSON.stringify(root, null, "\t"));
  }

  return enabled;
}

/* ------------------------------------------------------------------ */
/*  Main init flow                                                     */
/* ------------------------------------------------------------------ */

async function init() {
  console.log("");
  console.log(`  ${BOLD}${CYAN}UE-MCP Setup${RESET}`);
  console.log("");

  const rl = createRL();

  // 1. Get project path
  let uprojectPath = process.argv[2] || "";

  if (!uprojectPath) {
    uprojectPath = await ask(rl, `  ${BOLD}?${RESET} UE project path (.uproject or directory): `);
    uprojectPath = uprojectPath.trim().replace(/^["']|["']$/g, "");
  }

  // Resolve to .uproject file
  const project = new ProjectContext();
  try {
    project.setProject(uprojectPath);
  } catch (e) {
    fail(e instanceof Error ? e.message : String(e));
    rl.close();
    process.exit(1);
  }

  ok(`Found UE ${project.engineAssociation ?? "?"} project "${project.projectName}"`);
  console.log("");

  // 2. Tool category selection
  console.log(`  ${BOLD}?${RESET} Which tool categories do you need?`);
  console.log(`    ${DIM}(enter comma-separated numbers to disable, or press Enter for all)${RESET}`);
  console.log("");

  const optional = CATEGORIES.filter((c) => !c.alwaysOn);
  for (let i = 0; i < optional.length; i++) {
    const c = optional[i];
    const plugins = c.requiredPlugins ? ` ${DIM}(requires ${c.requiredPlugins.join(", ")})${RESET}` : "";
    console.log(`    ${DIM}${String(i + 1).padStart(2)}.${RESET} ${c.label}${plugins}`);
  }

  console.log("");
  const disableInput = await ask(rl, `  ${BOLD}?${RESET} Disable (e.g. "6,8" or press Enter for none): `);

  const disabled: string[] = [];
  if (disableInput.trim()) {
    const indices = disableInput
      .split(",")
      .map((s) => parseInt(s.trim(), 10) - 1)
      .filter((i) => i >= 0 && i < optional.length);
    for (const i of indices) {
      disabled.push(optional[i].name);
    }
  }

  if (disabled.length > 0) {
    info(`Disabled: ${disabled.join(", ")}`);
  } else {
    info("All categories enabled");
  }
  console.log("");

  // 3. Determine required plugins
  const requiredPlugins = new Set(["PythonScriptPlugin"]);
  for (const cat of CATEGORIES) {
    if (disabled.includes(cat.name)) continue;
    if (cat.requiredPlugins) {
      for (const p of cat.requiredPlugins) requiredPlugins.add(p);
    }
  }

  // 4. Deploy C++ plugin
  const deployResult = deploy(project);
  if (deployResult.error) {
    fail(`Plugin deployment failed: ${deployResult.error}`);
  } else {
    if (deployResult.cppPluginDeployed) {
      ok(`Plugin deployed to ${project.projectName}/Plugins/UE_MCP_Bridge/`);
    } else {
      ok("Plugin already deployed");
    }
  }

  // 5. Enable required plugins
  const enabled = ensurePluginsEnabled(project.projectPath!, [...requiredPlugins]);
  if (enabled.length > 0) {
    ok(`Enabled: ${enabled.join(", ")}`);
  } else {
    ok("Required plugins already enabled");
  }

  // 6. Write .ue-mcp.json
  writeProjectConfig(project.projectDir!, disabled);
  ok(".ue-mcp.json written");

  console.log("");

  // 7. MCP client configuration
  const clients = detectMcpClients(project.projectDir!);
  const detected = clients.filter((c) => c.detected);

  if (detected.length > 0) {
    console.log(`  ${BOLD}?${RESET} MCP clients detected:`);
    for (let i = 0; i < detected.length; i++) {
      console.log(`    ${DIM}${i + 1}.${RESET} ${detected[i].name} ${DIM}(${detected[i].configPath})${RESET}`);
    }
    console.log(`    ${DIM}${detected.length + 1}.${RESET} Skip`);
    console.log("");

    const clientInput = await ask(rl, `  ${BOLD}?${RESET} Configure which? (e.g. "1" or "1,2" or Enter to skip): `);

    if (clientInput.trim()) {
      const indices = clientInput
        .split(",")
        .map((s) => parseInt(s.trim(), 10) - 1)
        .filter((i) => i >= 0 && i < detected.length);

      for (const i of indices) {
        writeMcpConfig(detected[i].configPath, project.projectPath!);
        ok(`${detected[i].name} config written`);
      }
    }
  } else {
    warn("No MCP clients detected. Add this to your MCP client config:");
    console.log("");
    console.log(`    ${DIM}{`);
    console.log(`      "mcpServers": {`);
    console.log(`        "ue-mcp": {`);
    console.log(`          "command": "npx",`);
    console.log(`          "args": ["ue-mcp", "${project.projectPath!.replace(/\\/g, "/")}"]`);
    console.log(`        }`);
    console.log(`      }`);
    console.log(`    }${RESET}`);
    console.log("");
  }

  rl.close();

  // 8. Done
  console.log("");
  console.log(`  ${BOLD}${GREEN}Setup complete!${RESET}`);
  console.log("");
  console.log(`  ${DIM}Restart the editor to load the bridge plugin.`);
  console.log(`  Then ask your AI: project(action="get_status")${RESET}`);
  console.log("");
}

init().catch((e) => {
  console.error(`\n  ${RED}Fatal error: ${e instanceof Error ? e.message : e}${RESET}\n`);
  process.exit(1);
});
