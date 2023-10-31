@echo off

:: Setting default values
SET UE_PATH=C:\Program Files\Epic Games\UE_5.3
SET OUTPUT_PATH=%USERPROFILE%\Documents

"%UE_PATH%\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin -plugin="%~dp0IOBuilderLiveLink.uplugin" -package="%OUTPUT_PATH%\IOBuilderLiveLink"