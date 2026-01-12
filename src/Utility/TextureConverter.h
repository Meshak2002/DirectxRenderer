//***************************************************************************************
// TextureConverter.h
//
// Utility for converting common texture formats (JPG, PNG, TGA, BMP) to DDS
// Uses Microsoft's DirectXTex library for texture processing
//
// Educational Notes:
// - DDS (DirectDraw Surface) is a container format for GPU textures
// - BC (Block Compression) formats compress textures to save GPU memory
// - BC7 is best for color/diffuse maps (high quality, 8:1 compression)
// - BC5 is best for normal maps (2-channel, optimized for normals)
// - BC1 is faster but lower quality (used for simple textures)
//***************************************************************************************

#pragma once

#include <string>
#include <vector>
#include <DirectXTex.h>

namespace TextureConverter
{
    // ===== COMPRESSION FORMATS =====
    //
    // Different texture types need different compression:
    // - BC7: Best quality for diffuse/color textures (RGBA)
    // - BC5: Optimized for normal maps (stores X,Y normals)
    // - BC1: Fast, lower quality (no alpha or 1-bit alpha)
    // - BC3: Good for textures with smooth alpha
    // - BC4: Single channel (grayscale, like roughness maps)
    //
    enum class CompressionFormat
    {
        BC1_UNORM,      // Low quality, fast (DXT1)
        BC3_UNORM,      // Medium quality with alpha (DXT5)
        BC5_UNORM,      // Normal maps (2-channel)
        BC7_UNORM,      // Highest quality (default for color textures)
        UNCOMPRESSED    // No compression (largest file size)
    };

    // ===== COMPRESSION SPEED =====
    //
    // BC7 compression can be VERY slow. Choose speed vs quality tradeoff:
    // - QUICK: Fast compression (~1-2 seconds) - Good quality
    // - DEFAULT: Medium speed (~10+ seconds) - Better quality
    // - SLOW: Very slow (~30+ seconds) - Best quality
    //
    enum class CompressionSpeed
    {
        QUICK,      // Fast, good quality (recommended for development)
        DEFAULT,    // Medium speed, better quality
        SLOW        // Very slow, best quality (for final assets)
    };

    // ===== CONVERSION OPTIONS =====
    //
    // These settings control how the texture is processed
    //
    struct ConversionOptions
    {
        CompressionFormat Format = CompressionFormat::BC7_UNORM;
        CompressionSpeed Speed = CompressionSpeed::QUICK;  // Fast by default!

        // Generate mipmaps: smaller versions of the texture for distant objects
        // Mipmaps improve performance and reduce aliasing
        bool GenerateMipmaps = true;

        // Premultiply alpha: multiplies RGB by alpha channel
        // Required for proper blending in some rendering pipelines
        bool PremultiplyAlpha = false;

        // Flip vertically: some image formats store pixels top-to-bottom
        // DirectX expects bottom-to-top, so we may need to flip
        bool FlipVertical = false;

        // Overwrite existing DDS files
        bool OverwriteExisting = true;
    };

    // ===== CONVERSION RESULT =====
    //
    // Information about what happened during conversion
    //
    struct ConversionResult
    {
        bool Success = false;
        std::string ErrorMessage;
        std::string InputFile;
        std::string OutputFile;
        size_t OriginalSize = 0;     // Size in bytes of source image
        size_t CompressedSize = 0;   // Size in bytes of DDS file
        int Width = 0;
        int Height = 0;
        int MipLevels = 0;           // Number of mipmap levels generated
    };

    // ===== CORE FUNCTIONS =====

    // Convert a single image file to DDS
    //
    // How it works:
    // 1. Load the source image (JPG/PNG/TGA/BMP) into memory
    // 2. Optionally generate mipmaps (smaller versions for LOD)
    // 3. Compress to BC format (reduces memory usage on GPU)
    // 4. Save as .dds file
    //
    ConversionResult ConvertTexture(
        const std::string& inputPath,
        const std::string& outputPath,
        const ConversionOptions& options = ConversionOptions()
    );

    // Batch convert all textures in a directory
    //
    // Scans for: .jpg, .jpeg, .png, .tga, .bmp
    // Converts each to .dds in the same folder (or specified output folder)
    //
    std::vector<ConversionResult> ConvertDirectory(
        const std::string& inputDir,
        const std::string& outputDir = "",  // Empty = same as input
        const ConversionOptions& options = ConversionOptions(),
        bool recursive = false              // Process subdirectories?
    );

    // Helper: Get recommended compression format based on filename
    //
    // Detects texture type from name:
    // - "*_normal.png" → BC5 (normal map)
    // - "*_diffuse.jpg" → BC7 (color texture)
    // - "*_roughness.png" → BC4 (single channel)
    //
    bool IsGivenFileaNormalMap(const std::string& filename);
    bool IsGivenFileaCubeMap(const std::string& filename);

    CompressionFormat GetRecommendedFormat(const std::string& filename);

    // Helper: Check if file is a supported image format
    bool IsSupportedImageFormat(const std::string& filename);

    // Helper: Convert DXGI format enum to string (for debugging)
    std::string FormatToString(CompressionFormat format);
}
