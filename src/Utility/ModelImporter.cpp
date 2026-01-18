//***************************************************************************************
// ModelImporter.cpp
//
// Implementation of model importing utilities using Assimp
//***************************************************************************************

#include "ModelImporter.h"
#include <filesystem>
#include <iostream>
#include <DirectXCollision.h>

namespace ModelImporter
{
    bool LoadModel(
        const std::string& filename,
        ModelData& outModelData,
        bool flipUVs,
        bool generateNormals,
        bool flipWindingOrder)
    {
        // Configure import flags
        unsigned int flags = aiProcess_Triangulate | aiProcess_JoinIdenticalVertices;

        if (flipUVs)
            flags |= aiProcess_FlipUVs;

        if (generateNormals)
            flags |= aiProcess_GenSmoothNormals;
        else
            flags |= aiProcess_GenNormals;  // Generate normals if not present

        if (flipWindingOrder)
            flags |= aiProcess_FlipWindingOrder;

        // Additional useful flags
        flags |= aiProcess_CalcTangentSpace;
        flags |= aiProcess_OptimizeMeshes;
        flags |= aiProcess_ValidateDataStructure;

        // Load the scene using C API for better memory management
        const aiScene* scene = aiImportFile(filename.c_str(), flags);

        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
        {
            const char* error = aiGetErrorString();
            std::cerr << "ERROR::ASSIMP::" << (error ? error : "Unknown error") << std::endl;
            if (scene)
                aiReleaseImport(scene);
            return false;
        }

        // Extract directory for texture loading
        std::filesystem::path filePath(filename);
        std::string directory = filePath.parent_path().string();
        outModelData.Name = filePath.stem().string();

        // Clear output data
        outModelData.Vertices.clear();
        outModelData.Indices32.clear();
        outModelData.Indices16.clear();
        outModelData.Materials.clear();
        outModelData.Submeshes.clear();

        // Temporary storage for 32-bit indices
        std::vector<uint32_t> tempIndices;
        std::vector<Vertex> tempVertices;

        // Process all meshes in the scene
        ProcessNode(scene->mRootNode, scene, tempVertices, tempIndices, outModelData.Submeshes);

        // Store vertices
        outModelData.Vertices = tempVertices;

        // Determine if we need 32-bit indices
        if (tempVertices.size() > 65535)
        {
            outModelData.Use32BitIndices = true;
            outModelData.Indices32 = tempIndices;
        }
        else
        {
            outModelData.Use32BitIndices = false;
            outModelData.Indices16.reserve(tempIndices.size());
            for (uint32_t idx : tempIndices)
                outModelData.Indices16.push_back(static_cast<uint16_t>(idx));
        }

        // Process materials
        for (unsigned int i = 0; i < scene->mNumMaterials; i++)
        {
            aiMaterial* material = scene->mMaterials[i];
            outModelData.Materials.push_back(ProcessMaterial(material, directory));
        }

        std::cout << "Model loaded successfully: " << filename << std::endl;
        std::cout << "  Vertices: " << outModelData.Vertices.size() << std::endl;
        std::cout << "  Indices: " << tempIndices.size() << std::endl;
        std::cout << "  Submeshes: " << outModelData.Submeshes.size() << std::endl;
        std::cout << "  Materials: " << outModelData.Materials.size() << std::endl;

        // Release the scene using C API
        // This properly frees all memory allocated by Assimp
        aiReleaseImport(scene);

        return true;
    }

    void ProcessNode(
        const aiNode* node,
        const aiScene* scene,
        std::vector<Vertex>& vertices,
        std::vector<uint32_t>& indices,
        std::vector<ModelData::Submesh>& submeshes)
    {
        // Process all meshes in this node
        for (unsigned int i = 0; i < node->mNumMeshes; i++)
        {
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];

            UINT baseVertex = static_cast<UINT>(vertices.size());
            UINT startIndex = static_cast<UINT>(indices.size());

            ProcessMesh(mesh, scene, vertices, indices, baseVertex);

            UINT indexCount = static_cast<UINT>(indices.size()) - startIndex;

            // Store submesh info
            ModelData::Submesh submesh;
            submesh.Name = mesh->mName.C_Str();
            submesh.BaseVertexLocation = baseVertex;
            submesh.StartIndexLocation = startIndex;
            submesh.IndexCount = indexCount;
            submesh.MaterialIndex = mesh->mMaterialIndex;
            submeshes.push_back(submesh);
        }

        // Recursively process child nodes
        for (unsigned int i = 0; i < node->mNumChildren; i++)
        {
            ProcessNode(node->mChildren[i], scene, vertices, indices, submeshes);
        }
    }

    void ProcessMesh(
        const aiMesh* mesh,
        const aiScene* scene,
        std::vector<Vertex>& vertices,
        std::vector<uint32_t>& indices,
        UINT& baseVertex)
    {
        // Process vertices
        for (unsigned int i = 0; i < mesh->mNumVertices; i++)
        {
            Vertex vertex;

            // Position
            vertex.Position.x = mesh->mVertices[i].x;
            vertex.Position.y = mesh->mVertices[i].y;
            vertex.Position.z = mesh->mVertices[i].z;

            // Normal
            if (mesh->HasNormals())
            {
                vertex.Normal.x = mesh->mNormals[i].x;
                vertex.Normal.y = mesh->mNormals[i].y;
                vertex.Normal.z = mesh->mNormals[i].z;
            }
            else
            {
                vertex.Normal = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);
            }

            // Texture coordinates (use first UV channel if available)
            if (mesh->mTextureCoords[0])
            {
                vertex.TexCoord.x = mesh->mTextureCoords[0][i].x;
                vertex.TexCoord.y = mesh->mTextureCoords[0][i].y;
            }
            else
            {
                vertex.TexCoord = DirectX::XMFLOAT2(0.0f, 0.0f);
            }

            // Tangent (generated by aiProcess_CalcTangentSpace)
            if (mesh->HasTangentsAndBitangents())
            {
                vertex.Tangent.x = mesh->mTangents[i].x;
                vertex.Tangent.y = mesh->mTangents[i].y;
                vertex.Tangent.z = mesh->mTangents[i].z;
            }
            else
            {
                // Default tangent pointing along X axis
                vertex.Tangent = DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f);
            }

            vertices.push_back(vertex);
        }

        // Process indices
        for (unsigned int i = 0; i < mesh->mNumFaces; i++)
        {
            aiFace face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; j++)
            {
                indices.push_back(baseVertex + face.mIndices[j]);
            }
        }
    }

    ModelMaterial ProcessMaterial(const aiMaterial* material, const std::string& modelDirectory)
    {
        ModelMaterial mat;

        // Get material name
        aiString name;
        material->Get(AI_MATKEY_NAME, name);
        mat.Name = name.C_Str();

        // Get diffuse color
        aiColor4D diffuse;
        if (AI_SUCCESS == material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse))
        {
            mat.DiffuseColor = DirectX::XMFLOAT4(diffuse.r, diffuse.g, diffuse.b, diffuse.a);
        }
        else
        {
            mat.DiffuseColor = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        }

        // Get roughness
        float roughness = 0.5f;
        material->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness);
        mat.Roughness = roughness;

        // Get metallic
        float metallic = 0.0f;
        material->Get(AI_MATKEY_METALLIC_FACTOR, metallic);
        mat.Metallic = metallic;

        // Get diffuse texture
        if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0)
        {
            aiString texPath;
            material->GetTexture(aiTextureType_DIFFUSE, 0, &texPath);
            std::filesystem::path fullPath = std::filesystem::path(modelDirectory) / texPath.C_Str();
            mat.DiffuseTexturePath = fullPath.string();
        }

        // Get normal map texture
        if (material->GetTextureCount(aiTextureType_NORMALS) > 0)
        {
            aiString texPath;
            material->GetTexture(aiTextureType_NORMALS, 0, &texPath);
            std::filesystem::path fullPath = std::filesystem::path(modelDirectory) / texPath.C_Str();
            mat.NormalTexturePath = fullPath.string();
        }
        else if (material->GetTextureCount(aiTextureType_HEIGHT) > 0)
        {
            // Some formats use height maps as normal maps
            aiString texPath;
            material->GetTexture(aiTextureType_HEIGHT, 0, &texPath);
            std::filesystem::path fullPath = std::filesystem::path(modelDirectory) / texPath.C_Str();
            mat.NormalTexturePath = fullPath.string();
        }

        return mat;
    }

    DirectX::BoundingBox CalculateBoundsForSubmesh(
        const std::vector<Vertex>& vertices,
        UINT baseVertexLocation,
        UINT indexCount,
        const std::vector<uint16_t>& indices16,
        const std::vector<uint32_t>& indices32,
        UINT startIndexLocation,
        bool use32BitIndices)
    {
        using namespace DirectX;

        if (vertices.empty())
        {
            return BoundingBox(XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 0.0f));
        }

        // Get the first vertex position for this submesh
        UINT firstIndex = use32BitIndices ? indices32[startIndexLocation] : indices16[startIndexLocation];
        XMVECTOR minBounds = XMLoadFloat3(&vertices[firstIndex].Position);
        XMVECTOR maxBounds = minBounds;

        // Iterate through all indices for this submesh
        for (UINT i = 0; i < indexCount; ++i)
        {
            UINT vertexIndex = use32BitIndices ?
                indices32[startIndexLocation + i] :
                indices16[startIndexLocation + i];

            XMVECTOR pos = XMLoadFloat3(&vertices[vertexIndex].Position);
            minBounds = XMVectorMin(minBounds, pos);
            maxBounds = XMVectorMax(maxBounds, pos);
        }

        XMVECTOR center = XMVectorScale(XMVectorAdd(minBounds, maxBounds), 0.5f);
        XMVECTOR extents = XMVectorScale(XMVectorSubtract(maxBounds, minBounds), 0.5f);

        XMFLOAT3 centerFloat;
        XMFLOAT3 extentsFloat;
        XMStoreFloat3(&centerFloat, center);
        XMStoreFloat3(&extentsFloat, extents);

        return BoundingBox(centerFloat, extentsFloat);
    }

    std::unique_ptr<MeshGeometry> CreateMeshGeometry(
        const ModelData& modelData,
        ID3D12Device* device,
        ID3D12GraphicsCommandList* cmdList,
        const std::string& geometryName)
    {
        auto meshGeometry = std::make_unique<MeshGeometry>();
        meshGeometry->Name = geometryName;

        // Convert vertices to raw float array (matching current vertex layout)
        std::vector<float> vertexData;
        vertexData.reserve(modelData.Vertices.size() * 11); // 11 floats per vertex

        for (const auto& v : modelData.Vertices)
        {
            // Position (3 floats)
            vertexData.push_back(v.Position.x);
            vertexData.push_back(v.Position.y);
            vertexData.push_back(v.Position.z);

            // TexCoord (2 floats)
            vertexData.push_back(v.TexCoord.x);
            vertexData.push_back(v.TexCoord.y);

            // Normal (3 floats)
            vertexData.push_back(v.Normal.x);
            vertexData.push_back(v.Normal.y);
            vertexData.push_back(v.Normal.z);

            // Tangent (3 floats)
            vertexData.push_back(v.Tangent.x);
            vertexData.push_back(v.Tangent.y);
            vertexData.push_back(v.Tangent.z);
        }

        // Set vertex buffer properties
        meshGeometry->VertexByteStride = 11 * sizeof(float);
        meshGeometry->VertexBufferByteSize = static_cast<UINT>(vertexData.size() * sizeof(float));

        ThrowIfFailed(D3DCreateBlob(meshGeometry->VertexBufferByteSize, &meshGeometry->VertexBufferCPU));
        memcpy(meshGeometry->VertexBufferCPU->GetBufferPointer(), vertexData.data(), meshGeometry->VertexBufferByteSize);

        meshGeometry->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(
            device, cmdList,
            vertexData.data(),
            meshGeometry->VertexBufferByteSize,
            meshGeometry->VertexBufferUploader);

        // Set index buffer properties
        if (modelData.Use32BitIndices)
        {
            meshGeometry->IndexFormat = DXGI_FORMAT_R32_UINT;
            meshGeometry->IndexBufferByteSize = static_cast<UINT>(modelData.Indices32.size() * sizeof(uint32_t));

            ThrowIfFailed(D3DCreateBlob(meshGeometry->IndexBufferByteSize, &meshGeometry->IndexBufferCPU));
            memcpy(meshGeometry->IndexBufferCPU->GetBufferPointer(), modelData.Indices32.data(), meshGeometry->IndexBufferByteSize);

            meshGeometry->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(
                device, cmdList,
                modelData.Indices32.data(),
                meshGeometry->IndexBufferByteSize,
                meshGeometry->IndexBufferUploader);
        }
        else
        {
            meshGeometry->IndexFormat = DXGI_FORMAT_R16_UINT;
            meshGeometry->IndexBufferByteSize = static_cast<UINT>(modelData.Indices16.size() * sizeof(uint16_t));

            ThrowIfFailed(D3DCreateBlob(meshGeometry->IndexBufferByteSize, &meshGeometry->IndexBufferCPU));
            memcpy(meshGeometry->IndexBufferCPU->GetBufferPointer(), modelData.Indices16.data(), meshGeometry->IndexBufferByteSize);

            meshGeometry->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(
                device, cmdList,
                modelData.Indices16.data(),
                meshGeometry->IndexBufferByteSize,
                meshGeometry->IndexBufferUploader);
        }

        // Create submesh entries
        if (modelData.Submeshes.empty())
        {
            // Single mesh, create default submesh
            SubmeshGeometry submesh;
            submesh.IndexCount = modelData.Use32BitIndices ?
                static_cast<UINT>(modelData.Indices32.size()) :
                static_cast<UINT>(modelData.Indices16.size());
            submesh.StartIndexLocation = 0;
            submesh.BaseVertexLocation = 0;

            // Calculate bounds for the entire mesh
            submesh.Bounds = CalculateBoundsForSubmesh(
                modelData.Vertices,
                submesh.BaseVertexLocation,
                submesh.IndexCount,
                modelData.Indices16,
                modelData.Indices32,
                submesh.StartIndexLocation,
                modelData.Use32BitIndices);

            meshGeometry->DrawArgs["Default"] = submesh;
        }
        else
        {
            // Multiple submeshes
            for (const auto& sub : modelData.Submeshes)
            {
                SubmeshGeometry submesh;
                submesh.IndexCount = sub.IndexCount;
                submesh.StartIndexLocation = sub.StartIndexLocation;
                submesh.BaseVertexLocation = sub.BaseVertexLocation;

                // Calculate bounds for this submesh
                submesh.Bounds = CalculateBoundsForSubmesh(
                    modelData.Vertices,
                    submesh.BaseVertexLocation,
                    submesh.IndexCount,
                    modelData.Indices16,
                    modelData.Indices32,
                    submesh.StartIndexLocation,
                    modelData.Use32BitIndices);

                std::string submeshName = sub.Name.empty() ? ("Submesh_" + std::to_string(meshGeometry->DrawArgs.size())) : sub.Name;
                meshGeometry->DrawArgs[submeshName] = submesh;
            }
        }

        return meshGeometry;
    }
}
