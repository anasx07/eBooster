#include "ebooster/TextureLoader.hpp"
#include "ebooster/Common.hpp"

#include <wincodec.h>
#include <wrl/client.h>
#include <filesystem>
#include <vector>

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

using Microsoft::WRL::ComPtr;

namespace ebooster {

TextureLoader::TextureLoader(ID3D11Device* pDevice)
    : m_pDevice(pDevice) {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
}

TextureLoader::~TextureLoader() {
    Clear();
}

void TextureLoader::Clear() {
    for (auto& [path, info] : m_cache) {
        if (info.srv) {
            info.srv->Release();
            info.srv = nullptr;
        }
    }
    m_cache.clear();
}

ID3D11ShaderResourceView* TextureLoader::LoadTexture(const std::string& filePath, int* outWidth, int* outHeight) {
    if (filePath.empty() || !std::filesystem::exists(filePath) || !m_pDevice) {
        return nullptr;
    }

    auto it = m_cache.find(filePath);
    if (it != m_cache.end()) {
        if (outWidth) *outWidth = it->second.width;
        if (outHeight) *outHeight = it->second.height;
        return it->second.srv;
    }

    ComPtr<IWICImagingFactory> pFactory;
    HRESULT hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&pFactory)
    );
    if (FAILED(hr)) return nullptr;

    std::wstring wpath = std::filesystem::path(filePath).wstring();
    ComPtr<IWICBitmapDecoder> pDecoder;
    hr = pFactory->CreateDecoderFromFilename(
        wpath.c_str(),
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnDemand,
        &pDecoder
    );
    if (FAILED(hr)) return nullptr;

    ComPtr<IWICBitmapFrameDecode> pFrame;
    hr = pDecoder->GetFrame(0, &pFrame);
    if (FAILED(hr)) return nullptr;

    UINT width = 0, height = 0;
    pFrame->GetSize(&width, &height);
    if (width == 0 || height == 0) return nullptr;

    ComPtr<IWICFormatConverter> pConverter;
    hr = pFactory->CreateFormatConverter(&pConverter);
    if (FAILED(hr)) return nullptr;

    hr = pConverter->Initialize(
        pFrame.Get(),
        GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0,
        WICBitmapPaletteTypeCustom
    );
    if (FAILED(hr)) return nullptr;

    UINT rowPitch = width * 4;
    UINT imageSize = rowPitch * height;
    std::vector<BYTE> pixelData(imageSize);

    hr = pConverter->CopyPixels(nullptr, rowPitch, imageSize, pixelData.data());
    if (FAILED(hr)) return nullptr;

    // Create Direct3D 11 2D Texture
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA initData{};
    initData.pSysMem = pixelData.data();
    initData.SysMemPitch = rowPitch;

    ComPtr<ID3D11Texture2D> pTexture;
    hr = m_pDevice->CreateTexture2D(&desc, &initData, &pTexture);
    if (FAILED(hr)) return nullptr;

    // Create Shader Resource View (SRV) for ImGui
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;

    ID3D11ShaderResourceView* pSRV = nullptr;
    hr = m_pDevice->CreateShaderResourceView(pTexture.Get(), &srvDesc, &pSRV);
    if (FAILED(hr)) return nullptr;

    TextureInfo info;
    info.srv = pSRV;
    info.width = static_cast<int>(width);
    info.height = static_cast<int>(height);

    if (outWidth) *outWidth = info.width;
    if (outHeight) *outHeight = info.height;

    m_cache[filePath] = info;
    return pSRV;
}

} // namespace ebooster
