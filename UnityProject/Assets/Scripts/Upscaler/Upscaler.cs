using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using AOT;
using JetBrains.Annotations;
using UnityEngine;
using Upscaler.impl;

namespace Upscaler
{
    public class Upscaler : BackendUpscaler
    {
        // EXPOSED API FEATURES

        // BASIC UPSCALER OPTIONS
        // Can be set in Editor or in code.

        /// Current Upscaling Mode to Use
        public Plugin.UpscalerMode upscalerMode = Plugin.UpscalerMode.DLSS;

        /// Quality / Performance Mode for the Upscaler
        public Plugin.QualityMode qualityMode = Plugin.QualityMode.Auto;

        // ADVANCED UPSCALER OPTIONS
        // Can be set in Editor or in code.

        /// Sharpness (Technically DLSS Deprecated); Ranges from 0 to 1
        public float sharpness;

        // Strictly code accessible endpoints

        /// Callback function that will run when an error is encountered by the upscaler.
        /// It will be passed status (which contains error information) and a more detailed error message string.
        /// This function allows developers to determine what should happen when upscaler encounters an error
        /// This function is called the frame after an error, and it's changes take effect that frame
        /// If the same error is encountered during multiple frames, the function is only called for the first frame
        [CanBeNull] public Action<Plugin.UpscalerStatus, string> ErrorCallback = null;

        /// The current UpscalerStatus
        /// Contains Error Information for Settings Errors or Internal Problems
        public Plugin.UpscalerStatus Status { get; private set; }

        /// Readonly list of device/OS supported Upscalers
        public IList<Plugin.UpscalerMode> SupportedUpscalerModes { get; private set; }

        /// Removes history so that artifacts from previous frames are not left over in DLSS
        /// Should be called every time the scene sees a complete scene change
        public void ResetHistoryBuffer()
        {
            Plugin.ResetHistory();
        }

        /// Returns true if the device and operating system support the given upscaling mode
        /// Returns false if device and OS do not support Upscaling Mode
        public bool DeviceSupportsUpscalerMode(Plugin.UpscalerMode mode)
        {
            return SupportedUpscalerModes.Contains(mode);
        }

        // INTERNAL CAMERA OPTION HISTORY
        // private bool _lastAllowMSAA;

        // INTERNAL API IMPLEMENTATION

        // Calls base class method to initialize Upscaler and then uses Plugin functions to gather information about device supported options
        public void OnEnable()
        {
            Setup((IntPtr)GCHandle.Alloc(this), InternalErrorCallbackWrapper);

            SupportedUpscalerModes = Enum.GetValues(typeof(Plugin.UpscalerMode)).Cast<Plugin.UpscalerMode>().Where(
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
            return upscalerMode != ActiveUpscalerMode || qualityMode != ActiveQualityMode ||
                   !sharpness.Equals(LastSharpness);
        }

        /// Validates settings changes and pushes them to BackendUpscaler so they take effect if no issue met.
        /// Status with new settings will be returned.
        /// Returns a Tuple containing new internal UpscalerStatus as well as a message about settings change.
        private Tuple<Plugin.UpscalerStatus, string> ValidateAndPushSettings()
        {
            // Check for lack of support for currently selected upscaler.
            var settingsError = Plugin.GetError(upscalerMode);
            if (Plugin.Failure(settingsError))
            {
                var errorMsg = "There was an Error in Changing Upscaler Settings. Details:\n";
                errorMsg += "Invalid Upscaler: " + Marshal.PtrToStringAnsi(Plugin.GetErrorMessage(upscalerMode)) + "\n";
                Status = settingsError;
                return new Tuple<Plugin.UpscalerStatus, string>(settingsError, errorMsg);
            }

            // Propagate changes to backend if no errors when changing settings.
            ActiveUpscalerMode = upscalerMode;
            LastSharpness = Sharpness;
            Sharpness = sharpness;
            ActiveQualityMode = qualityMode;

            // Get proper success status and set internal status to match.
            Status = upscalerMode == Plugin.UpscalerMode.None
                ? Plugin.UpscalerStatus.NoUpscalerSet
                : Plugin.UpscalerStatus.Success;

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
            if (ErrorCallback == null)
            {
                Debug.LogError(message + "\nNo error callback. Reverting upscaling to None.");
                upscalerMode = Plugin.UpscalerMode.None;
            }
            else
            {
                ErrorCallback(reason, message);

                if (!ChangeInSettings())
                {
                    Debug.LogError(message +
                                   "\nError callback did not make any settings changes. Reverting Upscaling to None.");
                    upscalerMode = Plugin.UpscalerMode.None;
                }

                var settingsChangeEffect = ValidateAndPushSettings();
                if (Plugin.Failure(settingsChangeEffect.Item1))
                {
                    Debug.LogError("Original Error:\n" + message + "\nCallback attempted to fix settings " +
                                   "but failed. New Error:\n" + settingsChangeEffect.Item2);
                    upscalerMode = Plugin.UpscalerMode.None;
                }

                Debug.LogError("Upscaler encountered an Error, but it was fixed by callback. Original Error:\n" + message);
            }
        }

        [MonoPInvokeCallback(typeof(Plugin.InternalErrorCallback))]
        private static void InternalErrorCallbackWrapper(IntPtr upscaler, Plugin.UpscalerStatus reason, IntPtr message)
        {
            var handle = (GCHandle)upscaler;

            (handle.Target as Upscaler)!.InternalErrorHandler(reason,
                "Error was encountered while upscaling. Details: " + Marshal.PtrToStringAnsi(message) + "\n");
        }
    }
}