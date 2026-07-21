@echo off
REM ============================================================================
REM init_memory.bat — 一键初始化 MCP Memory 记忆系统 (Windows)
REM 用法:
REM   在当前项目:  tools\init_memory.bat
REM   指定项目:    tools\init_memory.bat D:\OtherProject
REM 效果: 创建 .mcp.json + .claude\settings.local.json + .memory\ 目录
REM ============================================================================

setlocal enabledelayedexpansion

if "%1"=="" (
    set "PROJECT_ROOT=%CD%"
) else (
    set "PROJECT_ROOT=%~1"
)

if not exist "%PROJECT_ROOT%" (
    echo [ERROR] 目录不存在: %PROJECT_ROOT%
    pause
    exit /b 1
)

set "MEMORY_DIR=%PROJECT_ROOT%\.memory"
set "CLAUD_DIR=%PROJECT_ROOT%\.claude"
set "MCP_JSON=%PROJECT_ROOT%\.mcp.json"
set "SETTINGS_JSON=%CLAUD_DIR%\settings.local.json"

REM 检测是否已有配置文件
set "EXISTING="
if exist "%MCP_JSON%"      set "EXISTING=1"
if exist "%SETTINGS_JSON%" set "EXISTING=1"
if exist "%MEMORY_DIR%\"   set "EXISTING=1"

if defined EXISTING (
    echo [WARNING] 以下文件已存在，不会覆盖：
    if exist "%MCP_JSON%"      echo   %MCP_JSON%
    if exist "%SETTINGS_JSON%" echo   %SETTINGS_JSON%
    if exist "%MEMORY_DIR%\"   echo   %MEMORY_DIR%\
    echo 如需重新初始化，请先手动删除后再运行。
    pause
    exit /b 1
)

REM 创建目录
if not exist "%CLAUD_DIR%"  mkdir "%CLAUD_DIR%"
if not exist "%MEMORY_DIR%" mkdir "%MEMORY_DIR%"

REM 生成 .mcp.json（路径用反斜杠）
(
echo {
echo   "mcpServers": {
echo     "memory": {
echo       "type": "stdio",
echo       "command": "cmd",
echo       "args": [
echo         "/c",
echo         "npx",
echo         "-y",
echo         "@modelcontextprotocol/server-memory"
echo       ],
echo       "env": {
echo         "MEMORY_FILE_PATH": "%MEMORY_DIR:\=\\%\\memory.jsonl"
echo       }
echo     }
echo   }
echo }
) > "%MCP_JSON%"

REM 生成 settings.local.json
(
echo {
echo   "enabledMcpjsonServers": [
echo     "memory"
echo   ]
echo }
) > "%SETTINGS_JSON%"

echo ==========================================
echo [OK] MCP Memory 已初始化
echo     目标: %PROJECT_ROOT%
echo ==========================================
echo  创建文件:
echo    %MCP_JSON%
echo    %SETTINGS_JSON%
echo    %MEMORY_DIR%\
echo.
echo  下次在此目录打开 Claude Code 时自动生效。
echo ==========================================
pause