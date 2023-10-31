@echo off

:: Setting default values
SET DEFAULT_UE_PATH=C:\Program Files\Epic Games\UE_5.3
SET DEFAULT_PLUGIN_PATH=%USERPROFILE%\Documents\IOBuilderLiveLink

:: Using arguments if provided, else default values

SET PLUGIN_PATH=%1
IF "%PLUGIN_PATH%"=="" SET PLUGIN_PATH=%DEFAULT_PLUGIN_PATH%

SET UE_PATH=%2
IF "%UE_PATH%"=="" SET UE_PATH=%DEFAULT_UE_PATH%

"%UE_PATH%\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin -plugin="%~dp0IOBuilderLiveLink.uplugin" -package="%PLUGIN_PATH%\IOBuilderLiveLink"
