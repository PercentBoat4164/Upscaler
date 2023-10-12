// API TODOS
/*
    @todo Integrate Sharpness with API
    @todo Integrate Scale Factors to API (for Dynamic Manual Mode)
    @todo Integrate C++ Width and Height Scale Range API with Scale Factor API
    @todo Integrate Scale API with Scalable Buffer Manager (scales go from 0 to 1, but write something on top to make sure it doesn't exceed 0.5)
    @todo Ensure Callback working with Backend Properly at Each Point in Pipeline (Need to Explicitly Call? Called Automatically?)
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
    public Action<UpscalerStatus, string> ErrorCallback;
    
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
    public UpscalerStatus Status { get => _internalStatus; }

    // Read Only List that contains a List of Device/OS Supported Upscalers
    public IList<Upscaler> SupportedUpscalingModes { get => _supportedUpscalingModes; }
    
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
        if (!ChangeInSettings())
        {
            base.OnPreCull();
            return;
        }

        //If there are settings changes, Validate and Push them (this includes a potential call to Error Callback) 
        var settingsChange = ValidateAndPushSettings();
            
        // If Settings Change Caused Error, pass to Internal Error Handler
        if (settingsChange.Item1 > UpscalerStatus.NoUpscalerSet)
        {
            InternalErrorHandler(settingsChange.Item1, settingsChange.Item2);
        }
        
        base.OnPreCull();
    }

    // Shows if Settings have Changed since Last Checked
    // No internal change should ever reflect a settings 'change'
    // This will only return true when a user change causes settings to fall out of sync between Upscaler and BackendUpscaler
    private bool ChangeInSettings()
    {
        return upscaler != ActiveUpscaler || quality != ActiveQuality;
    }
    
    // Validates Settings Changes and Push them to BackendUpscaler so they Take Effect if No Issue Met
    // Status with new settings will be returned
    // Returns a Tuple containing New Internal Status as well as a Message About Settings Change
    private Tuple<UpscalerStatus, String> ValidateAndPushSettings()
    {
        String errorMsg = "There was an Error in Changing Upscaler Settings. Details:\n";
        
        // Check for Lack of Support for Currently Selected Upscaler
        UpscalerStatus settingsError = Upscaler_GetError(upscaler);
        if (settingsError > UpscalerStatus.NoUpscalerSet)
        {
            errorMsg += "Invalid Upscaler: " + 
                           Marshal.PtrToStringAuto(Upscaler_GetErrorMessage(upscaler)) + "\n";
            _internalStatus = settingsError;
            return new Tuple<UpscalerStatus, string>(settingsError, errorMsg);
        }
        
        // Propagate Changes to Backend If No Errors In Changing Settings
        ActiveUpscaler = upscaler;
        ActiveQuality = quality;

        // Get Proper Success Status and Set Internal Status to match
        var stat = (upscaler == Upscaler.None) ? UpscalerStatus.NoUpscalerSet : UpscalerStatus.Success;
        _internalStatus = stat;
        
        // Return some success status if no settings errors were encountered
        return new Tuple<UpscalerStatus, string>(stat, "Upscaler Settings Updated Successfully");
    }

    // If the program ever encounters some kind of error, this will be called
    // Rendering errors call this through InternalErrorCallbackWrapper
    // Settings errors call this from ValidateAndPushSettings
    private void InternalErrorHandler(UpscalerStatus reason, String message)
    {
        if (ErrorCallback is null)
        {
            Debug.LogError(message + "\nNo error callback. Reverting upscaling to None.");
            upscaler = Upscaler.None;
            ActiveUpscaler = Upscaler.None;
        }
        else
        {
            ErrorCallback(reason, message);
            
            if (!ChangeInSettings())
            {
                Debug.LogError(message + "\nError callback did not make any settings changes. Reverting Upscaling to None.");
                upscaler = Upscaler.None;
                ActiveUpscaler = Upscaler.None;
            }

            var settingsChangeEffect = ValidateAndPushSettings();
            if (settingsChangeEffect.Item1 > UpscalerStatus.NoUpscalerSet)
            {
                Debug.LogError("Original Error:\n" + message + "\nCallback attempted to fix settings " +
                               "but failed. New Error:\n" + settingsChangeEffect.Item2);
            }
            
            Debug.LogError("Upscaler encountered an Error, but it was fixed by callback. Original Error:\n" + message);
        }
    }
    
    private delegate void InternalErrorCallback(UpscalerStatus error, String message);

    [MonoPInvokeCallback(typeof(InternalErrorCallback))]
    private void InternalErrorCallbackWrapper(UpscalerStatus reason, IntPtr message)
    {
        InternalErrorHandler(reason,
            "Error was encountered while upscaling. Details: " + Marshal.PtrToStringAnsi(message) + "\n");
    }

    [DllImport("GfxPluginDLSSPlugin")]
    private static extern void Upscaler_SetErrorCallback(InternalErrorCallback cb);
    
    [DllImport("GfxPluginDLSSPlugin")]
    protected static extern UpscalerStatus Upscaler_GetError(Upscaler upscaler);

    [DllImport("GfxPluginDLSSPlugin")]
    protected static extern IntPtr Upscaler_GetErrorMessage(Upscaler upscaler);

    [DllImport("GfxPluginDLSSPlugin")]
    protected static extern UpscalerStatus Upscaler_GetCurrentError();

    [DllImport("GfxPluginDLSSPlugin")]
    protected static extern IntPtr Upscaler_GetCurrentErrorMessage();
}