Installation

1, Prerequisites - Before you start, ensure that you have Visual Studio Community Edition installed corresponding to your Unreal Engine Version:
-

Follow Unreal Engine Official Documentation about the Installation.

You don't need to do the "Recommended Settings" part!

Unreal Engine 4

https://docs.unrealengine.com/4.27/en-US/ProductionPipelines/DevelopmentSetup/VisualStudioSetup/

Unreal Engine 5

https://docs.unrealengine.com/5.3/en-US/setting-up-visual-studio-development-environment-for-cplusplus-projects-in-unreal-engine/

You can select your exact engine version on the Documentation page (top left corner).

2, Run build.bat:
-

Option 1:
-
Edit build.bat and set Default Paths:

DEFAULT_UE_PATH - Specify Unreal Engine's folder

DEFAULT_PLUGIN_PATH - Specify the Output folder eg. [Your Unreal Project Path]\Plugins

Example:

DEFAULT_UE_PATH=C:\Program Files\Epic Games\UE_5.3

DEFAULT_PLUGIN_PATH=C:\PluginOutput

Option 2:
-

Run build.bat with specified arguments

Example:

build.bat "C:\PluginOutput" "C:\Program Files\Epic Games\UE_5.3"

Depending on your system's security settings, you may need to provide administrator rights for the build process to proceed.
