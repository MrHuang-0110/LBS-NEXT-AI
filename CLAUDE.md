# CLAUDE.md

项目框架信息已移至 MCP Memory（`.memory/memory.jsonl`），每次会话自动加载。

## Memory system

This project uses **MCP Memory** (`@modelcontextprotocol/server-memory`) for persistent memory. Config: [.mcp.json](.mcp.json) and [.claude/settings.local.json](.claude/settings.local.json). Data lives in [.memory/memory.jsonl](.memory/memory.jsonl).

To initialize MCP Memory on a new project, run one of:
- `bash tools/init_memory.sh` (Git Bash / WSL)
- `tools\init_memory.bat` (Windows cmd, double-click)

## Agent skills

### Issue tracker

Issues are tracked via GitHub Issues. See `docs/agents/issue-tracker.md`.

### Triage labels

Default five-label vocabulary (needs-triage, needs-info, ready-for-agent, ready-for-human, wontfix). See `docs/agents/triage-labels.md`.

### Domain docs

Single-context layout: root-level CONTEXT.md + docs/adr/. See `docs/agents/domain.md`.