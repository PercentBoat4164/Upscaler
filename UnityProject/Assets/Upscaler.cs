// API TODOS
/*
    @todo Integrate Sharpness with API
    @todo Integrate Scale Factors to API (for Dynamic Manual Mode)
    @todo Integrate C++ Width and Height Scale Range API with Scale Factor API
    @todo Integrate Scale API with Scalable Buffer Manager (scales go from 0 to 1, but write something on top to make sure it doesn't exceed 0.5)
    @todo Show Readonly Field for Upscaler Status in Editor
    @todo ABSOLUTELY MASSIVE Backend DLSS Refactor In Order (productivity hurting)
    @todo DOCUMENTATION
*/

using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using AOT;
using UnityEngine;

[Serializable]
public class Upscaler : BackendUpscaler
{
    // EXPOSED API FEATURES
    
    // Callback Function that Will Run when an Error is Encountered by DLSS
    // Function will be passed Status (which contains error information) and Error Message String
    // This Function allows Developers to Determine what Should Happen when Upscaler Encounters an Error
    // This Function is Called the Frame After an Error, and it's changes take effect that frame
    // If the same error is encountered during multiple frames, the function is only called for the first frame
    public static Action<UpscalerStatus, string> ErrorCallback;

    // Basic Upscaler Options (Can be set In Editor or By Code)
    
    // Current Upscaling Mode to Use
    public Upscaler upscaler = Upscaler.DLSS;
    
    // Quality / Performance Mode for the Upscaler
    public Quality quality = Quality.DynamicAuto;
    
    // Upscale Factors for Width and Height Respectively (Only Used in Dynamic Manual Scaling)
    public uint widthScaleFactor;
    public uint heightScaleFactor;

    // The Currently Upscaler Status
    // Contains Error Information for Settings Errors or Internal Problems
    public UpscalerStatus Status => _internalStatus;

    // Read Only List that contains a List of Device/OS Supported Upscalers
    public IList<Upscaler> SupportedUpscalingModes => _supportedUpscalingModes;

    // Removes history so that artifacts from previous frames are not left over in DLSS
    // Should be called every time the scene sees a complete scene change
    public void ResetHistoryBuffer()
    {
        Upscaler_ResetHistory();
    }

    // Returns true if the device and operating system support the given upscaling mode
    // Returns false if device and OS do not support Upscaling Mode
    public bool DeviceSupportsUpscalingMode(Upscaler upscalingMode)
    {
        return _supportedUpscalingModes.Contains(upscalingMode);
    }
    
    // INTERNAL API IMPLEMENTATION
    
    // Internal Values for supported upscaling modes and upscaler status
    private IList<Upscaler> _supportedUpscalingModes;
    private UpscalerStatus _internalStatus;
    private UpscalerStatus _prevStatus;
    
    // Runs when the Script is enabled
    // Calls Base Class Method to Initialize Plugin and then uses Plugin Functions to gather information about device supported options
    public new void OnEnable()
    {
        base.OnEnable();

        var tempList = new List<Upscaler>();
        
        foreach (Upscaler tempUpscaler in Enum.GetValues(typeof(Upscaler)))
        {
            if (Upscaler_GetError(tempUpscaler) <= UpscalerStatus.NoUpscalerSet)
            {
                tempList.Add(tempUpscaler);
            }
        }

        _supportedUpscalingModes = tempList.AsReadOnly();
    }
    
    // Runs Before Culling; Validates Settings and Checks for Errors from Previous Frame Before Calling
    // Real OnPreCull Actions from Backend
    private new void OnPreCull()
    {
        // Check if an Internal Error was Caught on the Last Frame. Set current Upscaler to None and Alert if So
        if (InternalErrorFlag > UpscalerStatus.NoUpscalerSet)
        {
            Debug.LogError("Upscaler encountered an error. Reverting to No Upscaling. Message: " 
                           + Marshal.PtrToStringAuto(Upscaler_GetCurrentErrorMessage()));
            _internalStatus = InternalErrorFlag;
            upscaler = Upscaler.None;
        }
        // If an Internal Error was not Caught on the Previous Frame, check for settings changes
        else {
            // At this point, internal status is one frame old, and the previous status is two frames old
            // If there was a new error last frame, call the callback function before evaluating settings
            // This gives a chance for developers to catch an error one frame after it occurs via a callback
            if (_internalStatus > UpscalerStatus.NoUpscalerSet && _internalStatus!=_prevStatus && ErrorCallback!=null)
                ErrorCallback(_internalStatus, Marshal.PtrToStringAnsi(Upscaler_GetCurrentErrorMessage()));
            
            ValidateSettings();
        }

        _prevStatus = _internalStatus;
        base.OnPreCull();
    }

    // Checks For/Validates Settings Changes
    // Returns true if no changes/errors
    // Returns false if errors occurred
    private bool ValidateSettings()
    {
        // If the Active and Inactive Upscaler are the Same, Validate the Upscaler
        if (upscaler != ActiveUpscaler)
        {
            UpscalerStatus newModeError = Upscaler_GetError(upscaler);
            if (newModeError > UpscalerStatus.NoUpscalerSet)
            {
                upscaler = Upscaler.None;
                ActiveUpscaler = Upscaler.None;
                _internalStatus = newModeError;
                Debug.LogError("There was an error updating the upscaler. Reverting to No Upscaling. Message: " 
                               + Marshal.PtrToStringAuto(Upscaler_GetErrorMessage(upscaler)));
            }

            else
            {
                ActiveUpscaler = upscaler;
                _internalStatus = UpscalerStatus.Success; 
            }
        }

        if (quality != ActiveQuality)
        {
            ActiveQuality = quality;
        }
        
        return true;
    }
    
    [DllImport("GfxPluginDLSSPlugin")]
    protected static extern UpscalerStatus Upscaler_GetError(Upscaler upscaler);

    [DllImport("GfxPluginDLSSPlugin")]
    protected static extern IntPtr Upscaler_GetErrorMessage(Upscaler upscaler);

    [DllImport("GfxPluginDLSSPlugin")]
    protected static extern UpscalerStatus Upscaler_GetCurrentError();

    [DllImport("GfxPluginDLSSPlugin")]
    protected static extern IntPtr Upscaler_GetCurrentErrorMessage();

    [DllImport("GfxPluginDLSSPlugin")]
    private static extern void Upscaler_SetErrorCallback(InternalErrorCallback cb);
    private delegate void InternalErrorCallback(UpscalerStatus er, IntPtr p);

    [MonoPInvokeCallback(typeof(InternalErrorCallback))]
    private static void InternalErrorCallbackWrapper(UpscalerStatus reason, IntPtr message)
    {
        ErrorCallback(reason, Marshal.PtrToStringAnsi(message));
    }
}