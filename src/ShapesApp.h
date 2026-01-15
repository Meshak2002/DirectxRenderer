#pragma once
#include "Base/DxRenderBase.h"
#include <DirectXMath.h>
#include "Base/UploadBuffer.h"
#include "Base/FrameResource.h"
#include <optional>
#include <climits>
#include "Base/ShadowMap.h"
#include "Base/Camera.h"

// Maximum number of textures that can be bound at once
static constexpr UINT MAX_TEXTURES = 512;

enum class RenderLayer
{
	Opaque = 0,
	Skybox = 1,
	ShadowDebug = 2,
	Count = 3
};

class ShapesApp : public DxRenderBase
{
public:
	ShapesApp(HINSTANCE Instance);
	ShapesApp(const ShapesApp& ScreenApp) = delete;
	ShapesApp& operator=(ShapesApp& ScreenApp) = delete;

	~ShapesApp();

	virtual bool Initialize() override;
	virtual void Update(const GameTime& Gt) override;
	virtual void Draw(const GameTime& Gt) override;
	virtual void OnResize() override;

	void ProcessKeyboardInput(float DeltaTime);
	virtual void OnMouseDown(WPARAM BtnState, int X, int Y) override;
	virtual void OnMouseUp(WPARAM BtnState, int X, int Y) override;
	virtual void OnMouseMove(WPARAM BtnState, int X, int Y) override;

private:
	struct RenderItem;
	struct ObjConstBuffer;
	struct PassConstBuffer;
	struct MaterialConstBuffer;

	void BuildRootSignature();
	void BuildShadersAndInputLayout();
	void BuildGeometryResource();
	void BuildRenderItems();
	void BuildFrameResources();
	void BuildDescriptorHeap();
	void BuildPSO();
	//OnDraw
	void UpdateConstBuffers();
	void DrawRenderItems(ID3D12GraphicsCommandList* CommandList, std::vector<RenderItem*>& RenderItem);
	void DrawSceneToShadowMap();

	void InitCamera();
	void BuildTextures();
	void BuildDescriptors();
	void BuildMaterials();
	Material* GetMaterialForTexture(std::string TexName);

	UINT GetHeapIndexOfTexture(std::string TexName);

	Microsoft::WRL::ComPtr<ID3D12RootSignature> RootSignature;
	std::vector<D3D12_INPUT_ELEMENT_DESC> InputLayouts;
	std::unordered_map<std::string,Microsoft::WRL::ComPtr<ID3D12PipelineState>> PSO;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> SrvDescriptorHeap;


	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>> Shaders;
	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> MeshGeometries;
	std::unordered_map<std::string, std::unique_ptr<Texture>> Textures;
	std::unordered_map<std::string, std::unique_ptr<Material>> Materials;

	std::vector<std::unique_ptr<FrameResource<PassConstBuffer,ObjConstBuffer,MaterialConstBuffer>>> FrameResources;
	std::vector<std::unique_ptr<RenderItem>> RenderItems;
	std::vector<RenderItem*> RenderLayerItems[(int)RenderLayer::Count];
	std::vector<Texture*> DiffTexture2DCaches;
	std::string SkyBox = "Tex_sunsetcube1024";


	UINT TotalFrameResources = 3;
	UINT ShadowMapHeapIndex;
	UINT CurrentFrameResourceIndex{UINT_MAX};
	POINT MouseLastPos;
	std::unique_ptr<Camera> ViewCamera;
	std::unique_ptr<ShadowMap> ShadowMapObj;
	CD3DX12_GPU_DESCRIPTOR_HANDLE NullSrvGpuHandle;
	CD3DX12_GPU_DESCRIPTOR_HANDLE ShadowMapSrvGpuHandle;
	DirectX::BoundingSphere SceneSphereBound;

protected:
	FrameResource<PassConstBuffer,ObjConstBuffer,MaterialConstBuffer>* GetCurrentFrameResource() const;
};




///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct ShapesApp::RenderItem
{
	RenderItem() = default;
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
	UINT ObjConstBufferIndex = -1;
	MeshGeometry* MeshGeometryRef;
	Material* MaterialRef;

	//For Multiple Objects on Same MeshGeometryData
	UINT IndexCount;
	UINT IndexStartLocation;
	UINT VertexStartLocation;
};

struct ShapesApp::ObjConstBuffer
{
	DirectX::XMFLOAT4X4 World;
};

struct ShapesApp::PassConstBuffer
{
	DirectX::XMFLOAT4X4 View;
	DirectX::XMFLOAT4X4 Proj;
	DirectX::XMFLOAT4X4 ViewProj;
	DirectX::XMFLOAT4X4 ShadowTransform;
	DirectX::XMFLOAT3	Eye;
	float Padding;  // Align Lights array to 16-byte boundary for HLSL
	Light Lights[16];
};

struct ShapesApp::MaterialConstBuffer
{
	DirectX::XMFLOAT4 DiffuseAlbedo;
	DirectX::XMFLOAT3 FresnelRO;
	float Shinnines;

	UINT DiffuseTexIndex;
	UINT NormalTexIndex;
};