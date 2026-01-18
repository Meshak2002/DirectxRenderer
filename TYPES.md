# Global Type System - DirectxRenderer

This document describes the unified type system used throughout the DirectX renderer project. All common types and structures are defined in a single location to eliminate duplication and ensure consistency.

## Core Type Definitions

### Location: `src/Utility/Vertex.h`

This is the **single source of truth** for all common vertex and geometry types used across the renderer.

### Defined Types:

#### 1. **`struct Vertex`** - Universal Vertex Format
Used by ALL geometry systems (GeometryGenerator, ModelImporter, rendering pipeline).

**Layout (44 bytes total):**
```cpp
struct Vertex {
    DirectX::XMFLOAT3 Position;   // 12 bytes (offset 0)
    DirectX::XMFLOAT2 TexCoord;   // 8 bytes  (offset 12)
    DirectX::XMFLOAT3 Normal;     // 12 bytes (offset 20)
    DirectX::XMFLOAT3 Tangent;    // 12 bytes (offset 32)
};
```

**Matches HLSL Input Layout:**
```hlsl
struct VertexIn {
    float3 PosL    : POSITION;   // Vertex.Position
    float2 TexC    : TEXCOORD;   // Vertex.TexCoord
    float3 NormalL : NORMAL;     // Vertex.Normal
    float3 TangentL: TANGENT;    // Vertex.Tangent
};
```

#### 2. **`uint16`** - 16-bit Index Type
```cpp
using uint16 = std::uint16_t;
```
- Used for meshes with < 65,536 vertices
- **Maximum addressable vertices:** 65,535
- **Common use case:** Most procedural geometry (cubes, spheres, quads)

#### 3. **`uint32`** - 32-bit Index Type
```cpp
using uint32 = std::uint32_t;
```
- Used for large imported models
- **Maximum addressable vertices:** 4,294,967,295
- **Common use case:** High-poly imported FBX/OBJ models

---

## Usage Across Modules

### ✅ **GeometryGenerator** (`src/Utility/GeometryGenerator.h`)
```cpp
#include "Vertex.h"  // Imports global Vertex, uint16, uint32

class GeometryGenerator {
public:
    using uint16 = ::uint16;  // Class alias to global type
    using uint32 = ::uint32;

    struct MeshData {
        std::vector<Vertex> Vertices;      // Uses global Vertex
        std::vector<uint32> Indices32;     // Uses global uint32
        std::vector<uint16>& GetIndices16();
    };
};
```

**Why the class aliases?**
- Allows existing code `GeometryGenerator::uint16` to continue working
- Provides backward compatibility
- Redirects to the global definition

### ✅ **ModelImporter** (`src/Utility/ModelImporter.h`)
```cpp
#include "Vertex.h"  // Imports global Vertex, uint16, uint32

namespace ModelImporter {
    // Uses global Vertex directly - no redefinition needed!

    struct ModelData {
        std::vector<Vertex> Vertices;       // Uses global Vertex
        std::vector<uint16_t> Indices16;    // Note: uses std::uint16_t
        std::vector<uint32_t> Indices32;    // Note: uses std::uint32_t
    };
}
```

**Note:** ModelImporter uses `std::uint16_t` explicitly (not the global alias). This is fine - both work!

### ✅ **ShapesApp** (`src/ShapesApp.cpp`)
```cpp
// Accesses Vertex through GeometryGenerator
auto VertexData = (GeometryGenerator::Vertex*)RenderItem->MeshGeometryRef->VertexBufferCPU->GetBufferPointer();

// Uses global index types
auto IndexData = (GeometryGenerator::uint16*)RenderItem->MeshGeometryRef->IndexBufferCPU->GetBufferPointer();
```

---

## Benefits of Unified System

### ✅ **Single Point of Modification**
Change vertex layout ONCE in `Vertex.h`:
```cpp
// Example: Add a Color field
struct Vertex {
    DirectX::XMFLOAT3 Position;
    DirectX::XMFLOAT2 TexCoord;
    DirectX::XMFLOAT3 Normal;
    DirectX::XMFLOAT3 Tangent;
    DirectX::XMFLOAT4 Color;  // ← Add once, reflects everywhere!
};
```

Then update HLSL and you're done - no hunting through multiple files!

### ✅ **Type Safety**
- No accidental size mismatches between different modules
- Compiler catches incompatible vertex formats immediately

### ✅ **No Duplication**
**Before (BAD):**
```
GeometryGenerator.h  → struct Vertex { ... }
ModelImporter.h      → struct Vertex { ... }  // DUPLICATE!
ShapesApp.cpp        → struct Vertex { ... }  // ANOTHER COPY!
```

**After (GOOD):**
```
Vertex.h             → struct Vertex { ... }  // SINGLE SOURCE
GeometryGenerator.h  → #include "Vertex.h"
ModelImporter.h      → #include "Vertex.h"
ShapesApp.cpp        → Uses it indirectly
```

---

## Shader Synchronization

**CRITICAL:** When modifying `Vertex`, update ALL shaders using that vertex format:

### Affected Shaders:
- `src/Shaders/ShapesApp.hlsl`
- `src/Shaders/ShadowMap.hlsl`
- `src/Shaders/ShadowMapDebug.hlsl`
- `src/Shaders/Skybox.hlsl`

### Example Sync:
If you add `float4 Color` to C++ Vertex:
```cpp
struct Vertex {
    DirectX::XMFLOAT3 Position;
    DirectX::XMFLOAT2 TexCoord;
    DirectX::XMFLOAT3 Normal;
    DirectX::XMFLOAT3 Tangent;
    DirectX::XMFLOAT4 Color;      // ← NEW
};
```

Update HLSL:
```hlsl
struct VertexIn {
    float3 PosL     : POSITION;
    float2 TexC     : TEXCOORD;
    float3 NormalL  : NORMAL;
    float3 TangentL : TANGENT;
    float4 Color    : COLOR;       // ← NEW
};
```

Update Input Layout in `ShapesApp::BuildShadersAndInputLayout()`:
```cpp
InputLayouts.push_back(
    { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT,
      0, 44, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 });  // offset 44
```

---

## Adding New Global Types

To add new common types, edit `src/Utility/Vertex.h`:

```cpp
// Example: Add bounding box type
struct BoundingBoxData {
    DirectX::XMFLOAT3 Min;
    DirectX::XMFLOAT3 Max;
};

// Example: Add material constants
struct MaterialConstants {
    DirectX::XMFLOAT4 DiffuseAlbedo;
    DirectX::XMFLOAT3 FresnelR0;
    float Roughness;
};
```

These will automatically be available everywhere that includes `Vertex.h`!

---

## Migration Checklist

When adding a new utility module that uses vertices:

1. ✅ `#include "Vertex.h"` at the top
2. ✅ Use `Vertex` directly (NOT `YourModule::Vertex`)
3. ✅ Use `uint16` and `uint32` for indices (NOT `std::uint16_t` unless preferred)
4. ✅ **NEVER** redefine `struct Vertex` in your module
5. ✅ Update HLSL shaders if vertex layout changes

---

## Current File Structure

```
src/Utility/Vertex.h          ← SINGLE SOURCE OF TRUTH
├─ Used by: GeometryGenerator.h
├─ Used by: ModelImporter.h
├─ Used by: d3dUtil.h (MeshGeometry)
└─ Used by: Any new geometry modules

src/Shaders/*.hlsl            ← Must match C++ Vertex layout
├─ ShapesApp.hlsl
├─ ShadowMap.hlsl
├─ ShadowMapDebug.hlsl
└─ Skybox.hlsl
```

---

## Summary

✅ **All vertex data** → `Vertex` struct in `Vertex.h`
✅ **All index types** → `uint16`/`uint32` in `Vertex.h`
✅ **One change, everywhere updated**
✅ **Type-safe, no duplicates, maintainable**

**Rule of thumb:** If multiple files need it, it belongs in `Vertex.h`!
