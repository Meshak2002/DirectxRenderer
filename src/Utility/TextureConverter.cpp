//***************************************************************************************
// TextureConverter.cpp
//
// Implementation of texture conversion utilities
//
// LEARNING GUIDE - The Texture Conversion Pipeline:
//
// Step 1: LOAD - Read image file from disk into memory
//         DirectXTex supports JPG, PNG, TGA, BMP, HDR, etc.
//         Creates a ScratchImage (CPU-side texture container)
//
// Step 2: DECOMPRESS - Convert to RGBA32 format
//         All images are normalized to a common format for processing
//
// Step 3: FLIP (optional) - Flip Y-axis if needed
//         Some formats store pixels top-down, DirectX expects bottom-up
//
// Step 4: PREMULTIPLY ALPHA (optional)
//         Multiply RGB channels by alpha: RGB = RGB * A
//         Important for proper alpha blending
//
// Step 5: GENERATE MIPMAPS - Create smaller versions
//         Mipmap chain: 1024x1024 → 512x512 → 256x256 → ... → 1x1
//         Improves performance and reduces aliasing at distance
//
// Step 6: COMPRESS - Apply block compression (BC1/BC3/BC5/BC7)
//         Reduces memory usage by 4:1 to 8:1
//         BC formats use 4x4 pixel blocks
//
// Step 7: SAVE - Write DDS file to disk
//         DDS = header + compressed texture data + mipmaps
//
//***************************************************************************************

#include "TextureConverter.h"
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <chrono>

namespace TextureConverter
{
    // ===== HELPER: Convert our enum to DirectXTex format =====
    DXGI_FORMAT CompressionFormatToDXGI(CompressionFormat format)
    {
        switch (format)
        {
        case CompressionFormat::BC1_UNORM:
            return DXGI_FORMAT_BC1_UNORM;  // DXT1: 8:1 compression, 1-bit alpha
        case CompressionFormat::BC3_UNORM:
            return DXGI_FORMAT_BC3_UNORM;  // DXT5: 4:1 compression, smooth alpha
        case CompressionFormat::BC5_UNORM:
            return DXGI_FORMAT_BC5_UNORM;  // 2-channel: perfect for normal maps
        case CompressionFormat::BC7_UNORM:
            return DXGI_FORMAT_BC7_UNORM;  // Best quality: 8:1 compression
        case CompressionFormat::UNCOMPRESSED:
            return DXGI_FORMAT_R8G8B8A8_UNORM;  // No compression: 32-bit RGBA
        default:
            return DXGI_FORMAT_BC7_UNORM;
        }
    }

    // ===== MAIN CONVERSION FUNCTION =====
    ConversionResult ConvertTexture(
        const std::string& inputPath,
        const std::string& outputPath,
        const ConversionOptions& options)
    {
        ConversionResult result;
        result.InputFile = inputPath;
        result.OutputFile = outputPath;

        namespace fs = std::filesystem;

        // Check if input file exists
        if (!fs::exists(inputPath))
        {
            result.ErrorMessage = "Input file does not exist: " + inputPath;
            return result;
        }

        // Check if output file exists and we shouldn't overwrite
        if (!options.OverwriteExisting && fs::exists(outputPath))
        {
            result.ErrorMessage = "Output file already exists (overwrite disabled): " + outputPath;
            return result;
        }

        try
        {
            // ===== STEP 1: LOAD IMAGE =====
            // DirectXTex::ScratchImage is a container for texture data in CPU memory
            // It holds the pixel data, format info, and metadata
            DirectX::ScratchImage srcImage;
            std::wstring wInputPath(inputPath.begin(), inputPath.end());
            HRESULT hr;

            // Determine file type and load accordingly
            std::string ext = fs::path(inputPath).extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

            if (ext == ".dds")
            {
                // Already a DDS file, just load it
                hr = DirectX::LoadFromDDSFile(wInputPath.c_str(),
                    DirectX::DDS_FLAGS_NONE, nullptr, srcImage);
            }
            else if (ext == ".tga")
            {
                // TGA format
                hr = DirectX::LoadFromTGAFile(wInputPath.c_str(), nullptr, srcImage);
            }
            else if (ext == ".hdr")
            {
                // HDR format (high dynamic range)
                hr = DirectX::LoadFromHDRFile(wInputPath.c_str(), nullptr, srcImage);
            }
            else
            {
                // WIC (Windows Imaging Component) handles JPG, PNG, BMP, etc.
                // This is the most common path for regular textures
                hr = DirectX::LoadFromWICFile(wInputPath.c_str(),
                    DirectX::WIC_FLAGS_NONE, nullptr, srcImage);
            }

            if (FAILED(hr))
            {
                result.ErrorMessage = "Failed to load image file. HRESULT: " + std::to_string(hr);
                return result;
            }

            // Store original image info
            const DirectX::TexMetadata& metadata = srcImage.GetMetadata();
            result.Width = static_cast<int>(metadata.width);
            result.Height = static_cast<int>(metadata.height);
            result.OriginalSize = srcImage.GetPixelsSize();

            std::cout << "Loaded: " << inputPath << " (" << result.Width << "x" << result.Height << ")" << std::endl;

            // ===== STEP 2: DECOMPRESS (if source is already compressed) =====
            // We need an uncompressed format for further processing
            DirectX::ScratchImage decompressedImage;
            if (DirectX::IsCompressed(metadata.format))
            {
                hr = DirectX::Decompress(srcImage.GetImages(), srcImage.GetImageCount(),
                    metadata, DXGI_FORMAT_R8G8B8A8_UNORM, decompressedImage);

                if (FAILED(hr))
                {
                    result.ErrorMessage = "Failed to decompress image. HRESULT: " + std::to_string(hr);
                    return result;
                }

                // Use decompressed image for next steps
                srcImage = std::move(decompressedImage);
            }

            // ===== STEP 3: FLIP VERTICAL (if requested) =====
            if (options.FlipVertical)
            {
                DirectX::ScratchImage flippedImage;
                hr = DirectX::FlipRotate(srcImage.GetImages(), srcImage.GetImageCount(),
                    srcImage.GetMetadata(), DirectX::TEX_FR_FLIP_VERTICAL, flippedImage);

                if (FAILED(hr))
                {
                    result.ErrorMessage = "Failed to flip image. HRESULT: " + std::to_string(hr);
                    return result;
                }

                srcImage = std::move(flippedImage);
            }

            // ===== STEP 4: PREMULTIPLY ALPHA (if requested) =====
            // Premultiplied alpha: RGB = RGB * A
            // Important for correct alpha blending: C_result = C_src + C_dst * (1 - A_src)
            if (options.PremultiplyAlpha)
            {
                DirectX::ScratchImage premultImage;
                hr = DirectX::PremultiplyAlpha(srcImage.GetImages(), srcImage.GetImageCount(),
                    srcImage.GetMetadata(), DirectX::TEX_PMALPHA_DEFAULT, premultImage);

                if (FAILED(hr))
                {
                    result.ErrorMessage = "Failed to premultiply alpha. HRESULT: " + std::to_string(hr);
                    return result;
                }

                srcImage = std::move(premultImage);
            }

            // ===== STEP 5: GENERATE MIPMAPS =====
            // Mipmaps are progressively smaller versions of the texture
            // GPU automatically selects appropriate mip level based on distance/screen size
            //
            // Example mipmap chain for 1024x1024 texture:
            // Level 0: 1024x1024 (full resolution)
            // Level 1: 512x512
            // Level 2: 256x256
            // Level 3: 128x128
            // ... down to 1x1
            //
            // Benefits:
            // - Reduces aliasing/flickering at distance
            // - Improves texture cache performance
            // - Reduces memory bandwidth
            DirectX::ScratchImage mipChain;
            if (options.GenerateMipmaps)
            {
                hr = DirectX::GenerateMipMaps(srcImage.GetImages(), srcImage.GetImageCount(),
                    srcImage.GetMetadata(), DirectX::TEX_FILTER_DEFAULT, 0, mipChain);

                if (FAILED(hr))
                {
                    result.ErrorMessage = "Failed to generate mipmaps. HRESULT: " + std::to_string(hr);
                    return result;
                }

                result.MipLevels = static_cast<int>(mipChain.GetMetadata().mipLevels);
                std::cout << "  Generated " << result.MipLevels << " mipmap levels" << std::endl;
            }
            else
            {
                mipChain = std::move(srcImage);
                result.MipLevels = 1;
            }

            // ===== STEP 6: COMPRESS =====
            // Block Compression (BC) reduces texture size by compressing 4x4 pixel blocks
            //
            // BC7 Compression Example:
            // - Input: 4x4 block = 16 pixels × 4 bytes (RGBA) = 64 bytes
            // - Output: 16 bytes (8:1 compression ratio)
            //
            // The compressor finds the best way to represent the block using:
            // - Color endpoints (2 colors defining a gradient)
            // - Index values (which color each pixel is closest to)
            // - Partition patterns (dividing block into regions)
            DirectX::ScratchImage compressedImage;
            DXGI_FORMAT targetFormat = CompressionFormatToDXGI(options.Format);

            if (options.Format != CompressionFormat::UNCOMPRESSED)
            {
                // Choose compression flags based on speed setting
                DirectX::TEX_COMPRESS_FLAGS compressFlags = DirectX::TEX_COMPRESS_DEFAULT;

                switch (options.Speed)
                {
                case CompressionSpeed::QUICK:
                    // QUICK mode: 10-20x faster than default!
                    // For BC7: Use QUICK flag + PARALLEL (multi-threaded)
                    // For BC1/BC3/BC5: Already fast, just use PARALLEL
                    if (targetFormat == DXGI_FORMAT_BC7_UNORM)
                    {
                        compressFlags = static_cast<DirectX::TEX_COMPRESS_FLAGS>(
                            DirectX::TEX_COMPRESS_BC7_QUICK | DirectX::TEX_COMPRESS_PARALLEL);
                    }
                    else
                    {
                        compressFlags = DirectX::TEX_COMPRESS_PARALLEL;  // Use parallel for other formats
                    }
                    break;
                case CompressionSpeed::DEFAULT:
                    compressFlags = DirectX::TEX_COMPRESS_PARALLEL;
                    break;
                case CompressionSpeed::SLOW:
                    // Maximum quality, very slow
                    if (targetFormat == DXGI_FORMAT_BC7_UNORM)
                    {
                        compressFlags = DirectX::TEX_COMPRESS_BC7_USE_3SUBSETS;
                    }
                    else
                    {
                        compressFlags = DirectX::TEX_COMPRESS_DEFAULT;
                    }
                    break;
                }

                std::cout << "  Compressing " << mipChain.GetImageCount() << " mip levels with "
                          << (options.Speed == CompressionSpeed::QUICK ? "QUICK" :
                              options.Speed == CompressionSpeed::DEFAULT ? "DEFAULT" : "SLOW")
                          << " mode..." << std::endl;

                auto startTime = std::chrono::high_resolution_clock::now();

                // Compress each mip level
                hr = DirectX::Compress(mipChain.GetImages(), mipChain.GetImageCount(),
                    mipChain.GetMetadata(), targetFormat,
                    compressFlags, DirectX::TEX_THRESHOLD_DEFAULT,
                    compressedImage);

                auto endTime = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

                if (FAILED(hr))
                {
                    result.ErrorMessage = "Failed to compress texture. HRESULT: " + std::to_string(hr);
                    return result;
                }

                std::cout << "  Compressed to " << FormatToString(options.Format)
                          << " in " << duration.count() << "ms" << std::endl;
            }
            else
            {
                compressedImage = std::move(mipChain);
            }

            // ===== STEP 7: SAVE DDS FILE =====
            // DDS file structure:
            // [Header: magic number, dimensions, format, flags]
            // [Mip Level 0: full resolution texture data]
            // [Mip Level 1: half resolution]
            // [Mip Level 2: quarter resolution]
            // ... etc.
            std::wstring wOutputPath(outputPath.begin(), outputPath.end());
            hr = DirectX::SaveToDDSFile(compressedImage.GetImages(),
                compressedImage.GetImageCount(),
                compressedImage.GetMetadata(),
                DirectX::DDS_FLAGS_NONE,
                wOutputPath.c_str());

            if (FAILED(hr))
            {
                result.ErrorMessage = "Failed to save DDS file. HRESULT: " + std::to_string(hr);
                return result;
            }

            // Get output file size
            if (fs::exists(outputPath))
            {
                result.CompressedSize = fs::file_size(outputPath);
                float compressionRatio = result.OriginalSize > 0 ?
                    static_cast<float>(result.OriginalSize) / result.CompressedSize : 1.0f;

                std::cout << "  Saved: " << outputPath << std::endl;
                std::cout << "  Size: " << result.OriginalSize << " → " << result.CompressedSize
                    << " bytes (" << compressionRatio << ":1 compression)" << std::endl;
            }

            result.Success = true;
        }
        catch (const std::exception& e)
        {
            result.ErrorMessage = std::string("Exception: ") + e.what();
        }

        return result;
    }

    // ===== BATCH CONVERT DIRECTORY =====
    std::vector<ConversionResult> ConvertDirectory(
        const std::string& inputDir,
        const std::string& outputDir,
        const ConversionOptions& options,
        bool recursive)
    {
        std::vector<ConversionResult> results;
        namespace fs = std::filesystem;

        if (!fs::exists(inputDir) || !fs::is_directory(inputDir))
        {
            ConversionResult errorResult;
            errorResult.ErrorMessage = "Input directory does not exist: " + inputDir;
            results.push_back(errorResult);
            return results;
        }

        std::string actualOutputDir = outputDir.empty() ? inputDir : outputDir;

        // Create output directory if it doesn't exist
        if (!fs::exists(actualOutputDir))
        {
            fs::create_directories(actualOutputDir);
        }

        // Lambda to process a single entry
        auto processEntry = [&](const fs::directory_entry& entry)
        {
            if (!entry.is_regular_file())
                return;

            std::string inputFile = entry.path().string();

            if (!IsSupportedImageFormat(inputFile))
                return;

            // Build output path
            std::string filename = entry.path().stem().string();
            std::string outputFile = actualOutputDir + "\\" + filename + ".dds";

            // Auto-detect format based on filename
            ConversionOptions autoOptions = options;
            autoOptions.Format = GetRecommendedFormat(inputFile);

            std::cout << "\nConverting: " << inputFile << std::endl;
            ConversionResult result = ConvertTexture(inputFile, outputFile, autoOptions);
            results.push_back(result);

            if (!result.Success)
            {
                std::cerr << "  ERROR: " << result.ErrorMessage << std::endl;
            }
        };

        // Iterate through files (recursive or non-recursive)
        if (recursive)
        {
            for (const auto& entry : fs::recursive_directory_iterator(inputDir))
            {
                processEntry(entry);
            }
        }
        else
        {
            for (const auto& entry : fs::directory_iterator(inputDir))
            {
                processEntry(entry);
            }
        }

        return results;
    }
    bool IsGivenFileaNormalMap(const std::string& filename)
    {
        std::string lower = filename;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        if (lower.find("normal") != std::string::npos ||
            lower.find("_n") != std::string::npos ||
            lower.find("_nrm") != std::string::npos)
        {
            return true;
        }
        return false;
    }
    bool IsGivenFileaCubeMap(const std::string& filename)
    {
        std::string lower = filename;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        if (lower.find("cube") != std::string::npos)
        {
            return true;
        }
        return false;
    }

    // ===== HELPER: Recommend format based on filename =====
    CompressionFormat GetRecommendedFormat(const std::string& filename)
    {
        std::string lower = filename;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        // Normal maps: use BC5 (2-channel compression optimized for normals)
        if (IsGivenFileaNormalMap(filename))
        {
            return CompressionFormat::BC5_UNORM;
        }

        // Roughness/Metallic/AO: single channel maps
        if (lower.find("rough") != std::string::npos ||
            lower.find("metal") != std::string::npos ||
            lower.find("ao") != std::string::npos ||
            lower.find("occlusion") != std::string::npos)
        {
            return CompressionFormat::BC7_UNORM;  // BC4 would be better but BC7 is more universal
        }

        // Default: BC7 for high-quality color textures
        return CompressionFormat::BC7_UNORM;
    }

    // ===== HELPER: Check if file format is supported =====
    bool IsSupportedImageFormat(const std::string& filename)
    {
        std::string ext = std::filesystem::path(filename).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        // Skip .dds files since they are already in the target format
        return ext == ".jpg" || ext == ".jpeg" || ext == ".png" ||
            ext == ".tga" || ext == ".bmp" || ext == ".hdr";
    }

    // ===== HELPER: Format to string =====
    std::string FormatToString(CompressionFormat format)
    {
        switch (format)
        {
        case CompressionFormat::BC1_UNORM: return "BC1 (DXT1)";
        case CompressionFormat::BC3_UNORM: return "BC3 (DXT5)";
        case CompressionFormat::BC5_UNORM: return "BC5 (Normal Map)";
        case CompressionFormat::BC7_UNORM: return "BC7 (High Quality)";
        case CompressionFormat::UNCOMPRESSED: return "Uncompressed RGBA";
        default: return "Unknown";
        }
    }
}
