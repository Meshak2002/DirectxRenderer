//***************************************************************************************
// ModelImporter.h
//
// Utility for importing 3D models using Assimp library
// Supports FBX, OBJ, GLTF, and other formats supported by Assimp
//***************************************************************************************

#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "d3dUtil.h"
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <string>
#include <vector>
#include <memory>
#include <DirectXMath.h>

namespace ModelImporter
{
    // Call this before application shutdown to cleanup Assimp internal state
    inline void Cleanup()
    {
        aiDetachAllLogStreams();
    }
    // Vertex structure matching the current shader input layout
    // Position (3) + TexCoord (2) + Normal (3) + Tangent (3) = 11 floats
    struct Vertex
    {
        DirectX::XMFLOAT3 Position;
        DirectX::XMFLOAT2 TexCoord;
        DirectX::XMFLOAT3 Normal;
        DirectX::XMFLOAT3 Tangent;

        Vertex()
            : Position(0.0f, 0.0f, 0.0f)
            , TexCoord(0.0f, 0.0f)
            , Normal(0.0f, 1.0f, 0.0f)
            , Tangent(1.0f, 0.0f, 0.0f)
        {}

        Vertex(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT2& tex,
               const DirectX::XMFLOAT3& norm, const DirectX::XMFLOAT3& tan)
            : Position(pos), TexCoord(tex), Normal(norm), Tangent(tan)
        {}
    };

    // Material information extracted from model
    struct ModelMaterial
    {
        std::string Name;
        DirectX::XMFLOAT4 DiffuseColor;
        std::string DiffuseTexturePath;
        std::string NormalTexturePath;
        float Roughness;
        float Metallic;
    };

    // Model loading result
    struct ModelData
    {
        std::vector<Vertex> Vertices;
        std::vector<uint16_t> Indices16;  // For meshes with < 65536 vertices
        std::vector<uint32_t> Indices32;  // For larger meshes
        std::vector<ModelMaterial> Materials;
        std::string Name;
        bool Use32BitIndices = false;

        // Submesh information (if model contains multiple meshes)
        struct Submesh
        {
            std::string Name;
            UINT IndexCount;
            UINT StartIndexLocation;
            INT BaseVertexLocation;
            UINT MaterialIndex;
        };
        std::vector<Submesh> Submeshes;
    };

    // Import a 3D model from file
    // Returns ModelData containing vertices, indices, and materials
    // Supports FBX, OBJ, GLTF, DAE, and other Assimp-supported formats
    bool LoadModel(
        const std::string& filename,
        ModelData& outModelData,
        bool flipUVs = true,
        bool generateNormals = false,
        bool flipWindingOrder = false);

    // Convert ModelData to MeshGeometry for rendering
    // Requires DirectX device and command list for GPU buffer creation
    std::unique_ptr<MeshGeometry> CreateMeshGeometry(
        const ModelData& modelData,
        ID3D12Device* device,
        ID3D12GraphicsCommandList* cmdList,
        const std::string& geometryName);

    // Helper: Process a single Assimp mesh
    void ProcessMesh(
        const aiMesh* mesh,
        const aiScene* scene,
        std::vector<Vertex>& vertices,
        std::vector<uint32_t>& indices,
        UINT& baseVertex);

    // Helper: Process Assimp node recursively
    void ProcessNode(
        const aiNode* node,
        const aiScene* scene,
        std::vector<Vertex>& vertices,
        std::vector<uint32_t>& indices,
        std::vector<ModelData::Submesh>& submeshes);

    // Helper: Extract material information
    ModelMaterial ProcessMaterial(const aiMaterial* material, const std::string& modelDirectory);
}
