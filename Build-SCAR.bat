@echo off
setlocal
if not defined SKSE_TOOLCHAIN_ROOT set "SKSE_TOOLCHAIN_ROOT=%~dp0..\_toolchain"
call "%SKSE_TOOLCHAIN_ROOT%\Build-SKSE-Mod.cmd" "%~dp0." %*
exit /b %ERRORLEVEL%
