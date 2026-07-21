#!/usr/bin/env bash
# ============================================================================
# init_memory.sh — 一键初始化 MCP Memory 记忆系统
# 用法:
#   在当前项目:  bash tools/init_memory.sh
#   指定项目:    bash tools/init_memory.sh /d/OtherProject
# 效果: 创建 .mcp.json + .claude/settings.local.json + .memory/ 目录
# ============================================================================

set -e

# 支持传入目标路径，默认为当前目录
PROJECT_ROOT="${1:-$(pwd)}"
MEMORY_DIR="$PROJECT_ROOT/.memory"
CLAUD_DIR="$PROJECT_ROOT/.claude"
MCP_JSON="$PROJECT_ROOT/.mcp.json"
SETTINGS_JSON="$CLAUD_DIR/settings.local.json"

# 检查目标目录是否存在
if [ ! -d "$PROJECT_ROOT" ]; then
  echo "❌ 目录不存在: $PROJECT_ROOT"
  exit 1
fi

# 检测是否已有配置文件，避免覆盖
EXISTING_FILES=""
[ -f "$MCP_JSON" ]          && EXISTING_FILES="$EXISTING_FILES  $MCP_JSON\n"
[ -f "$SETTINGS_JSON" ]     && EXISTING_FILES="$EXISTING_FILES  $SETTINGS_JSON\n"
[ -d "$MEMORY_DIR" ]        && EXISTING_FILES="$EXISTING_FILES  $MEMORY_DIR/\n"

if [ -n "$EXISTING_FILES" ]; then
  echo "⚠️  以下文件已存在，不会覆盖："
  printf "$EXISTING_FILES"
  echo "如需重新初始化，请先手动删除后再运行。"
  exit 1
fi

# 创建目录
mkdir -p "$CLAUD_DIR" "$MEMORY_DIR"

# 生成 .mcp.json
# Windows 路径需要反斜杠给 cmd.exe 用
WIN_PATH=$(echo "$MEMORY_DIR" | sed 's|/|\\|g')
cat > "$MCP_JSON" <<EOF
{
  "mcpServers": {
    "memory": {
      "type": "stdio",
      "command": "cmd",
      "args": [
        "/c",
        "npx",
        "-y",
        "@modelcontextprotocol/server-memory"
      ],
      "env": {
        "MEMORY_FILE_PATH": "${WIN_PATH}\\memory.jsonl"
      }
    }
  }
}
EOF

# 生成 settings.local.json
cat > "$SETTINGS_JSON" <<EOF
{
  "enabledMcpjsonServers": [
    "memory"
  ]
}
EOF

echo "=========================================="
echo "✅ MCP Memory 已初始化"
echo "   目标: $PROJECT_ROOT"
echo "=========================================="
echo "  创建文件:"
echo "    $MCP_JSON"
echo "    $SETTINGS_JSON"
echo "    $MEMORY_DIR/"
echo ""
echo "  下次在此目录打开 Claude Code 时自动生效。"
echo "=========================================="