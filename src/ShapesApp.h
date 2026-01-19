#pragma once
#include "Base/DxRenderBase.h"
#include <DirectXMath.h>
#include "Base/UploadBuffer.h"
#include "Base/FrameResource.h"
#include <optional>
#include <climits>
#include "Base/ShadowMap.h"
#include "Base/CubeMapRt.h"
#include "Base/Camera.h"

// Maximum number of textures that can be bound at once
static constexpr UINT MAX_TEXTURES = 512;

enum class RenderLayer
{
	Opaque = 0,
	Skybox = 1,
	ShadowDebug = 2,
	Reflection = 3,
	Count = 4
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
	virtual void CreateRtvDsvHeap() override;

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
	void CreateModelGeometry(std::string Path, std::string GeomertryName);
	void BuildGeometryResource();
	void BuildRenderItems();
	void BuildFrameResources();
	void BuildDescriptorHeap();
	void BuildPSO();
	//OnDraw
	void UpdateConstBuffers();
	void DrawRenderItems(ID3D12GraphicsCommandList* CommandList, std::vector<RenderItem*>& RenderItem);
	void DrawSceneToShadowMap();
	void DrawSceneToCubeMap();

	void Pick(int X, int Y);
	void MovePickedObj(float X, float Y, float Z , bool bInLocalSpace=true);
	void RotatePickedObj(float Pitch, float Yaw, float Roll);
	void ScalePickedObj(float ScaleX, float ScaleY, float ScaleZ);
	void InitCamera();
	void InitCubeMapCameras(float CenterX , float CenterY, float CenterZ);
	void BuildTextures();
	void BuildDescriptors();
	Material* BuildOrGetMaterial(std::string aMatName, std::string aDiffuseTexName, std::string aNormalTexName,
		float aDiffuseAlbedo = 1, float aFresnalRO = .5f, float aShininess = .5f, float aUvTileValue = 1.0f);
	Material* GetMaterial(std::string aMaterialName);
	Texture* GetTexture(std::string aTextureName);
	bool AddTexture(std::unique_ptr<Texture> aTexture);
	void SaveRenderItemsData();
	void LoadRenderItemsData();
	RenderItem* AddRenderItem(std::unique_ptr<RenderItem> aRenderItem, RenderLayer aLayer);
	void ModelToRenderItem(const std::string& meshKey, UINT& objIndex, Material* material,
		const DirectX::XMMATRIX& worldTransform, RenderLayer layer = RenderLayer::Opaque);


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
	std::vector<Texture*> Texture2DStack;
	std::string SkyBox = "Tex_sunsetcube1024";
	RenderItem* PickedRenderItem = nullptr;

	UINT TotalFrameResources = 3;
	UINT ShadowSkyMapHeapIndex;
	UINT ShadowCubeMapHeapIndex;
	UINT SrvCubeMapHeapIndex;

	UINT CurrentFrameResourceIndex{UINT_MAX};
	POINT MouseLastPos;
	bool bDebugShadowMap=false;
	bool bLeftMouseDown=false;
	std::unique_ptr<Camera> ViewCamera;
	std::unique_ptr<Camera> CubeMapCameras[6];
	std::unique_ptr<ShadowMap> ShadowMapObj;
	std::unique_ptr<CubeMapRT> CubeMapObj;
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
	std::string Name;
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
	UINT ObjConstBufferIndex = -1;
	MeshGeometry* MeshGeometryRef;
	Material* MaterialRef;
	DirectX::BoundingBox Bounds;

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
	float Shininess;
	float UvTileValue = 1.0f;  // Changed to float for fractional tiling
	UINT DiffuseTexIndex;
	UINT NormalTexIndex;
	UINT Padding;  // Pad to 16-byte alignment (48 bytes total)
};