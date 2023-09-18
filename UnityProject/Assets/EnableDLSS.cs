
// USER NEEDS TO BE ABLE TO:
// Set the upscaler
// Set the performance / quality mode
// Set the render resolution
// Set the output resolution
// Set the 'reset history buffer' bit for this frame
// Set sharpness values (Should be 0 because DLSS sharpness is deprecated)
// Set HDR values (Exposure / auto exposure)*
// Validate settings
// Detect if and why upscaling has failed or is unavailable
// * = for later

using System;

[System.Serializable]
public class EnableDLSS : BackendDLSS
{
    public Type upscaler = Type.DLSS;

    // Every frame where the screen sees a complete change,
    // the history buffer should be reset so that artifacts from the previous screen are not left behind
    public void ResetHistoryBuffer()
    {
        
    }
    
    public bool DeviceSupportsUpscalingMode(Type upscalingMode)
    {
        return Upscaler_IsSupported(upscalingMode);
    }
}