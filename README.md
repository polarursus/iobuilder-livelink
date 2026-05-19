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

## Lens distortion (optional)

The `LiveLink Camera` node in IO Builder can additionally stream a calibrated
lens model (from the Lens Calibration node) alongside the camera tracking. It
is **optional** — graphs without a lens connected behave exactly as before and
older plugin builds simply ignore it.

When lens data is present the source publishes a second LiveLink subject named
`<Camera> Lens` using the **IOBuilder Lens** role (Brown-Conrady / "Spherical"
distortion: `K1,K2,K3,P1,P2`, principal point `Cx,Cy`, focal length, FOV,
focus, aperture, plus sensor/resolution when available).

To apply it in Unreal (UE 5.3+, requires the **Camera Calibration** and
**Live Link** engine plugins, which this plugin enables):

1. Add a `Cine Camera Actor` and a **Lens Component** to it; set the Lens
   Component's distortion model to **Spherical** (or assign a Lens File that
   uses it).
2. Add a **Live Link Component Controller**, set its subject to
   `<Camera> Lens`; it picks the **IOBuilder Lens Controller**, which streams
   the live distortion state into the Lens Component each frame.
3. Keep the existing **Camera** subject bound for transform/FOV/focus as
   before — the two subjects work together.
