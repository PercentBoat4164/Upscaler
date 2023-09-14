#include "DX11.hpp"

// Graphics API
#include <d3d11.h>

// Unity
#include <IUnityGraphicsD3D11.h>

GraphicsAPI::Type DX11::getType() {
    return GraphicsAPI::DX11;
}

void DX11::prepareForOneTimeSubmits() {
    device = DX11Interface->GetDevice();
    device->GetImmediateContext(&_oneTimeSubmitContext);
}

ID3D11DeviceContext *DX11::beginOneTimeSubmitRecording() {
    _oneTimeSubmitRecording = true;
    return _oneTimeSubmitContext;
}

void DX11::endOneTimeSubmitRecording() {
    if (!_oneTimeSubmitRecording) return;
    _oneTimeSubmitRecording = false;
}

void DX11::cancelOneTimeSubmitRecording() {
    if (!_oneTimeSubmitRecording) return;
    _oneTimeSubmitContext->ClearState();
    _oneTimeSubmitRecording = false;
}

void DX11::finishOneTimeSubmits() {
    cancelOneTimeSubmitRecording();
    _oneTimeSubmitContext->Release();
}

DX11 *DX11::get() {
    static DX11 *dx11{new DX11};
    return dx11;
}

bool DX11::useUnityInterfaces(IUnityInterfaces *t_unityInterfaces) {
    DX11Interface = t_unityInterfaces->Get<IUnityGraphicsD3D11>();
    return true;
}

IUnityGraphicsD3D11 *DX11::getUnityInterface() {
    return DX11Interface;
}
