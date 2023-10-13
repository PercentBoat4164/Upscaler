// Final API Touches
/*
 * @todo Ensure Callback working with Backend Properly at Each Point in Pipeline (Need to Explicitly Call? Called Automatically? Different Cases? Ensure with Thadd how that's going to work)
 * @todo Ask Thadd about off by 1 error (DLSS works in dynamic manual even when value is supposedly too low)
 */

using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using AOT;
using UnityEngine;
using UnityEngine.Serialization;

[Serializable]
public class Upscaler : BackendUpscaler
{
    // EXPOSED API FEATURES
    
    // BASIC UPSCALER OPTIONS
    // Can Be Set In Editor Or By Code
    
    // Current Upscaling Mode to Use
    public Upscaler upscaler = Upscaler.DLSS;
    
    // Quality / Performance Mode for the Upscaler
    public Quality quality = Quality.DynamicAuto;
    
    // ADVANCED UPSCALER OPTIONS
    // Can Be Set In Editor Or By Code
    
    // Sharpness (Technically DLSS Deprecated); Ranges from 0 to 1
    public float sharpness = 0;
    private float _activeSharpness = 0;
    
    // Upscale Factors for Width and Height Respectively (Only Used in Dynamic Manual Scaling)
    public float widthScaleFactor = 0.9f;
    public float heightScaleFactor = 0.9f;
    
    // Strictly Code Accessible Endpoints

    // Bounds for Scale Factor Obtained from the Plugin
    public float MinWidthScaleFactor => MinWidthScale;
    public float MinHeightScaleFactor => MinHeightScale;
    public float MaxWidthScaleFactor => MaxWidthScale;
    public float MaxHeightScaleFactor => MaxHeightScale;
    
    // Callback Function that Will Run when an Error is Encountered by DLSS
    // Function will be passed Status (which contains error information) and Error Message String
    // This Function allows Developers to Determine what Should Happen when Upscaler Encounters an Error
    // This Function is Called the Frame After an Error, and it's changes take effect that frame
    // If the same error is encountered during multiple frames, the function is only called for the first frame
    public Action<UpscalerStatus, string> ErrorCallback;
    
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
    private ulong _prevMaxRes, _prevMinRes;
    
    // Runs when the Script is enabled
    // Calls Base Class Method to Initialize Plugin and then uses Plugin Functions to gather information about device supported options
    public new void OnEnable()
    {
        Upscaler_SetErrorCallback(InternalErrorCallbackWrapper);
        
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
        if (ChangeInSettings())
        {
            //If there are settings changes, Validate and Push them (this includes a potential call to Error Callback) 
            var settingsChange = ValidateAndPushSettings();
        
            // If Settings Change Caused Error, pass to Internal Error Handler
            if (settingsChange.Item1 > UpscalerStatus.NoUpscalerSet)
            {
                InternalErrorHandler(settingsChange.Item1, settingsChange.Item2);
            }
        }

        // If the upscaler is active and the quality mode is Dynamic, ensure dynamic resolution is enabled
        if (!_camera.allowDynamicResolution && ActiveUpscaler != Upscaler.None && 
            (ActiveQuality == Quality.DynamicAuto) || (ActiveQuality == Quality.DynamicManual)) 
        {
            _camera.allowDynamicResolution = true;
        }
        
        // If the Upscaler is not None, the Quality Mode does not allow for Dynamic Resolution, and the Camera is set
        // to dynamic resolution, turn Dynamic Resolution off and give a warning
        if (ActiveUpscaler != Upscaler.None && ActiveQuality != Quality.DynamicAuto &&
            ActiveQuality != Quality.DynamicManual && _camera.allowDynamicResolution)
        {
            _camera.allowDynamicResolution = false;
            Debug.LogWarning("Dynamic resolution has been disabled for the upscaled camera " +
                             "since the current upscaling and quality modes do not allow for it.");
        }

        // If the quality mode is dynamic and min/max scale changed, alter bounds on scale factor and reset scale factor
        if (ActiveQuality == Quality.DynamicAuto || ActiveQuality == Quality.DynamicManual)
        {
            var maxres = Upscaler_GetMaximumInputResolution();
            if (maxres != _prevMaxRes)
            {
                var maxWidth = (uint)(maxres >> 32);
                var maxHeight = (uint)(maxres & 0xFFFFFFFF);

                MaxHeightScale = (float)maxHeight / _camera.pixelHeight;
                MaxWidthScale = (float)maxWidth / _camera.pixelWidth;
                
                _prevMaxRes = maxres;
                
                ActiveUpscalingFactorWidth = ActiveUpscalingFactorWidth;
                ActiveUpscalingFactorHeight = ActiveUpscalingFactorHeight;
                widthScaleFactor = ActiveUpscalingFactorWidth;
                heightScaleFactor = ActiveUpscalingFactorHeight;
            }
            
            var minres = Upscaler_GetMinimumInputResolution();
            if (minres != _prevMinRes)
            {
                var minWidth = (uint)(minres >> 32);
                var minHeight = (uint)(minres & 0xFFFFFFFF);

                MinHeightScale = (float)minHeight / _camera.pixelHeight;
                MinWidthScale = (float)minWidth / _camera.pixelWidth;
                
                _prevMinRes = minres;

                ActiveUpscalingFactorWidth = ActiveUpscalingFactorWidth;
                ActiveUpscalingFactorHeight = ActiveUpscalingFactorHeight;
                widthScaleFactor = ActiveUpscalingFactorWidth;
                heightScaleFactor = ActiveUpscalingFactorHeight;
            }
        }
        
        base.OnPreCull();
    }
    
    // Shows if Settings have Changed since Last Checked
    // No internal change should ever reflect a settings 'change'
    // This will only return true when a user change causes settings to fall out of sync between Upscaler and BackendUpscaler
    private bool ChangeInSettings()
    {
        if (upscaler != ActiveUpscaler)
        {
            if (_camera.allowMSAA && upscaler != Upscaler.None)
            {
                Debug.LogWarning("MSAA should not be turned on for cameras that have upscaling applied to them. " +
                                 "It is not necessary and not performant. It could also cause image artifacting.");
            }
            return true;
        }

        return quality != ActiveQuality || !sharpness.Equals(_activeSharpness)
                                        || !widthScaleFactor.Equals(ActiveUpscalingFactorWidth) 
                                        || !heightScaleFactor.Equals(ActiveUpscalingFactorHeight);
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
        
        // Check Sharpness Value for Change
        // Only Warn if Error and Revert Setting; No Error for Small Setting
        if (!sharpness.Equals(_activeSharpness))
        {
            if (sharpness < 0 || sharpness > 1)
            {
                Debug.LogError("Sharpness value must be between 0 and 1. Sharpness reverted to previous value.");
                sharpness = _activeSharpness;
            }
            else
            {
                _activeSharpness = sharpness;
                Upscaler_SetSharpnessValue(_activeSharpness);
            }
        }
        
        // Propagate Changes to Backend If No Errors In Changing Settings
        ActiveUpscaler = upscaler;
        ActiveQuality = quality;

        if (!widthScaleFactor.Equals(ActiveUpscalingFactorWidth) || !heightScaleFactor.Equals(ActiveUpscalingFactorHeight))
        {
            ActiveUpscalingFactorWidth = widthScaleFactor;
            ActiveUpscalingFactorHeight = heightScaleFactor;
            
            if (!ActiveUpscalingFactorWidth.Equals(widthScaleFactor))
            {
                Debug.LogWarning("The Width Scale Factor Provided is outside of the proper range. It has been" +
                                 "clipped to: " + ActiveUpscalingFactorWidth);
                widthScaleFactor = ActiveUpscalingFactorWidth;
            }
            
            if (!ActiveUpscalingFactorHeight.Equals(heightScaleFactor))
            {
                Debug.LogWarning("The Height Scale Factor Provided is outside of the proper range. It has been" +
                                 "clipped to: " + ActiveUpscalingFactorHeight);
                heightScaleFactor = ActiveUpscalingFactorHeight;
            }
            
            if (ActiveQuality != Quality.DynamicManual)
            {
                Debug.LogWarning("Changing the upscaling factor manually only applies when the " +
                                 "Quality Mode is Dynamic Manual. The value has been changed but will have no effect.");
            }
        }
        
        // Get Proper Success Status and Set Internal Status to match
        var stat = (upscaler == Upscaler.None) ? UpscalerStatus.NoUpscalerSet : UpscalerStatus.Success;
        _internalStatus = stat;
        
        // Return some success status if no settings errors were encountered
        return new(stat, "Upscaler Settings Updated Successfully");
    }

    // If the program ever encounters some kind of error, this will be called
    // Rendering errors call this through InternalErrorCallbackWrapper
    // Settings errors call this from ValidateAndPushSettings
    private void InternalErrorHandler(UpscalerStatus reason, String message, bool resetTargets = false)
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
                upscaler = Upscaler.None;
                ActiveUpscaler = Upscaler.None;
            }
            
            Debug.LogError("Upscaler encountered an Error, but it was fixed by callback. Original Error:\n" + message);
            
            
        }
    }
    
    [DllImport("GfxPluginDLSSPlugin")]
    private static extern UpscalerStatus Upscaler_SetSharpnessValue(float sharpness);
    
    private delegate void InternalErrorCallback(UpscalerStatus error, IntPtr message);

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
    
    [DllImport("GfxPluginDLSSPlugin")]
    private static extern ulong Upscaler_GetMinimumInputResolution();

    [DllImport("GfxPluginDLSSPlugin")]
    private static extern ulong Upscaler_GetMaximumInputResolution();
}