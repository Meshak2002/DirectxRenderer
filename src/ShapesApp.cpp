#include "ShapesApp.h"
#include "DirectXMath.h"
#include "iostream"
#include <filesystem>

const int gNumFrameResources = 3;

ShapesApp::ShapesApp(HINSTANCE ScreenInstance) : DxRenderBase(ScreenInstance)
{
	ViewCamera = std::make_unique<Camera>();
}

ShapesApp::~ShapesApp()
{
	if (DxDevice3D)
		FlushCommandQueue();
}

void ShapesApp::ProcessKeyboardInput(float DeltaTime)
{
	float MovementSpeed = 10 * DeltaTime;
	if (GetAsyncKeyState('W') & 0x8000)
		ViewCamera->Walk(MovementSpeed);
	if (GetAsyncKeyState('S') & 0x8000)
		ViewCamera->Walk(-MovementSpeed);
	if (GetAsyncKeyState('A') & 0x8000)
		ViewCamera->Strafe(-MovementSpeed);
	if (GetAsyncKeyState('D') & 0x8000)
		ViewCamera->Strafe(MovementSpeed);
	if (GetAsyncKeyState('Q') & 0x8000)
		ViewCamera->Fly(-MovementSpeed);
	if (GetAsyncKeyState('E') & 0x8000)
		ViewCamera->Fly(MovementSpeed);

	ViewCamera->UpdateViewMatrix();
}

void ShapesApp::OnMouseDown(WPARAM BtnState, int X, int Y)
{
	MouseLastPos.x = X;
	MouseLastPos.y = Y;

	SetCapture(MainWindowHandle);
}

void ShapesApp::OnMouseUp(WPARAM BtnState, int X, int Y)
{
	ReleaseCapture();
}

void ShapesApp::OnMouseMove(WPARAM BtnState, int X, int Y)
{
	if ((BtnState & MK_RBUTTON) != 0)
	{
		float VarianceOnX = (float)(MouseLastPos.x - X);
		float VarianceOnY = (float)(MouseLastPos.y - Y);
		
		VarianceOnX = DirectX::XMConvertToRadians(0.25f * VarianceOnX);
		VarianceOnY = DirectX::XMConvertToRadians(0.25f * VarianceOnY);

		ViewCamera->Pitch(-VarianceOnY);
		ViewCamera->Yaw(-VarianceOnX);
		ViewCamera->UpdateViewMatrix();

		MouseLastPos.x = X;
		MouseLastPos.y = Y;
	}
}

void ShapesApp::OnResize()
{
	DxRenderBase::OnResize();
	ViewCamera->SetLens(0.25f * DirectX::XM_PI, AspectRatio(), 1, 1000);
}

bool ShapesApp::Initialize()
{
	if (!DxRenderBase::Initialize())
		return false;

	InitCamera();

	ThrowIfFailed(CommandList->Reset(CommandAlloc.Get(), nullptr));

	BuildRootSignature();
	BuildShadersAndInputLayout();
	BuildDescriptorHeap();

	BuildGeometryResource();
	BuildTextures();
	BuildMaterials();
	BuildRenderItems();

	BuildFrameResources();
	BuildPSO();

	CommandList->Close();
	ID3D12CommandList* Commands[] = {CommandList.Get()};
	CommandQueue->ExecuteCommandLists(_countof(Commands), Commands) ;

	FlushCommandQueue();
	return true;
}

void ShapesApp::InitCamera()
{
	ViewCamera->SetPosition(0, 0, -15);
	ViewCamera->LookAt(
		ViewCamera->GetPosition3f(),
		DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
		DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f));
	ViewCamera->UpdateViewMatrix();
}

void ShapesApp::BuildTextures()
{
	UINT TexturesCount = 0;
	auto DescHeapCpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE( DescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	std::string TextureDirectory = "Assets\\Textures\\Diffuse";
	assert(std::filesystem::exists(TextureDirectory));

	//Create Diffuse Textures
	for (auto Entry : std::filesystem::directory_iterator(TextureDirectory))
	{
		if (!Entry.is_regular_file() || Entry.path().extension() != ".dds")
			continue;
		auto NewTexture = std::make_unique<Texture>();
		NewTexture->Name =  "Tex" + Entry.path().stem().string();
		NewTexture->Filename = Entry.path().wstring();
		NewTexture->HeapIndex = TexturesCount;
		ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(DxDevice3D.Get(), CommandList.Get(),
			NewTexture->Filename.c_str(), NewTexture->Resource, NewTexture->UploadHeap));

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = NewTexture->Resource.Get()->GetDesc().Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = NewTexture->Resource.Get()->GetDesc().MipLevels;
		srvDesc.Texture2D.ResourceMinLODClamp = 0;
		DxDevice3D->CreateShaderResourceView(NewTexture->Resource.Get(), &srvDesc, DescHeapCpuHandle);
		DescHeapCpuHandle.Offset(1, CbvSrvUavDescriptorSize);
		Textures[NewTexture->Name] = move(NewTexture);
		TexturesCount++;
	}
}

void ShapesApp::BuildMaterials()
{
	//Build For All Textures :
	for(const auto& [Key,Value] : Textures)
	{
		auto NewMaterial = std::make_unique<Material>();
		NewMaterial->DiffuseSrvHeapIndex = Textures[Key]->HeapIndex;
		NewMaterial->DiffuseAlbedo = DirectX::XMFLOAT4(1,1,1,1);
		NewMaterial->FresnelR0 = DirectX::XMFLOAT3(0.2f, 0.2f, 0.2f);  // Increased for more visible specular
		NewMaterial->Roughness = 0.1f;  // Lower roughness = sharper, more visible highlights
		Materials[std::string("Mat")+ Key] = move(NewMaterial);
	}
}

Material* ShapesApp::GetMaterialForTexture(std::string TexName)
{
	auto MaterialRef =  Materials[std::string("Mat") + TexName].get();
	if(!MaterialRef)
	{
		std::string ErrorMsg = "Material for the given Texture name is not present: " + TexName + "\n";
		::OutputDebugStringA(ErrorMsg.c_str());
		assert(MaterialRef && "Material for the given Texture name is not present");
	}
	return MaterialRef;
}

void ShapesApp::Update(const GameTime& Gt)
{
	ProcessKeyboardInput(Gt.GetDeltaTime());

	CurrentFrameResourceIndex = (CurrentFrameResourceIndex + 1) % TotalFrameResources;

	if (GetCurrentFrameResource()->FenceValue != 0 && Fence->GetCompletedValue() < GetCurrentFrameResource()->FenceValue)
	{
		HANDLE EventHandle = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
		Fence->SetEventOnCompletion(GetCurrentFrameResource()->FenceValue, EventHandle);
		WaitForSingleObject(EventHandle, INFINITE);
		CloseHandle(EventHandle);
	}

	UpdateConstBuffers();
}

void ShapesApp::UpdateConstBuffers()
{
	auto PassConstBufferRes = GetCurrentFrameResource()->PassConstBufferRes.get();
	auto ObjConstBufferRes = GetCurrentFrameResource()->ObjConstBufferRes.get();
	auto MatConstBufferRes = GetCurrentFrameResource()->MatConstBufferRes.get();
	int ObjConstBufferIndex = 0;


	DirectX::XMMATRIX XView = ViewCamera->GetView();
	DirectX::XMMATRIX XProj = ViewCamera->GetProj();
	DirectX::XMMATRIX XViewProj = DirectX::XMMatrixMultiply(XView, XProj);
	auto EyePos = ViewCamera->GetPosition3f();

	PassConstBuffer PassConstBufferData = {};
	// Transpose before sending to GPU! Which changes row majour to column majour
	DirectX::XMStoreFloat4x4(&PassConstBufferData.View, DirectX::XMMatrixTranspose(XView));
	DirectX::XMStoreFloat4x4(&PassConstBufferData.Proj, DirectX::XMMatrixTranspose(XProj));
	DirectX::XMStoreFloat4x4(&PassConstBufferData.ViewProj, DirectX::XMMatrixTranspose(XViewProj));
	PassConstBufferData.Eye = EyePos;

	// Light 0: Upper-right key light (creates specular highlights on plane)
	PassConstBufferData.Lights[0].Direction = { -0.57735f, -0.57735f, -0.57735f };  // normalized(-1, -1, -1)
	PassConstBufferData.Lights[0].Strength = { 0.4f, 0.4f, 0.4f };

	// Light 1: Top-down light (illuminates floor)
	PassConstBufferData.Lights[1].Direction = { 0, 1, 0 };
	PassConstBufferData.Lights[1].Strength = { 0.7f, 0.7f, 0.7f };

	// Light 2: Soft fill light from left (reduces harsh shadows)
	PassConstBufferData.Lights[2].Direction = { 0.7071f, -0.0f, 0.7071f };  // normalized(1, 0, -1)
	PassConstBufferData.Lights[2].Strength = { 0.8f, 0.8f, 0.8f };

	PassConstBufferRes->CopyData(0, PassConstBufferData);


	for (auto& RenderItem : RenderItems)
	{
		DirectX::XMMATRIX XWorld = DirectX::XMLoadFloat4x4(&RenderItem->World);
		ObjConstBuffer  ObjConstBufferData;
		DirectX::XMStoreFloat4x4(&ObjConstBufferData.World, DirectX::XMMatrixTranspose(XWorld));
		ObjConstBufferRes->CopyData(ObjConstBufferIndex, ObjConstBufferData);

		MaterialConstBuffer MatConstBufferData
		{
			RenderItem->MaterialRef->DiffuseAlbedo,
			RenderItem->MaterialRef->FresnelR0,
			RenderItem->MaterialRef->Roughness
		};
		MatConstBufferRes->CopyData(ObjConstBufferIndex, MatConstBufferData);
		ObjConstBufferIndex++;
	}
}

void ShapesApp::Draw(const GameTime& Gt)
{
	auto CurrentFrameResource = GetCurrentFrameResource();

	ThrowIfFailed(CurrentFrameResource->CommandAlloc->Reset());
	ThrowIfFailed(CommandList->Reset(CurrentFrameResource->CommandAlloc.Get(), PSO.Get()));

	auto Barier = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBufferResource(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	CommandList->ResourceBarrier(1, &Barier);

	CommandList->RSSetViewports(1, &Viewport);
	CommandList->RSSetScissorRects(1, &ScissorRect);

	CommandList->ClearRenderTargetView(CurrentBackBufferHeapDescHandle(), DirectX::Colors::BurlyWood, 0, nullptr);
	CommandList->ClearDepthStencilView(DepthStencilHeapDescHandle(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
		1, 0, 0, nullptr);

	auto Rtv = CurrentBackBufferHeapDescHandle();
	auto Dsv = DepthStencilHeapDescHandle();
	CommandList->OMSetRenderTargets(1, &Rtv, true, &Dsv);

	ID3D12DescriptorHeap* DescHeap[] = {DescriptorHeap.Get()};
	CommandList->SetDescriptorHeaps(_countof(DescHeap), DescHeap);

	CommandList->SetGraphicsRootSignature(RootSignature.Get());

	DrawRenderItems(CommandList.Get(), RenderItems);

	auto Barier2 = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBufferResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	CommandList->ResourceBarrier(1, &Barier2);

	CommandList->Close();
	ID3D12CommandList* Commands[] = { CommandList.Get() };
	CommandQueue->ExecuteCommandLists(_countof(Commands), Commands);

	ThrowIfFailed(SwapChain->Present(0, 0));
	CurrentBackBuffer = (CurrentBackBuffer + 1) % 2;

	CurrentFrameResource->FenceValue = ++CurrentFenceValue;
	CommandQueue->Signal(Fence.Get(), CurrentFenceValue);

}

void ShapesApp::DrawRenderItems(ID3D12GraphicsCommandList* CommandList, std::vector<std::unique_ptr<RenderItem>>& RenderItem)
{
	auto PassConstBufferRes = GetCurrentFrameResource()->PassConstBufferRes.get() ;
	auto PassBufferGpuAddress = PassConstBufferRes->GetResourceGpuAddress();
	CommandList->SetGraphicsRootConstantBufferView(0, PassBufferGpuAddress);

	auto ObjConstBufferRes = GetCurrentFrameResource()->ObjConstBufferRes.get() ;
	auto MatConstBufferRes = GetCurrentFrameResource()->MatConstBufferRes.get() ;
	auto DescHeapGpuAddress = DescriptorHeap->GetGPUDescriptorHandleForHeapStart();

	for (auto& RItem : RenderItem)
	{
		auto vbv = RItem->MeshGeometryRef->VertexBufferView();
		CommandList->IASetVertexBuffers(0, 1, &vbv);
		auto ibv = RItem->MeshGeometryRef->IndexBufferView();
		CommandList->IASetIndexBuffer(&ibv);
		CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		auto ObjConstBufferSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjConstBuffer));
		auto ObjBufferGpuAddress = ObjConstBufferRes->GetResourceGpuAddress() + ObjConstBufferSize * RItem->ObjConstBufferIndex;
		CommandList->SetGraphicsRootConstantBufferView(1, ObjBufferGpuAddress);

		auto DescriptorTableGpuAddress = CD3DX12_GPU_DESCRIPTOR_HANDLE( DescHeapGpuAddress );
		DescriptorTableGpuAddress.Offset(RItem->MaterialRef->DiffuseSrvHeapIndex, CbvSrvUavDescriptorSize);
		CommandList->SetGraphicsRootDescriptorTable(2, DescriptorTableGpuAddress);

		auto MatConstBufferSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstBuffer));
		auto MatBufferGpuAddress = MatConstBufferRes->GetResourceGpuAddress() + MatConstBufferSize * RItem->ObjConstBufferIndex;
		CommandList->SetGraphicsRootConstantBufferView(3, MatBufferGpuAddress);

		CommandList->DrawIndexedInstanced(RItem->IndexCount, 1, RItem->IndexStartLocation, RItem->VertexStartLocation, 0);
	}
}

FrameResource<ShapesApp::PassConstBuffer,ShapesApp::ObjConstBuffer,ShapesApp::MaterialConstBuffer>* ShapesApp::GetCurrentFrameResource() const
{
	assert((CurrentFrameResourceIndex>=0 && CurrentFrameResourceIndex < FrameResources.size()) && "Trying to get FrameRes REF with an invalid Index");
	return FrameResources[CurrentFrameResourceIndex].get();
}

void ShapesApp::BuildRootSignature()
{
	const size_t TotalRootParameters = 4;
	CD3DX12_ROOT_PARAMETER RootParameter[TotalRootParameters];
	RootParameter[0].InitAsConstantBufferView(0,0);
	RootParameter[1].InitAsConstantBufferView(1,0);

	CD3DX12_DESCRIPTOR_RANGE TextureDescTable;
	TextureDescTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
	RootParameter[2].InitAsDescriptorTable(1,&TextureDescTable);

	RootParameter[3].InitAsConstantBufferView(2,0);	//Material

	auto Samplers = d3dUtil::GetStaticSamplers();
	CD3DX12_ROOT_SIGNATURE_DESC RootSignatureDesc(TotalRootParameters, RootParameter, Samplers.size() , Samplers.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	Microsoft::WRL::ComPtr<ID3DBlob> SignatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> ErrorBlob;
	ThrowIfFailed( D3D12SerializeRootSignature(&RootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1 
		, SignatureBlob.GetAddressOf(), ErrorBlob.GetAddressOf()));
	ThrowIfFailed( DxDevice3D->CreateRootSignature(0, SignatureBlob->GetBufferPointer(),
		SignatureBlob->GetBufferSize(), IID_PPV_ARGS(RootSignature.GetAddressOf() )));
}

void ShapesApp::BuildShadersAndInputLayout()
{
	InputLayouts.push_back(
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,
			0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}	);

	InputLayouts.push_back(
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT,
			0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}	);

	InputLayouts.push_back(
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,
			0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}	);
	InputLayouts.push_back(
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT,
			0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}	);

	Shaders["Vertex"] = d3dUtil::CompileShader(L"src\\Shaders\\ShapesApp.hlsl", nullptr, "VS", "vs_5_1");
	Shaders["Pixel"] = d3dUtil::CompileShader(L"src\\Shaders\\ShapesApp.hlsl", nullptr, "PS", "ps_5_1");
}

void ShapesApp::BuildGeometryResource()
{
	// Cube geometry with regular UVs (no tiling)
	auto CubeMeshGeo = std::make_unique<MeshGeometry>();
	CubeMeshGeo->Name = "Cube";
	const std::vector<float> CubeVertices =
	{
		// Position (x,y,z)   Color (r,g,b,a)        TexCoord (u,v)  Normal (x,y,z)

		// Front face (+Z)
		-0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 0.0f, 1.0f,  0.0f, 1.0f,  0.0f, 0.0f, 1.0f,  // 0
		 0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 0.0f, 1.0f,  1.0f, 1.0f,  0.0f, 0.0f, 1.0f,  // 1
		 0.5f,  0.5f,  0.5f,  0.0f, 0.0f, 1.0f, 1.0f,  1.0f, 0.0f,  0.0f, 0.0f, 1.0f,  // 2
		-0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 0.0f, 1.0f,  0.0f, 0.0f,  0.0f, 0.0f, 1.0f,  // 3

		// Back face (-Z)
		 0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 1.0f, 1.0f,  0.0f, 1.0f,  0.0f, 0.0f, -1.0f, // 4
		-0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 1.0f, 1.0f,  1.0f, 1.0f,  0.0f, 0.0f, -1.0f, // 5
		-0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 1.0f, 1.0f,  1.0f, 0.0f,  0.0f, 0.0f, -1.0f, // 6
		 0.5f,  0.5f, -0.5f,  0.5f, 0.5f, 0.5f, 1.0f,  0.0f, 0.0f,  0.0f, 0.0f, -1.0f, // 7

		// Top face (+Y)
		-0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 0.0f, 1.0f,  0.0f, 1.0f,  0.0f, 1.0f, 0.0f,  // 8
		 0.5f,  0.5f,  0.5f,  0.0f, 1.0f, 0.0f, 1.0f,  1.0f, 1.0f,  0.0f, 1.0f, 0.0f,  // 9
		 0.5f,  0.5f, -0.5f,  0.5f, 0.5f, 0.5f, 1.0f,  1.0f, 0.0f,  0.0f, 1.0f, 0.0f,  // 10
		-0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 1.0f, 1.0f,  0.0f, 0.0f,  0.0f, 1.0f, 0.0f,  // 11

		// Bottom face (-Y)
		-0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 1.0f, 1.0f,  0.0f, 1.0f,  0.0f, -1.0f, 0.0f, // 12
		 0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 1.0f, 1.0f,  1.0f, 1.0f,  0.0f, -1.0f, 0.0f, // 13
		 0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 0.0f, 1.0f,  1.0f, 0.0f,  0.0f, -1.0f, 0.0f, // 14
		-0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 0.0f, 1.0f,  0.0f, 0.0f,  0.0f, -1.0f, 0.0f, // 15

		// Right face (+X)
		 0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 0.0f, 1.0f,  0.0f, 1.0f,  1.0f, 0.0f, 0.0f,  // 16
		 0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 1.0f, 1.0f,  1.0f, 1.0f,  1.0f, 0.0f, 0.0f,  // 17
		 0.5f,  0.5f, -0.5f,  0.5f, 0.5f, 0.5f, 1.0f,  1.0f, 0.0f,  1.0f, 0.0f, 0.0f,  // 18
		 0.5f,  0.5f,  0.5f,  0.0f, 0.0f, 1.0f, 1.0f,  0.0f, 0.0f,  1.0f, 0.0f, 0.0f,  // 19

		// Left face (-X)
		-0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 1.0f, 1.0f,  0.0f, 1.0f,  -1.0f, 0.0f, 0.0f, // 20
		-0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 0.0f, 1.0f,  1.0f, 1.0f,  -1.0f, 0.0f, 0.0f, // 21
		-0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 0.0f, 1.0f,  1.0f, 0.0f,  -1.0f, 0.0f, 0.0f, // 22
		-0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 1.0f, 1.0f,  0.0f, 0.0f,  -1.0f, 0.0f, 0.0f  // 23
	};
	const std::vector<uint16_t> CubeIndices =
	{
		// Front face
		0, 1, 2,
		0, 2, 3,
		// Back face
		4, 5, 6,
		4, 6, 7,
		// Top face
		8, 9, 10,
		8, 10, 11,
		// Bottom face
		12, 13, 14,
		12, 14, 15,
		// Right face
		16, 17, 18,
		16, 18, 19,
		// Left face
		20, 21, 22,
		20, 22, 23
	};

	CubeMeshGeo->VertexByteStride	   = 12 * sizeof(float);
	CubeMeshGeo->VertexBufferByteSize = static_cast<UINT>(CubeVertices.size() * sizeof(float));
	CubeMeshGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(DxDevice3D.Get(), CommandList.Get(),
									(void*)CubeVertices.data(), CubeMeshGeo->VertexBufferByteSize, CubeMeshGeo->VertexBufferUploader);

	CubeMeshGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
	CubeMeshGeo->IndexBufferByteSize  = static_cast<UINT>(CubeIndices.size() * sizeof(uint16_t));
	CubeMeshGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(DxDevice3D.Get(), CommandList.Get(),
									(void*)CubeIndices.data(), CubeMeshGeo->IndexBufferByteSize, CubeMeshGeo->IndexBufferUploader);

	SubmeshGeometry CubeMeshPartition;
	CubeMeshPartition.BaseVertexLocation = 0;
	CubeMeshPartition.IndexCount = static_cast<UINT>(CubeIndices.size());
	CubeMeshGeo->DrawArgs["Base"] = CubeMeshPartition;

	MeshGeometries[CubeMeshGeo->Name] = move(CubeMeshGeo);

	// Surface geometry with tiled UVs
	auto SurfaceMeshGeo = std::make_unique<MeshGeometry>();
	SurfaceMeshGeo->Name = "Surface";
	const std::vector<float> SurfaceVertices =
	{
		// Position (x,y,z)   Color (r,g,b,a)        TexCoord (u,v)  Normal (x,y,z)
		-0.5f, -0.5f, 0.0f,  1.0f, 0.0f, 0.0f, 1.0f,  0.0f, 10.0f,  0.0f, 0.0f, 1.0f,  // bottom-left
		 0.5f, -0.5f, 0.0f,  0.0f, 1.0f, 0.0f, 1.0f,  10.0f, 10.0f,  0.0f, 0.0f, 1.0f,  // bottom-right
		-0.5f,  0.5f, 0.0f,  0.0f, 0.0f, 1.0f, 1.0f,  0.0f, 0.0f,  0.0f, 0.0f, 1.0f,  // top-left
		 0.5f,  0.5f, 0.0f,  1.0f, 1.0f, 1.0f, 1.0f,  10.0f, 0.0f,  0.0f, 0.0f, 1.0f   // top-right
	};
	const std::vector<uint16_t> SurfaceIndices =
	{
		0, 2, 1,
		1, 2, 3
	};

	SurfaceMeshGeo->VertexByteStride	   = 12 * sizeof(float);
	SurfaceMeshGeo->VertexBufferByteSize = static_cast<UINT>(SurfaceVertices.size() * sizeof(float));
	SurfaceMeshGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(DxDevice3D.Get(), CommandList.Get(),
									(void*)SurfaceVertices.data(), SurfaceMeshGeo->VertexBufferByteSize, SurfaceMeshGeo->VertexBufferUploader);

	SurfaceMeshGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
	SurfaceMeshGeo->IndexBufferByteSize  = static_cast<UINT>(SurfaceIndices.size() * sizeof(uint16_t));
	SurfaceMeshGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(DxDevice3D.Get(), CommandList.Get(),
									(void*)SurfaceIndices.data(), SurfaceMeshGeo->IndexBufferByteSize, SurfaceMeshGeo->IndexBufferUploader);

	SubmeshGeometry SurfaceMeshPartition;
	SurfaceMeshPartition.BaseVertexLocation = 0;
	SurfaceMeshPartition.IndexCount = static_cast<UINT>(SurfaceIndices.size());
	SurfaceMeshGeo->DrawArgs["Base"] = SurfaceMeshPartition;

	MeshGeometries[SurfaceMeshGeo->Name] = move(SurfaceMeshGeo);
}

void ShapesApp::BuildRenderItems()
{
	std::unique_ptr<RenderItem> CubeMesh = std::make_unique<RenderItem>();
	DirectX::XMStoreFloat4x4(&CubeMesh->World, DirectX::XMMatrixIdentity());
	CubeMesh->ObjConstBufferIndex = 0;
	CubeMesh->MeshGeometryRef = MeshGeometries["Cube"].get();
	CubeMesh->MaterialRef = GetMaterialForTexture("Texice");
	CubeMesh->IndexCount = CubeMesh->MeshGeometryRef->DrawArgs["Base"].IndexCount;
	CubeMesh->IndexStartLocation = CubeMesh->MeshGeometryRef->DrawArgs["Base"].StartIndexLocation;
	CubeMesh->VertexStartLocation = CubeMesh->MeshGeometryRef->DrawArgs["Base"].BaseVertexLocation;
	RenderItems.push_back( move(CubeMesh) );
	
	std::unique_ptr<RenderItem> SurfaceMesh = std::make_unique<RenderItem>();
	DirectX::XMStoreFloat4x4(&SurfaceMesh->World, DirectX::XMMatrixScaling(10,10,1)
		*DirectX::XMMatrixRotationX(DirectX::XM_PIDIV2)*DirectX::XMMatrixTranslation(0.0f,-2.0f,0.0f));
	SurfaceMesh->ObjConstBufferIndex = 1;
	SurfaceMesh->MeshGeometryRef = MeshGeometries["Surface"].get();
	SurfaceMesh->MaterialRef = GetMaterialForTexture("Texcheckboard");
	SurfaceMesh->IndexCount = SurfaceMesh->MeshGeometryRef->DrawArgs["Base"].IndexCount;
	SurfaceMesh->IndexStartLocation = SurfaceMesh->MeshGeometryRef->DrawArgs["Base"].StartIndexLocation;
	SurfaceMesh->VertexStartLocation = SurfaceMesh->MeshGeometryRef->DrawArgs["Base"].BaseVertexLocation;
	RenderItems.push_back( move(SurfaceMesh) );
}

void ShapesApp::BuildFrameResources()
{
	UINT RenderItemCount = static_cast<UINT>(RenderItems.size()); //Total Const Buffer Data we needed
	UINT TotalPass = 1;
	for (UINT i = 0; i < TotalFrameResources; i++)
		FrameResources.push_back(std::make_unique<FrameResource<PassConstBuffer,ObjConstBuffer,MaterialConstBuffer>>(DxDevice3D.Get(), TotalPass, RenderItemCount, RenderItemCount));
}

void ShapesApp::BuildDescriptorHeap()
{
	std::string TextureDirectory = "Assets\\Textures\\Diffuse";
	assert(std::filesystem::exists(TextureDirectory));
	UINT TexturesCount{0};  // @TODO Handle for Normal Textures
	for (auto Entry : std::filesystem::directory_iterator(TextureDirectory))
		if (Entry.is_regular_file() && Entry.path().extension() == ".dds")
			TexturesCount++;

	D3D12_DESCRIPTOR_HEAP_DESC HeapDesc;
	HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	HeapDesc.NodeMask = 0;
	HeapDesc.NumDescriptors = TexturesCount;
	HeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	ThrowIfFailed( DxDevice3D->CreateDescriptorHeap(&HeapDesc, IID_PPV_ARGS(&DescriptorHeap)) );
}

void ShapesApp::BuildPSO()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC OpaquePsoDesc;
	ZeroMemory(&OpaquePsoDesc, sizeof(OpaquePsoDesc));
	OpaquePsoDesc.pRootSignature = RootSignature.Get();

	D3D12_INPUT_LAYOUT_DESC InputLayoutDesc;
	InputLayoutDesc.NumElements = static_cast<UINT>(InputLayouts.size());
	InputLayoutDesc.pInputElementDescs = InputLayouts.data();
	OpaquePsoDesc.InputLayout = InputLayoutDesc;

	OpaquePsoDesc.VS =
	{	reinterpret_cast<BYTE*>(Shaders["Vertex"]->GetBufferPointer()),
		Shaders["Vertex"]->GetBufferSize()	};
	OpaquePsoDesc.PS =
	{	reinterpret_cast<BYTE*>(Shaders["Pixel"]->GetBufferPointer()),
		Shaders["Pixel"]->GetBufferSize()	};

	OpaquePsoDesc.RasterizerState          = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	OpaquePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID  ;

	OpaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	OpaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	OpaquePsoDesc.DSVFormat = DepthStencilFormat;
	OpaquePsoDesc.NumRenderTargets = 1;
	OpaquePsoDesc.RTVFormats[0] = BackBufferFormat;

	OpaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	OpaquePsoDesc.SampleMask = UINT_MAX;
	OpaquePsoDesc.SampleDesc.Count = 1;
	OpaquePsoDesc.SampleDesc.Quality = 0;
	ThrowIfFailed(DxDevice3D->CreateGraphicsPipelineState(&OpaquePsoDesc, IID_PPV_ARGS(&PSO) ));
}

