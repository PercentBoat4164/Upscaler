#pragma once

#ifdef ENABLE_DX12

#    include "GraphicsAPI.hpp"

// Graphics API
#    include <d3d12.h>
#    include <d3d12compatibility.h>

// Unity
#    include "IUnityGraphicsD3D12.h"

class DX12 final : public GraphicsAPI {
    ID3D12CommandAllocator    *_oneTimeSubmitCommandAllocator{nullptr};
    ID3D12GraphicsCommandList *_oneTimeSubmitCommandList{nullptr};
    bool                       _oneTimeSubmitRecording{false};
    ID3D12Fence               *_oneTimeSubmitFence{};
    HANDLE                     _oneTimeSubmitEvent{};

    DX12() = default;

    IUnityGraphicsD3D12v7 *DX12Interface{};
    ID3D12Device          *device{};

public:
    DX12(const DX12 &) = delete;
    DX12(DX12 &&)      = default;

    DX12 &operator=(const DX12 &) = delete;
    DX12 &operator=(DX12 &&)      = default;

    static DX12 *get();

    Type         getType() override;

    bool                                 useUnityInterfaces(IUnityInterfaces *t_unityInterfaces) override;
    [[nodiscard]] IUnityGraphicsD3D12v7 *getUnityInterface() const;

    ~DX12() override = default;
};

#endif