// Precompiled header. Pulls in the heavy Windows + DirectX umbrella includes
// once, so we don't pay parsing cost in every translation unit.

#pragma once

// clang-format off
#include <Windows.h>
#include <windowsx.h>
#include <ShellScalingApi.h>
#include <dwmapi.h>
#include <uxtheme.h>

// initguid.h emits the actual GUID bodies for COM CLSID/IID symbols
// declared elsewhere with EXTERN_C. Without it, references like
// CLSID_D2D1GaussianBlur are unresolved at link time. It MUST be
// included exactly once in the project, and BEFORE any D2D / DXGI /
// DComp header that declares those identifiers.
#include <initguid.h>

#include <d3d11.h>
#include <dxgi1_3.h>
#include <d2d1_3.h>
#include <d2d1effects_2.h>
#include <d2d1helper.h>
#include <dwrite_3.h>
#include <dcomp.h>
#include <wincodec.h>

#include <wrl/client.h>
// clang-format on

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace vrtx {

// Concise alias for COM smart pointer.
template <typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

// Throw a Win32 HRESULT failure as a runtime_error with the hex code.
inline void ThrowIfFailed(HRESULT hr, const char* what = "HRESULT") {
    if (FAILED(hr)) {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "%s failed: 0x%08lX", what,
                      static_cast<unsigned long>(hr));
        throw std::runtime_error(buf);
    }
}

}  // namespace vrtx
