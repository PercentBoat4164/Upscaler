#ifdef ENABLE_DX12
#    include "DX12.hpp"

#    include <d3d12compatibility.h>

#    include <IUnityGraphicsD3D12.h>

IUnityGraphicsD3D12v7* DX12::graphicsInterface{nullptr};

bool DX12::registerUnityInterfaces(IUnityInterfaces* t_unityInterfaces) {
    graphicsInterface = t_unityInterfaces->Get<IUnityGraphicsD3D12v7>();
    return true;
}

IUnityGraphicsD3D12v7* DX12::getGraphicsInterface() {
    return graphicsInterface;
}

bool DX12::unregisterUnityInterfaces() {
    graphicsInterface = nullptr;
    return true;
}
#endif