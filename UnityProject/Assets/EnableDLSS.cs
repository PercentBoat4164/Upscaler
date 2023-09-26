
// USER NEEDS TO BE ABLE TO:
// Set the upscaler
// Set the performance / quality mode
// Set scaling of render resolution to reach output resolution (only used in DYNAMIC_MANUAL)
// Set the 'reset history buffer' bit for this frame
// Set sharpness values (Should be 0 because DLSS sharpness is deprecated)
// Set HDR values (Exposure / auto exposure)*
// Validate settings
// Detect if and why upscaling has failed or is unavailable
// * = for later

using System;
using UnityEngine;
using UnityEngine.Serialization;

[Serializable]
public class EnableDLSS : BackendDLSS
{
    public enum UpscalerSettingsStatus
    {
        UPSCALER_SET_SUCCESSFULLY,
        ERROR_DEVICE_NOT_SUPPORTED
    }
    
    public Type upscaler = Type.NONE; 
    public UpscalerSettingsStatus UpscalerStatus { get => _internalStatus; }

    private UpscalerSettingsStatus _internalStatus;
    
    private new void OnPreRender()
    {
        _internalStatus = ValidateSettings();

        if (_internalStatus != UpscalerSettingsStatus.UPSCALER_SET_SUCCESSFULLY)
        {
            Debug.LogError("Upscaling Failed. Defaulting to No Upscaling. Reason: " + _internalStatus);
            ActiveUpscaler = Type.NONE;
        }
        
        base.OnPreRender();
    }
    
    // Every frame where the screen sees a complete change,
    // the history buffer should be reset so that artifacts from the previous screen are not left behind
    public void ResetHistoryBuffer()
    {

    }

    public bool DeviceSupportsUpscalingMode(Type upscalingMode)
    {
        return Upscaler_GetError(upscalingMode);
    }

    private UpscalerSettingsStatus ValidateSettings()
    {
        if (upscaler != ActiveUpscaler)
        {
            if (!Upscaler_GetError(upscaler))
            {
                return UpscalerSettingsStatus.ERROR_DEVICE_NOT_SUPPORTED;
            }
            ActiveUpscaler = upscaler;
        }

        return UpscalerSettingsStatus.UPSCALER_SET_SUCCESSFULLY;
    }
}