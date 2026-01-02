#pragma once
#include "Base/DxRenderBase.h"
#include <DirectXMath.h>
#include "Utility/Objects/UploadBuffer.h"
#include "Utility/Objects/FrameResource.h"
#include <optional>
#include <climits>

#include "Utility/Camera.h"

class ShapesApp : public DxRenderBase
{
public:
	ShapesApp(HINSTANCE Instance);
	ShapesApp(const ShapesApp& ScreenApp) = delete;
	ShapesApp& operator=(ShapesApp& ScreenApp) = delete;

	virtual ~ShapesApp() override;

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

	void BuildRootSignature();
	void BuildShadersAndInputLayout();
	void BuildGeometry();
	void BuildRenderItem();
	void BuildFrameResources();
	void BuildDescriptorHeap();
	void BuildDescriptors();
	void BuildPSO();

	void UpdateConstBuffers();
	void DrawRenderItems(ID3D12GraphicsCommandList* CommandList, std::vector<std::unique_ptr<RenderItem>>& RenderItem);


	Microsoft::WRL::ComPtr<ID3D12RootSignature> RootSignature;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> DescriptorHeap;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>> Shaders;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> PSO;
	std::vector<D3D12_INPUT_ELEMENT_DESC> InputLayouts;


	std::vector<std::unique_ptr<FrameResource<PassConstBuffer,ObjConstBuffer>>> FrameResources;
	UINT TotalFrameResources = 3;
	UINT CurrentFrameResourceIndex{UINT_MAX};
	
	std::vector<std::unique_ptr<RenderItem>> RenderItems;
	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> MeshGeometries;
	
	std::unique_ptr<Camera> ViewCamera;
	POINT MouseLastPos;
	

protected:
	FrameResource<PassConstBuffer,ObjConstBuffer>* GetCurrentFrameResource() const;
};




///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct ShapesApp::RenderItem
{
	RenderItem() = default;
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
	UINT ObjConstBufferIndex = -1;
	MeshGeometry* MeshGeometryData;

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
};