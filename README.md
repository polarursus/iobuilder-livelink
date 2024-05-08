## Installation

### Prerequisites 
Before you start, ensure that you have Visual Studio Community Edition installed corresponding to your Unreal Engine Version.

- **Follow Unreal Engine Official Documentation about the Installation.** 

    > Note: You don't need to do the "Recommended Settings" part!

  - **Unreal Engine 4.27**: 
    [Official Documentation](https://docs.unrealengine.com/4.27/en-US/ProductionPipelines/DevelopmentSetup/VisualStudioSetup/)

  - **Unreal Engine 5.3**: 
    [Official Documentation](https://docs.unrealengine.com/5.3/en-US/setting-up-visual-studio-development-environment-for-cplusplus-projects-in-unreal-engine/)

  - **Unreal Engine 5.4**: 
    [Official Documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/setting-up-visual-studio-development-environment-for-cplusplus-projects-in-unreal-engine?application_version=5.4)

  - **Tip**: You can select your exact engine version on the Documentation page (top left corner).

### Build the Plugin

1. Edit `build.bat` and set the following paths:

    - `UE_PATH`: Specify Unreal Engine's folder.
    - `OUTPUT_PATH`: Specify the Output folder, e.g., `[Your Unreal Project Path]\Plugins`.

    **Example:**
    ```
    UE_PATH=C:\Program Files\Epic Games\UE_5.3
    PLUGIN_PATH=C:\PluginOutput
    ```

2. Run `build.bat`

    - Execute the `build.bat` file. Allow some time for the Plugin to be built.

    > Note: You might need Administrator rights depending on the accessed paths
