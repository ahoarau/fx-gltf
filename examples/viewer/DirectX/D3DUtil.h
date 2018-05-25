// ------------------------------------------------------------
// Copyright(c) 2018 Jesse Yurkovich
// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// See the LICENSE file in the repo root for full license information.
// ------------------------------------------------------------
#pragma once

#include <algorithm>
#include <cstdio>
#include <d3dcompiler.h>
#include <dxgi1_5.h>
#include <exception>
#include <fx/gltf.h>
#include <wrl.h>

#include "Logger.h"

namespace Util
{
    struct BBox
    {
        DirectX::XMFLOAT3 Min{};
        DirectX::XMFLOAT3 Max{};
        DirectX::XMFLOAT3 CenterTranslation{};
    };

    static DXGI_FORMAT GetFormat(fx::gltf::Accessor const * accessor)
    {
        if (accessor->type == fx::gltf::Accessor::Type::Vec3 && accessor->componentType == fx::gltf::Accessor::ComponentType::Float)
        {
            return DXGI_FORMAT_R32G32B32_FLOAT;
        }
        else if (accessor->type == fx::gltf::Accessor::Type::Scalar && accessor->componentType == fx::gltf::Accessor::ComponentType::UnsignedInt)
        {
            return DXGI_FORMAT_R32_UINT;
        }
        else if (accessor->type == fx::gltf::Accessor::Type::Scalar && accessor->componentType == fx::gltf::Accessor::ComponentType::UnsignedShort)
        {
            return DXGI_FORMAT_R16_UINT;
        }
        else
        {
            throw std::runtime_error("Unknown accessor types");
        }
    }

    static std::size_t ResourceSize(std::size_t size) noexcept
    {
        const std::size_t MinResourceSize = 64 * 1024;
        return size < MinResourceSize ? MinResourceSize : size;
    }

    static void AdjustBBox(BBox & currentBBox, BBox const & other) noexcept
    {
        currentBBox.Min.x = std::min(currentBBox.Min.x, other.Min.x);
        currentBBox.Min.y = std::min(currentBBox.Min.y, other.Min.y);
        currentBBox.Min.z = std::min(currentBBox.Min.z, other.Min.z);

        currentBBox.Max.x = std::max(currentBBox.Max.x, other.Max.x);
        currentBBox.Max.y = std::max(currentBBox.Max.y, other.Max.y);
        currentBBox.Max.z = std::max(currentBBox.Max.z, other.Max.z);
    }

    static void CenterBBox(BBox & currentBBox)
    {
        using namespace DirectX;
        const DirectX::XMVECTOR min = DirectX::XMLoadFloat3(&currentBBox.Min);
        const DirectX::XMVECTOR max = DirectX::XMLoadFloat3(&currentBBox.Max);
        const DirectX::XMVECTOR mid = DirectX::XMVectorNegate(0.5f * (min + max));

        DirectX::XMStoreFloat3(&currentBBox.CenterTranslation, mid);
    }

    static DirectX::XMFLOAT4 HSVtoRBG(float hue, float saturation, float value) noexcept
    {
        DirectX::XMFLOAT4 rgba{};

        rgba.x = fabsf(hue * 6.0f - 3.0f) - 1.0f;
        rgba.y = 2.0f - fabsf(hue * 6.0f - 2.0f);
        rgba.z = 2.0f - fabsf(hue * 6.0f - 4.0f);

        rgba.x = std::clamp(rgba.x, 0.0f, 1.0f);
        rgba.y = std::clamp(rgba.y, 0.0f, 1.0f);
        rgba.z = std::clamp(rgba.z, 0.0f, 1.0f);

        rgba.x = ((rgba.x - 1.0f) * saturation + 1.0f) * value;
        rgba.y = ((rgba.y - 1.0f) * saturation + 1.0f) * value;
        rgba.z = ((rgba.z - 1.0f) * saturation + 1.0f) * value;

        rgba.w = 1.0f;
        return rgba;
    }
} // namespace Util

namespace DX
{
    // Helper class for COM exceptions
    class com_exception : public std::exception
    {
    public:
        explicit com_exception(HRESULT hr) noexcept
            : result(hr) {}

        const char * what() const noexcept override
        {
            static char s_str[64] = {};
            sprintf_s(s_str, "Failure with HRESULT of %08X", result);
            return s_str;
        }

    private:
        HRESULT result;
    };

    // Helper utility converts D3D API failures into exceptions.
    inline void ThrowIfFailed(HRESULT hr)
    {
        if (FAILED(hr))
        {
            throw com_exception(hr);
        }
    }

    inline Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(
        std::wstring const & filename,
        std::string const & entrypoint,
        std::string const & target,
        D3D_SHADER_MACRO const * defines)
    {
        UINT compileFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)
        compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

        Microsoft::WRL::ComPtr<ID3DBlob> byteCode{};
        Microsoft::WRL::ComPtr<ID3DBlob> errors{};
        const HRESULT hr =
            D3DCompileFromFile(filename.c_str(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE, entrypoint.c_str(), target.c_str(), compileFlags, 0, &byteCode, &errors);

        if (errors != nullptr)
        {
            Logger::WriteLine(static_cast<char *>(errors->GetBufferPointer()));
        }

        ThrowIfFailed(hr);
        return byteCode;
    }
} // namespace DX