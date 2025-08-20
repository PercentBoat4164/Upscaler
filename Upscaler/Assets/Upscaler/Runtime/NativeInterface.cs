using System;
using System.Collections;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using UnityEditor;
using UnityEngine;
using UnityEngine.Experimental.Rendering;
using UnityEngine.Rendering;

namespace Upscaler.Runtime
{
    internal static class NativeInterface
    {
        [DllImport("GfxPluginUpscaler")]
        private static extern bool LoadedCorrectlyPlugin();

        private static bool WarnOnBadLoad()
        {
            try
            {
                return LoadedCorrectlyPlugin();
            }
            catch (DllNotFoundException)
            {
                if (Application.platform == RuntimePlatform.WindowsEditor || Application.platform == RuntimePlatform.WindowsPlayer)
                    Debug.LogError("The Upscaler plugin could not be loaded. Please restart Unity. If this problem persists please reinstall Upscaler or submit an issue on Upscaler's GitHub: https://github.com/PercentBoat4164/Upscaler.");
                return false;
            }
        }

        internal static readonly bool Loaded = WarnOnBadLoad();
    }
}