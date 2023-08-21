using AOT;
using fts;
using System;
using System.Runtime.InteropServices;
using UnityEngine;

// [PluginAttr("DLSSPlugin")]  // Plugin must be stored in "Assets/Plugins".
public class NewBehaviourScript : MonoBehaviour
{
    public delegate void debugCallback(IntPtr message);
//     [PluginFunctionAttr("SetDebugCallback")]  // Must match the name of the function in the C++ file.
//     public static _SetDebugCallback SetDebugCallback = null;  // The first identifier does not matter. The second is the name that will be used in the C# script.
//     public delegate void _SetDebugCallback(debugCallback cb);  // Signature must match that of the C++ function.
//
//     [PluginFunctionAttr("InitializeNGX")]
//     public static _InitializeNGX InitializeNGX = null;
//     public delegate void _InitializeNGX();

    [DllImport("DLSSPlugin")] public static extern void SetDebugCallback(debugCallback cb);
    [DllImport("DLSSPlugin")] public static extern void InitializeNGX();

    [MonoPInvokeCallback(typeof(debugCallback))]
    void LogDebugMessage(IntPtr message) {
    	Debug.Log(Marshal.PtrToStringAnsi(message));
    }

    void OnLoad() {
    }

    // OnEnable is called when the plugin is enabled
    void OnEnable() {
    	SetDebugCallback(LogDebugMessage);
    	InitializeNGX();
    }
	
    // Start is called before the first frame update
    void Start()
    {
    }

    // Update is called once per frame
    void Update()
    {

    }
}
