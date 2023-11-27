using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using AOT;
using JetBrains.Annotations;
using UnityEngine;

[Serializable]
public class Upscaler : BackendUpscaler
{
    // EXPOSED API FEATURES

    // BASIC UPSCALER OPTIONS
    // Can Be Set In Editor Or By Code

    /// Current Upscaling Mode to Use
    public Plugin.Mode upscaler = Plugin.Mode.DLSS;

    /// Quality / Performance Mode for the Upscaler
    public Plugin.Quality quality = Plugin.Quality.DynamicAuto;

    // ADVANCED UPSCALER OPTIONS
    // Can Be Set In Editor Or By Code

    /// Sharpness (Technically DLSS Deprecated); Ranges from 0 to 1
    public float sharpness;

    // Must remain separate for niceness in the Editor
    /// According to the DLSS documentation:
    ///  > To guarantee the accuracy of the DLSS image reconstruction, the aspect ratio of the render size must stay
    ///  > constant with [that of] the final display size.
    /// This is the render scale (Only Used in Dynamic Manual Scaling).
    /// It is the factor by which the output resolution must be multiplied to get the rendering resolution.
    public float renderScale = 1f;
    private float _lastRenderScale;

    // Strictly Code Accessible Endpoints

    /// Callback function that will run when an error is encountered by the upscaler.
    /// It will be passed status (which contains error information) and a more detailed error message string.
    /// This function allows developers to determine what should happen when upscaler encounters an error
    /// This function is called the frame after an error, and it's changes take effect that frame
    /// If the same error is encountered during multiple frames, the function is only called for the first frame
    [CanBeNull] public Action<Plugin.UpscalerStatus, string> errorCallback;

    /// The Currently Upscaler Status
    /// Contains Error Information for Settings Errors or Internal Problems
    public Plugin.UpscalerStatus Status { get; private set; }

    /// Read Only List that contains a List of Device/OS Supported Upscalers
    public IList<Plugin.Mode> SupportedUpscalingModes { get; private set; }

    /// Removes history so that artifacts from previous frames are not left over in DLSS
    /// Should be called every time the scene sees a complete scene change
    public void ResetHistoryBuffer() => Plugin.ResetHistory();

    /// Returns true if the device and operating system support the given upscaling mode
    /// Returns false if device and OS do not support Upscaling Mode
    public bool DeviceSupportsUpscalingMode(Plugin.Mode upscalingMode) => SupportedUpscalingModes.Contains(upscalingMode);

    // INTERNAL CAMERA OPTION HISTORY
    private bool _lastAllowDynamicResolution;
    private bool _lastAllowMSAA;

    // INTERNAL API IMPLEMENTATION

    // Runs when the Script is enabled
    // Calls Base Class Method to Initialize Plugin and then uses Plugin Functions to gather information about device supported options
    public void OnEnable()
    {
        Setup((IntPtr)GCHandle.Alloc(this), InternalErrorCallbackWrapper);

        ScalableBufferManager.ResizeBuffers(1f, 1f);

        SupportedUpscalingModes = Enum.GetValues(typeof(Plugin.Mode)).Cast<Plugin.Mode>().Where(
              tempUpscaler => Plugin.Success(Plugin.GetError(tempUpscaler))
            ).ToList().AsReadOnly();
    }

    /// Runs Before Culling; Validates Settings and Checks for Errors from Previous Frame Before Calling
    /// Real OnPreCull Actions from Backend
    protected override void OnPreCull()
    {
        if (!Application.isPlaying) return;

        if (ChangeInSettings())
        {
            //If there are settings changes, Validate and Push them (this includes a potential call to Error Callback)
            var settingsChange = ValidateAndPushSettings();

            // If Settings Change Caused Error, pass to Internal Error Handler
            if (Plugin.Failure(settingsChange.Item1))
                InternalErrorHandler(settingsChange.Item1, settingsChange.Item2);
        }
        base.OnPreCull();
    }

    /// Shows if settings have changed since last checked
    /// No internal change should ever reflect a settings 'change'
    /// This will only return true when a user change causes settings to fall out of sync between Upscaler and BackendUpscaler
    private bool ChangeInSettings()
    {
        return upscaler != ActiveMode || quality != ActiveQuality || !sharpness.Equals(LastSharpness) || !renderScale.Equals(_lastRenderScale) || _lastAllowDynamicResolution != Camera.allowDynamicResolution || _lastAllowMSAA != Camera.allowMSAA;
    }

    /// Validates settings changes and pushes them to BackendUpscaler so they take effect if no issue met.
    /// Status with new settings will be returned.
    /// Returns a Tuple containing new internal UpscalerStatus as well as a message about settings change.
    private Tuple<Plugin.UpscalerStatus, string> ValidateAndPushSettings()
    {
        // Check for lack of support for currently selected upscaler.
        var settingsError = Plugin.GetError(upscaler);
        if (Plugin.Failure(settingsError))
        {
            var errorMsg = "There was an Error in Changing Upscaler Settings. Details:\n";
            errorMsg += "Invalid Upscaler: " + Marshal.PtrToStringAnsi(Plugin.GetErrorMessage(upscaler)) + "\n";
            Status = settingsError;
            return new Tuple<Plugin.UpscalerStatus, string>(settingsError, errorMsg);
        }

        // Propagate changes to backend if no errors when changing settings.
        ActiveMode = upscaler;
        LastSharpness = Sharpness;
        Sharpness = sharpness;

        // MSAA is not compatible with upscaling.
        if (Camera.allowMSAA && upscaler != Plugin.Mode.None)
            Camera.allowMSAA = false;

        _lastAllowMSAA = Camera.allowMSAA;

        // Dynamic resolution
        if (upscaler != Plugin.Mode.None)
        {
            if (_lastAllowDynamicResolution != Camera.allowDynamicResolution)
                quality = Camera.allowDynamicResolution ? Plugin.Quality.DynamicAuto : Plugin.Quality.Auto;
            else
                Camera.allowDynamicResolution = Plugin.IsDynamicResolutionEnabled(quality);

            if (_lastAllowDynamicResolution != Camera.allowDynamicResolution)
                if (Camera.allowDynamicResolution) renderScale = 1f;
                else ScalableBufferManager.ResizeBuffers(1f, 1f);

            _lastAllowDynamicResolution = Camera.allowDynamicResolution;
        }

        ActiveQuality = quality;

        // Manual dynamic resolution
        if (ActiveQuality == Plugin.Quality.DynamicManual)
        {
            renderScale = Math.Clamp(renderScale, MinScaleFactor, MaxScaleFactor);
            ScalableBufferManager.ResizeBuffers(renderScale, renderScale);
            _lastRenderScale = renderScale;
        }

        // Get proper success status and set internal status to match.
        Status = upscaler == Plugin.Mode.None ? Plugin.UpscalerStatus.NoUpscalerSet : Plugin.UpscalerStatus.Success;

        // Return some success status if no settings errors were encountered.
        return new Tuple<Plugin.UpscalerStatus, string>(Status, "Upscaler Settings Updated Successfully");
    }

    // If the program ever encounters some kind of error, this will be called.
    // Rendering errors call this through InternalErrorCallbackWrapper.
    // Settings errors call this from ValidateAndPushSettings.

    // If there's an error at all, revert to previous successful upscaler (if it exists, otherwise none).
    // Unrecoverable error: remove upscaler that was set from list of supported upscalers.
    // In general, call the callback once during the frame (settings will update next frame).
    private void InternalErrorHandler(Plugin.UpscalerStatus reason, string message)
    {
        if (errorCallback == null)
        {
            Debug.LogError(message + "\nNo error callback. Reverting upscaling to None.");
            upscaler = Plugin.Mode.None;
        }
        else
        {
            errorCallback(reason, message);

            if (!ChangeInSettings())
            {
                Debug.LogError(message + "\nError callback did not make any settings changes. Reverting Upscaling to None.");
                upscaler = Plugin.Mode.None;
            }

            var settingsChangeEffect = ValidateAndPushSettings();
            if (Plugin.Failure(settingsChangeEffect.Item1))
            {
                Debug.LogError("Original Error:\n" + message + "\nCallback attempted to fix settings " +
                               "but failed. New Error:\n" + settingsChangeEffect.Item2);
                upscaler = Plugin.Mode.None;
            }

            Debug.LogError("Upscaler encountered an Error, but it was fixed by callback. Original Error:\n" + message);
        }
    }

    [MonoPInvokeCallback(typeof(Plugin.InternalErrorCallback))]
    private static void InternalErrorCallbackWrapper(IntPtr upscaler, Plugin.UpscalerStatus reason, IntPtr message)
    {
        var handle = (GCHandle) upscaler;

        (handle.Target as Upscaler)!.InternalErrorHandler(reason,
            "Error was encountered while upscaling. Details: " + Marshal.PtrToStringAnsi(message) + "\n");
    }
}