# v1.0.0-rc.3 Release Notes

Removes the `agent_prompt` flow task and the Anthropic SDK dependency.

## Removed

- **`agent_prompt` flow task** and every piece of scaffolding behind it: `AnthropicProvider`, the `LLMProvider` abstraction on `FlowContext`, the `llm` field on the flow context, and the `ANTHROPIC_API_KEY` env var path.
- **`@anthropic-ai/sdk`** dependency (4 packages dropped from `node_modules`).
- `docs/flows.md` sections and examples that referenced `agent_prompt` or the Anthropic-backed provider.

## Why

ue-mcp is an MCP server. Whatever is running it is already an LLM agent. `agent_prompt` meant the flow engine called out to a *second* LLM (direct Anthropic API, second API key, second model choice, second billing surface) from inside a step that was itself triggered by the first LLM. That's a round-trip for a string the parent agent would generate anyway when it saw the flow result.

The `on_failure: [agent_prompt]` auto-triage pattern was the clearest example: the agent triggers the flow, the flow fails, the flow calls sampling/Anthropic to ask the LLM to triage, returns a paragraph, and the agent reads that paragraph. The agent could have just read the failure directly.

Flows are deterministic pipelines over the UE bridge. Reasoning belongs in the parent agent. Mixing the two mechanisms confused the mental model and added a dep, a config knob, and a failure mode (bad key, wrong model) for a feature most users never ran.

Sampling via MCP (`server.createMessage`) wasn't kept as a replacement: it has the same round-trip problem, just over MCP instead of HTTPS, and it adds client-capability fragility on top.

## Migration

If a `ue-mcp.yml` flow has a step using `task: agent_prompt` (including inside `on_failure`), remove it. The parent agent sees the flow result directly and can act on it. `on_failure` hooks can still run deterministic cleanup tasks (`editor.execute_console`, `shell`, any bridge action).

No bridge or MCP tool changes. All 429 handlers unchanged.
