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
using System.Runtime.InteropServices;
using UnityEngine;

[Serializable]
public class EnableDLSS : BackendDLSS
{
    private const int ERROR_TYPE_OFFSET = 30;
    private const int ERROR_CODE_OFFSET = 16;
    private const int ERROR_RECOVERABLE = 1;
    
    // [31-30] = Error type
    // [29-16] = Error code
    // [15-1] = RESERVED
    // [0] = Recoverable
    public enum UpscalerStatus : uint {
        UPSCALER_SET_SUCCESSFULLY                        = 0U,
        HARDWARE_ISSUE                                   = 1U << ERROR_TYPE_OFFSET,
        HARDWARE_ISSUE_DEVICE_EXTENSIONS_NOT_SUPPORTED   = HARDWARE_ISSUE | 1U << ERROR_CODE_OFFSET,
        HARDWARE_ISSUE_DEVICE_NOT_SUPPORTED              = HARDWARE_ISSUE | 2U << ERROR_CODE_OFFSET,
        SOFTWARE_ISSUE                                   = 2U << ERROR_TYPE_OFFSET,
        SOFTWARE_ISSUE_INSTANCE_EXTENSIONS_NOT_SUPPORTED = SOFTWARE_ISSUE | 1U << ERROR_CODE_OFFSET,
        SOFTWARE_ISSUE_DEVICE_DRIVERS_OUT_OF_DATE        = SOFTWARE_ISSUE | 2U << ERROR_CODE_OFFSET,
        SOFTWARE_ISSUE_OPERATING_SYSTEM_NOT_SUPPORTED    = SOFTWARE_ISSUE | 3U << ERROR_CODE_OFFSET,
        SOFTWARE_ISSUE_INVALID_WRITE_PERMISSIONS         = SOFTWARE_ISSUE | 4U << ERROR_CODE_OFFSET,
        SOFTWARE_ISSUE_FEATURE_DENIED                    = SOFTWARE_ISSUE | 5U << ERROR_CODE_OFFSET,
        SOFTWARE_ISSUE_OUT_OF_GPU_MEMORY                 = SOFTWARE_ISSUE | 6U << ERROR_CODE_OFFSET,
        SETTINGS_ISSUE                                   = 3U << ERROR_TYPE_OFFSET | ERROR_RECOVERABLE,
        SETTINGS_ISSUE_INPUT_RESOLUTION_TOO_SMALL        = SETTINGS_ISSUE | 1U << ERROR_CODE_OFFSET,
        SETTINGS_ISSUE_INPUT_RESOLUTION_TOO_BIG          = SETTINGS_ISSUE | 2U << ERROR_CODE_OFFSET,
        SETTINGS_ISSUE_UPSCALER_NOT_AVAILABLE            = SETTINGS_ISSUE | 3U << ERROR_CODE_OFFSET,
        SETTINGS_ISSUE_QUALITY_MODE_NOT_AVAILABLE        = SETTINGS_ISSUE | 4U << ERROR_CODE_OFFSET,
    }

    private UpscalerStatus _internalStatus;
    
    public Type upscaler = Type.None;
    public UpscalerStatus Status { get => _internalStatus; }

    
    private new void OnPreCull()
    {
        var programStatus = (UpscalerStatus) Upscaler_GetCurrentError();

        if (programStatus != 0)
        {
            Debug.LogError("Upscaler encountered an error. Reverting to No Upscaling. Message: " 
                           + Marshal.PtrToStringAuto(Upscaler_GetCurrentErrorMessage()));
            base.ActiveUpscaler = Type.None;
            _internalStatus = programStatus;
        }
        else
        {
            _internalStatus = ValidateSettings();
        }
        
        base.OnPreCull();
    }
    
    // Every frame where the screen sees a complete change,
    // the history buffer should be reset so that artifacts from the previous screen are not left behind
    public void ResetHistoryBuffer()
    {
        
    }

    public bool DeviceSupportsUpscalingMode(Type upscalingMode)
    {
        return Upscaler_GetError(upscalingMode) == 0;
    }

    private UpscalerStatus ValidateSettings()
    {
        if (upscaler == ActiveUpscaler)
        {
            return UpscalerStatus.UPSCALER_SET_SUCCESSFULLY;
        }
        
        UpscalerStatus newModeError = (UpscalerStatus) Upscaler_GetError(upscaler);

        if (newModeError != 0)
        {
            Debug.LogError("Error setting new upscaler. Reverting to No Upscaling. Message: " 
                           + Marshal.PtrToStringAuto(Upscaler_GetErrorMessage(upscaler)));
            ActiveUpscaler = Type.None;
            return newModeError;
        }

        Debug.Log("Successfully set new upscaler.");
        ActiveUpscaler = upscaler;
        return UpscalerStatus.UPSCALER_SET_SUCCESSFULLY;
    }
}