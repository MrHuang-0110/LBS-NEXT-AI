@echo off
REM 已改用 tools\mdk_deploy_prompt.py（由 Keil After Build 自动调用）
REM 保留本文件仅作兼容；请勿再配置为 After Build 第二步。
python -u "%~dp0tools\mdk_deploy_prompt.py" "%~dp0Output\atk_f103.axf"
exit /b %ERRORLEVEL%
