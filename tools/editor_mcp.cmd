@echo off
rem The Underworld Editor's MCP server (O:\UltimaUnderworldEditorAvalonia\src\UwEditor.Mcp).
rem   editor_mcp build   builds it (Release)
rem   editor_mcp test    runs its tests on copies of work\testlevel\gamedata\UW
rem   editor_mcp apptest the app's Let Claude work here test (headless)
rem   editor_mcp         runs the server on stdio, only on work\testlevel\game
rem   editor_mcp app     passes stdio through to the editor app (its Level menu, Let Claude work here)
rem Through dotnet.exe on O: (Defender's ASR rule blocks a new exe); nothing on C:.
set DOTNET_CLI_HOME=O:\appsnotonc\dotnet10\home
set NUGET_PACKAGES=O:\appsnotonc\dotnet10\home\.nuget\packages
set DOTNET_CLI_TELEMETRY_OPTOUT=1
set DOTNET_NOLOGO=1
set ED=O:\UltimaUnderworldEditorAvalonia
set DN=O:\appsnotonc\dotnet10\dotnet.exe
if "%1"=="build" "%DN%" build "%ED%\src\UwEditor.Mcp.Server" -c Release -nologo -v q & goto :eof
if "%1"=="apptest" set TESTS=%ED%\tests\UwEditor.App.Tests& set FILTER=--filter FullyQualifiedName~McpWindow
if "%1"=="test" set TESTS=%ED%\tests\UwEditor.Mcp.Tests& set FILTER=
if defined TESTS (
  set UW_GAMEDATA=O:\uw\uo-new\work\testlevel\gamedata
  set UW_MCP_TMP=O:\uw\uo-new\work\testlevel\tmp
  if not exist O:\uw\uo-new\work\testlevel\tmp mkdir O:\uw\uo-new\work\testlevel\tmp
  set TMP=O:\uw\uo-new\work\testlevel\tmp
  set TEMP=O:\uw\uo-new\work\testlevel\tmp
  "%DN%" test "%TESTS%" -c Release -nologo %FILTER% %2 %3 %4
  goto :eof
)
if "%1"=="app" "%DN%" "%ED%\src\UwEditor.Mcp.Server\bin\Release\net10.0\UwEditor.Mcp.Server.dll" --app & goto :eof
"%DN%" "%ED%\src\UwEditor.Mcp.Server\bin\Release\net10.0\UwEditor.Mcp.Server.dll" --only O:\uw\uo-new\work\testlevel\game
