# DirectX 12 Renderer

A modern DirectX 12 rendering engine built with C++20, featuring advanced rendering techniques including shadow mapping, cube map reflections, normal mapping, and runtime object manipulation.

![DirectX Renderer Demo](OutputPics/DirectX_Output_11__.png)

## Features

### Core Rendering
- **DirectX 12 API** - Modern low-level graphics API with explicit control
- **Triple Buffering** - Frame resource management for optimal CPU-GPU parallelism
- **Descriptor Heap Management** - Efficient GPU resource binding
- **Root Signature System** - Flexible shader parameter configuration

### Advanced Techniques
- ✨ **Shadow Mapping** - Real-time dynamic shadows with configurable resolution (2048x2048)
- 🌍 **Cube Map Reflections** - Environment-mapped reflections on metallic surfaces
- 🗺️ **Normal Mapping** - Detailed surface lighting using normal maps
- 🎨 **Physically-Based Materials** - Diffuse, specular, and roughness properties
- 🌅 **Skybox Rendering** - Immersive environment backgrounds
- 🔦 **Multi-Light Support** - Dynamic directional and point lights

### Interactive Features
- 🖱️ **Runtime Object Picking** - Click to select 3D objects in the scene
- 🎯 **Real-time Translation** - Drag and move selected objects with mouse
- 📷 **Free Camera** - WASD movement with mouse look controls
- 💾 **Scene Serialization** - Save/load object positions to disk

### Asset Pipeline
- 📦 **Assimp Integration** - Load FBX, OBJ, and other 3D model formats
- 🖼️ **DDS Texture Support** - Optimized DirectX texture format
- 🔄 **Automatic Texture Conversion** - Runtime JPG/PNG to DDS conversion
- 🎨 **Material System** - Diffuse + Normal map material definitions

## Screenshots

The demo scene features:
- Winter environment with skybox
- Real-time reflective sphere (center)
- Shadow-casting objects (skull, crate, SMG)
- Dynamic lighting and reflections

## Requirements

### System Requirements
- **OS**: Windows 10/11 (64-bit)
- **GPU**: DirectX 12 compatible graphics card
- **RAM**: 4 GB minimum, 8 GB recommended
- **VRAM**: 2 GB minimum

### Development Requirements
- **Visual Studio 2022** or later
- **Windows SDK 10.0** or later
- **C++20** compiler support
- **vcpkg** package manager

## Building the Project

### 1. Clone the Repository
```bash
git clone https://github.com/yourusername/DirectxRenderer.git
cd DirectxRenderer
```

### 2. Install Dependencies
The project uses vcpkg for dependency management. Dependencies are defined in `vcpkg.json`:
- **assimp** - 3D model loading
- **directxtex** (with dx12 feature) - Texture processing

```bash
# Ensure vcpkg is integrated with Visual Studio
vcpkg integrate install

# Dependencies will be automatically installed when building
```

### 3. Build with Visual Studio
```bash
# Open the solution
start DirectxRenderer.sln

# Or build from command line
msbuild DirectxRenderer.sln /p:Configuration=Release /p:Platform=x64
```

### 4. Build with MSBuild (Command Line)
```bash
# Release build
msbuild DirectxRenderer.sln /p:Configuration=Release /p:Platform=x64

# Debug build
msbuild DirectxRenderer.sln /p:Configuration=Debug /p:Platform=x64
```

### Output
Compiled executable: `bin/x64/Release/DirectxRenderer.exe`

## Usage

### Camera Controls
| Key | Action |
|-----|--------|
| `W` | Move forward |
| `A` | Move left |
| `S` | Move backward |
| `D` | Move right |
| `Q` | Move up |
| `E` | Move down |
| `Mouse` | Look around (hold right-click) |

### Object Manipulation
| Action | Control |
|--------|---------|
| **Select Object** | Left-click on object |
| **Move Object** | Hold left-click + drag mouse |
| **Release Object** | Release left mouse button |

### Keyboard Shortcuts
| Key | Function |
|-----|----------|
| `1` | Toggle shadow map debug view |
| `Esc` | Exit application |

## Project Structure

```
DirectxRenderer/
├── src/
│   ├── Base/              # Core rendering infrastructure
│   │   ├── DxRenderBase.h/cpp    # Base DirectX 12 renderer
│   │   ├── Camera.h/cpp          # First-person camera
│   │   ├── FrameResource.h       # Triple buffering system
│   │   ├── ShadowMap.h/cpp       # Shadow mapping
│   │   └── CubeMapRt.h/cpp       # Cube map render targets
│   ├── Utility/           # Helper classes
│   │   ├── d3dUtil.h/cpp         # DirectX utilities
│   │   ├── MathHelper.h          # Math helper functions
│   │   ├── ModelImporter.h/cpp   # Assimp model loading
│   │   └── DDSTextureLoader.h/cpp # DDS texture loading
│   ├── Shaders/           # HLSL shader files
│   │   ├── ShapesApp.hlsl        # Main rendering shader
│   │   ├── ShadowMap.hlsl        # Shadow pass shader
│   │   ├── Skybox.hlsl           # Skybox shader
│   │   └── LightingUtil.hlsl     # Lighting calculations
│   ├── ShapesApp.h/cpp    # Main application
│   └── main.cpp           # Entry point
├── Assets/
│   ├── DDS/               # Texture files
│   ├── Models/            # 3D model files (FBX, OBJ)
│   └── RenderItems_metadata.txt  # Scene data
├── vcpkg.json             # Dependency manifest
└── DirectxRenderer.sln    # Visual Studio solution
```

## Architecture

### Inheritance-Based Design
```
DxRenderBase (Base Renderer)
    └── ShapesApp (Main Application)
```

### Rendering Pipeline
```
Initialize → Load Assets → Build Resources → Render Loop
                                                ├→ Update (Input, Camera, Animation)
                                                └→ Draw (Shadow Pass → Main Pass → Present)
```

### Key Components

**DxRenderBase**: Foundation class managing:
- Window creation and Win32 message loop
- DirectX 12 device, command queue, and swap chain
- Render target and depth-stencil descriptor heaps
- CPU-GPU synchronization via fencing

**ShapesApp**: Main application implementing:
- Camera system with WASD movement
- Object picking and runtime translation
- Shadow mapping and cube map reflections
- Material and texture management
- Frame resource triple buffering

## Technical Highlights

### Memory Management
- ✅ Smart pointers (`ComPtr`, `unique_ptr`) for automatic resource management
- ✅ Upload buffer cleanup after GPU copy completion
- ✅ Proper fence synchronization for CPU-GPU coordination
- ✅ Exception-safe resource handling

### Performance Optimizations
- ⚡ Reusable fence event handles (eliminates 60+ allocations/sec)
- ⚡ Triple buffering for CPU-GPU parallelism
- ⚡ Efficient descriptor heap management
- ⚡ Command list reuse and batching

### Error Handling
- 🛡️ Runtime exception handling (works in Release builds)
- 🛡️ Comprehensive HRESULT validation
- 🛡️ Null pointer safety checks
- 🛡️ Resource state validation

## Configuration

Key settings can be modified in `ShapesApp.cpp`:

```cpp
// Shadow map resolution
UINT DepthTextureWidth = 2048;
UINT DepthTextureHeight = 2048;

// Cube map resolution
UINT CubeMapWidth = 512;
UINT CubeMapHeight = 512;

// Frame resources (triple buffering)
UINT TotalFrameResources = 3;

// Maximum textures
static constexpr UINT MAX_TEXTURES = 512;
```

## Adding Custom Models

1. **Export your model** as FBX, OBJ, or other Assimp-supported format
2. **Place model file** in `Assets/Models/`
3. **Add textures** (DDS format) to `Assets/DDS/`
4. **Load in code** using `CreateModelGeometry()`:

```cpp
CreateModelGeometry("Assets\\Models\\YourModel.fbx", "ModelName");
```

## Dependencies

Managed via vcpkg:
- **assimp** - 3D model import library
- **directxtex** - DirectX texture processing

Standard libraries:
- DirectX 12 (`d3d12.lib`)
- DXGI (`dxgi.lib`)
- D3D Compiler (`d3dcompiler.lib`)

## Known Issues

- Window must maintain focus for keyboard input
- Texture directory must exist before running
- Some models may require texture path adjustments

## Future Enhancements

- [ ] Deferred rendering pipeline
- [ ] Screen-space ambient occlusion (SSAO)
- [ ] Particle system
- [ ] Post-processing effects (bloom, tone mapping)
- [ ] ImGui integration for runtime debugging
- [ ] Multithreaded command list generation
- [ ] Compute shader integration

## Contributing

Contributions are welcome! Please feel free to submit issues or pull requests.

## License

This project is open source and available for educational purposes.

## Acknowledgments

- **Frank Luna** - Camera implementation and DirectX 12 patterns from "Introduction to 3D Game Programming with DirectX 12"
- **Microsoft** - DirectX 12 SDK and documentation
- **Assimp Team** - Robust 3D model loading library

## Contact

For questions or feedback, please open an issue on GitHub.

---

**Built with ❤️ using DirectX 12 and Modern C++20**
