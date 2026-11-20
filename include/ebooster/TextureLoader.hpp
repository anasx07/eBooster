#pragma once

#include <d3d11.h>
#include <string>
#include <unordered_map>

namespace ebooster {

struct TextureInfo {
    ID3D11ShaderResourceView* srv{nullptr};
    int width{0};
    int height{0};
};

class TextureLoader {
public:
    explicit TextureLoader(ID3D11Device* pDevice);
    ~TextureLoader();

    // Loads a texture from file (JPG, PNG, BMP) using Windows Imaging Component (WIC)
    ID3D11ShaderResourceView* LoadTexture(const std::string& filePath, int* outWidth = nullptr, int* outHeight = nullptr);

    // Clears all GPU texture resources
    void Clear();

private:
    ID3D11Device* m_pDevice{nullptr};
    std::unordered_map<std::string, TextureInfo> m_cache;
};

} // namespace ebooster
