#include "DX12.hpp"

// Upscaler
#include <Upscaler/Upscaler.hpp>

// Graphics API
#include <d3d12.h>
#include <d3d12compatibility.h>

// Unity
#include <IUnityGraphicsD3D12.h>

DX12 *DX12::get() {
    static DX12 *dx12{new DX12};
    return dx12;
}

GraphicsAPI::Type DX12::getType() {
    return GraphicsAPI::DX12;
}

bool DX12::useUnityInterfaces(IUnityInterfaces *t_unityInterfaces) {
    DX12Interface = t_unityInterfaces->Get<IUnityGraphicsD3D12v7>();
    return true;
}

IUnityGraphicsD3D12v7 *DX12::getUnityInterface() const {
    return DX12Interface;
}
