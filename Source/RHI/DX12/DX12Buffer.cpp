#include "DX12Buffer.h"
#include <cassert>
#include <cstring>
#include <cstdio>
#include <windows.h>
#include <dxgi1_4.h>

DX12Buffer::DX12Buffer(
    DX12Device* device,
    uint32_t size,
    BufferType type,
    const void* initialData,
    uint32_t stride)
    : m_size(size)
    , m_type(type)
    , m_stride(stride)
{
    // 基本チェック
    if (!device || !device->GetDevice() || size == 0) {
        char buf[256];
        sprintf_s(buf, "[DX12Buffer] Invalid params: device=%p size=%u\n", (void*)device, size);
        OutputDebugStringA(buf);
        return;
    }

    // 定数バッファは 256 byte 境界に揃える必要がある
    uint32_t alignedSize = size;
    if (type == BufferType::Constant) {
        alignedSize = (size + 255) & ~255;
        m_size = alignedSize;
    }

    // ----------------------------------------
    // ヒープ設定
    // UAVStorage:
    //   GPU専用(DEFAULT heap)
    //   CPUから直接Mapできない
    //
    // それ以外:
    //   CPUアップロード用(UPLOAD heap)
    //   MapしてCPUから書き込める
    // ----------------------------------------
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = (type == BufferType::UAVStorage)
        ? D3D12_HEAP_TYPE_DEFAULT
        : D3D12_HEAP_TYPE_UPLOAD;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    // ----------------------------------------
    // バッファリソースの説明
    // BUFFERなので Dimension は BUFFER 固定
    // Width にバイト数を入れる
    // ----------------------------------------
    D3D12_RESOURCE_DESC bufferDesc = {};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Alignment = 0;
    bufferDesc.Width = alignedSize;
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.SampleDesc.Quality = 0;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    // UAVバッファだけは UAV フラグを付ける
    bufferDesc.Flags = (type == BufferType::UAVStorage)
        ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
        : D3D12_RESOURCE_FLAG_NONE;

    // ----------------------------------------
    // 初期状態
    // UAVStorage:
    //   バッファは COMMON で作成し、実使用時に明示的に遷移する。
    //   CreateCommittedResource で UAV 初期状態を指定しても
    //   debug layer では COMMON 扱いになるため、ここでは COMMON を使う。
    //
    // それ以外:
    //   UPLOAD heap なので GENERIC_READ
    // ----------------------------------------
    D3D12_RESOURCE_STATES initialState =
        (type == BufferType::UAVStorage)
        ? D3D12_RESOURCE_STATE_COMMON
        : D3D12_RESOURCE_STATE_GENERIC_READ;

    // 実際に D3D12 バッファ作成
    HRESULT hr = device->GetDevice()->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        initialState,
        nullptr,
        IID_PPV_ARGS(&m_resource));

    // 失敗時は詳細をデバッグ出力し、m_resource=null のまま続行
    if (FAILED(hr)) {
        char buf[512];
        sprintf_s(
            buf,
            "[DX12Buffer] CreateCommittedResource FAILED: hr=0x%08X size=%u aligned=%u type=%d stride=%u heap=%s\n",
            static_cast<unsigned>(hr),
            size,
            alignedSize,
            static_cast<int>(type),
            stride,
            (type == BufferType::UAVStorage) ? "DEFAULT" : "UPLOAD");
        OutputDebugStringA(buf);
        // デバイスロスト時は理由も出力
        if (hr == DXGI_ERROR_DEVICE_REMOVED && device->GetDevice()) {
            HRESULT reason = device->GetDevice()->GetDeviceRemovedReason();
            char buf2[256];
            sprintf_s(buf2, "[DX12Buffer] DeviceRemovedReason: 0x%08X\n", static_cast<unsigned>(reason));
            OutputDebugStringA(buf2);
            FILE* f2 = nullptr;
            fopen_s(&f2, "dx12_buffer_errors.log", "a");
            if (f2) { fprintf(f2, "%s", buf2); fclose(f2); }
        }
        // ファイルにもログ出力
        FILE* f = nullptr;
        fopen_s(&f, "dx12_buffer_errors.log", "a");
        if (f) { fprintf(f, "%s", buf); fclose(f); }
        return;
    }

    // ----------------------------------------
    // 初期データを書き込む
    //
    // 注意:
    // UAVStorage は DEFAULT heap なので Map() できない。
    // そのため initialData を入れたい場合は
    // 本来は UploadBuffer 経由のコピーが必要。
    //
    // 今の Map() は UAVStorage のとき nullptr を返す。
    // ----------------------------------------
    if (initialData && m_resource) {
        void* mapped = Map();
        if (mapped) {
            memcpy(mapped, initialData, size);
            Unmap();
        }
    }
}

void* DX12Buffer::Map()
{
    // UAVStorage(DEFAULT heap) は CPU から直接 Map 不可
    if (m_type == BufferType::UAVStorage) {
        return nullptr;
    }

    // リソースが作成されていない場合
    if (!m_resource) {
        return nullptr;
    }

    // まだMapしていない時だけMapする
    if (!m_mappedData) {
        D3D12_RANGE readRange = { 0, 0 }; // CPUは読み取らない
        HRESULT hr = m_resource->Map(0, &readRange, &m_mappedData);
        if (FAILED(hr)) {
            return nullptr;
        }
    }

    return m_mappedData;
}

void DX12Buffer::Unmap()
{
    // Map済みの時だけUnmap
    if (m_mappedData) {
        m_resource->Unmap(0, nullptr);
        m_mappedData = nullptr;
    }
}
