using System;
using System.Collections.Generic;
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
    private float _activeSharpness;

    // Must remain separate for niceness in the Editor
    /// Upscale Factors for Width and Height Respectively (Only Used in Dynamic Manual Scaling)
    public float widthScaleFactor = 0.9f;
    public float heightScaleFactor = 0.9f;

    // Strictly Code Accessible Endpoints

    /// Bounds for Scale Factor Obtained from the Plugin
    public Vector2 MinScaleFactor => MinScale;
    public Vector2 MaxScaleFactor => MaxScale;

    /// Callback Function that Will Run when an Error is Encountered by DLSS
    /// Function will be passed Status (which contains error information) and Error Message String
    /// This Function allows Developers to Determine what Should Happen when Upscaler Encounters an Error
    /// This Function is Called the Frame After an Error, and it's changes take effect that frame
    /// If the same error is encountered during multiple frames, the function is only called for the first frame
    [CanBeNull] public Action<Plugin.UpscalerStatus, string> ErrorCallback;

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

    // INTERNAL API IMPLEMENTATION

    // Internal Values for supported upscaling modes and upscaler status
    private ulong _prevMaxScale, _prevMinScale;

    // Runs when the Script is enabled
    // Calls Base Class Method to Initialize Plugin and then uses Plugin Functions to gather information about device supported options
    public new void OnEnable()
    {
        base.OnEnable();

        var handle = GCHandle.Alloc(this);
        Plugin.InitializePlugin((IntPtr) handle, InternalErrorCallbackWrapper);

        var tempList = new List<Plugin.Mode>();

        foreach (Plugin.Mode tempUpscaler in Enum.GetValues(typeof(Plugin.Mode)))
            if (Plugin.Success(Plugin.GetError(tempUpscaler)))
                tempList.Add(tempUpscaler);

        SupportedUpscalingModes = tempList.AsReadOnly();
    }

    /*@todo Determine if I am necessary. */
    private new void OnDisable()
    {
        base.OnDisable();
    }

    /// Runs Before Culling; Validates Settings and Checks for Errors from Previous Frame Before Calling
    /// Real OnPreCull Actions from Backend
    private void OnPreCull()
    {
        if (ChangeInSettings())
        {
            //If there are settings changes, Validate and Push them (this includes a potential call to Error Callback)
            var settingsChange = ValidateAndPushSettings();

            // If Settings Change Caused Error, pass to Internal Error Handler
            if (Plugin.Failure(settingsChange.Item1))
            {
                InternalErrorHandler(settingsChange.Item1, settingsChange.Item2);
            }
        }

        /*@todo Make switching me nicer. */
        // If the upscaler is active and the quality mode is Dynamic, ensure dynamic resolution is enabled
        if (Camera.allowDynamicResolution)
            Camera.allowDynamicResolution = ActiveMode == Plugin.Mode.None;
        // if (!Camera.allowDynamicResolution && ActiveMode != Plugin.Mode.None &&
        //     ActiveQuality == Plugin.Quality.DynamicAuto || ActiveQuality == Plugin.Quality.DynamicManual)
        // {
        //     Camera.allowDynamicResolution = false;
        // }
        //
        // // If the Upscaler is not None, the Quality Mode does not allow for Dynamic Resolution, and the Camera is set
        // // to dynamic resolution, turn Dynamic Resolution off and give a warning
        // if (ActiveMode != Plugin.Mode.None && ActiveQuality != Plugin.Quality.DynamicAuto &&
        //     ActiveQuality != Plugin.Quality.DynamicManual && Camera.allowDynamicResolution)
        // {
        //     Camera.allowDynamicResolution = false;
        //     Debug.LogWarning("Dynamic resolution has been disabled for the upscaled camera " +
        //                      "since the current upscaling and quality modes do not allow for it.");
        // }

        // If the upscaling resolution changed, alter bounds on scale factor and reset scale factor
        if (DUpscalingResolution)
        {
            var maximumInputResolution = Plugin.GetMaximumInputResolution();
            if (maximumInputResolution != _prevMaxScale)
            {
                MaxScale = new Vector2((uint)(maximumInputResolution >> 32),
                    (uint)(maximumInputResolution & 0xFFFFFFFF)) / UpscalingResolution;

                _prevMaxScale = maximumInputResolution;

                widthScaleFactor = ActiveUpscalingFactor.x;
                heightScaleFactor = ActiveUpscalingFactor.y;
            }

            var minimumInputResolution = Plugin.GetMinimumInputResolution();
            if (minimumInputResolution != _prevMinScale)
            {
                MinScale = new Vector2((uint)(minimumInputResolution >> 32),
                    (uint)(minimumInputResolution & 0xFFFFFFFF)) / UpscalingResolution;

                _prevMinScale = minimumInputResolution;

                widthScaleFactor = ActiveUpscalingFactor.x;
                heightScaleFactor = ActiveUpscalingFactor.y;
            }
        }

        BeforeCameraCulling();
    }

    /// Shows if Settings have Changed since Last Checked
    /// No internal change should ever reflect a settings 'change'
    /// This will only return true when a user change causes settings to fall out of sync between Upscaler and BackendUpscaler
    private bool ChangeInSettings()
    {
        if (upscaler == ActiveMode)
            return quality != ActiveQuality || !sharpness.Equals(_activeSharpness) ||
                   !widthScaleFactor.Equals(ActiveUpscalingFactor.x) ||
                   !heightScaleFactor.Equals(ActiveUpscalingFactor.y);
        /*todo Move me to where I should go. */
        if (Camera.allowMSAA && upscaler != Plugin.Mode.None)
            Debug.LogWarning("MSAA should not be turned on for cameras that have upscaling applied to them. " +
                             "It is not necessary and not performant. It could also cause image artifacting.");
        return true;

    }

    /// Validates Settings Changes and Push them to BackendUpscaler so they Take Effect if No Issue Met
    /// Status with new settings will be returned
    /// Returns a Tuple containing New Internal Status as well as a Message About Settings Change
    private Tuple<Plugin.UpscalerStatus, string> ValidateAndPushSettings()
    {
        var errorMsg = "There was an Error in Changing Upscaler Settings. Details:\n";

        // Check for Lack of Support for Currently Selected Upscaler
        var settingsError = Plugin.GetError(upscaler);
        if (Plugin.Failure(settingsError))
        {
            errorMsg += "Invalid Upscaler: " +
                        Marshal.PtrToStringAuto(Plugin.GetErrorMessage(upscaler)) + "\n";
            Status = settingsError;
            return new Tuple<Plugin.UpscalerStatus, string>(settingsError, errorMsg);
        }

        // Check Sharpness Value for Change
        // Only Warn if Error and Revert Setting; No Error for Small Setting
        if (!sharpness.Equals(_activeSharpness))
        {
            if (sharpness is < 0 or > 1)
            {
                Debug.LogError("Sharpness value must be between 0 and 1. Sharpness reverted to previous value.");
                sharpness = _activeSharpness;
            }
            else
            {
                _activeSharpness = sharpness;
                Plugin.SetSharpnessValue(_activeSharpness);
            }
        }

        // Propagate Changes to Backend If No Errors In Changing Settings
        ActiveMode = upscaler;
        ActiveQuality = quality;

        if (!widthScaleFactor.Equals(ActiveUpscalingFactor.x) || !heightScaleFactor.Equals(ActiveUpscalingFactor.y))
        {
            ActiveUpscalingFactor = new Vector2(widthScaleFactor, heightScaleFactor);
            if (!ActiveUpscalingFactor.x.Equals(widthScaleFactor))
            {
                Debug.LogWarning("The Width Scale Factor Provided is outside of the proper range. It has been" +
                                 "clipped to: " + ActiveUpscalingFactor.x);
                widthScaleFactor = ActiveUpscalingFactor.x;
            }

            if (!ActiveUpscalingFactor.y.Equals(heightScaleFactor))
            {
                Debug.LogWarning("The Height Scale Factor Provided is outside of the proper range. It has been" +
                                 "clipped to: " + ActiveUpscalingFactor.y);
                heightScaleFactor = ActiveUpscalingFactor.y;
            }

            if (ActiveQuality != Plugin.Quality.DynamicManual)
            {
                Debug.LogWarning("Changing the upscaling factor manually only applies when the " +
                                 "Quality Mode is Dynamic Manual. The value has been changed but will have no effect.");
            }
        }

        // Get Proper Success Status and Set Internal Status to match
        var stat = upscaler == Plugin.Mode.None ? Plugin.UpscalerStatus.NoUpscalerSet : Plugin.UpscalerStatus.Success;
        Status = stat;

        // Return some success status if no settings errors were encountered
        return new Tuple<Plugin.UpscalerStatus, string>(stat, "Upscaler Settings Updated Successfully");
    }

    // If the program ever encounters some kind of error, this will be called
    // Rendering errors call this through InternalErrorCallbackWrapper
    // Settings errors call this from ValidateAndPushSettings

    // If there's an error at all, revert to previous successful upscaler (if it exists, otherwise none)
    // Unrecoverable Error: Remove upscaler that was set from list of supported upscalers
    // In general, call the callback once during the frame (settings will update next frame)
    private void InternalErrorHandler(Plugin.UpscalerStatus reason, string message)
    {
        if (ErrorCallback == null)
        {
            Debug.LogError(message + "\nNo error callback. Reverting upscaling to None.");
            upscaler = Plugin.Mode.None;
            ActiveMode = Plugin.Mode.None;
        }
        else
        {
            ErrorCallback(reason, message);

            if (!ChangeInSettings())
            {
                Debug.LogError(message + "\nError callback did not make any settings changes. Reverting Upscaling to None.");
                upscaler = Plugin.Mode.None;
                ActiveMode = Plugin.Mode.None;
            }

            var settingsChangeEffect = ValidateAndPushSettings();
            if (Plugin.Failure(settingsChangeEffect.Item1))
            {
                Debug.LogError("Original Error:\n" + message + "\nCallback attempted to fix settings " +
                               "but failed. New Error:\n" + settingsChangeEffect.Item2);
                upscaler = Plugin.Mode.None;
                ActiveMode = Plugin.Mode.None;
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