#include "ShapesApp.h"
#include "DirectXMath.h"

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

	ViewCamera->SetPosition(0, 0, -15);
	ViewCamera->LookAt(
		ViewCamera->GetPosition3f(),
		DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),    
		DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f));   
	ViewCamera->UpdateViewMatrix();


	ThrowIfFailed(CommandList->Reset(CommandAlloc.Get(), nullptr));
	BuildRootSignature();
	BuildShadersAndInputLayout();

	BuildGeometry();
	BuildRenderItem();

	BuildFrameResources();
	BuildDescriptorHeap();
	BuildDescriptors();

	BuildPSO();

	CommandList->Close();
	ID3D12CommandList* Commands[] = {CommandList.Get()};
	CommandQueue->ExecuteCommandLists(_countof(Commands), Commands) ;

	FlushCommandQueue();
	return true;
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
	int ObjConstBufferIndex = 0;


	DirectX::XMMATRIX XView = ViewCamera->GetView();
	DirectX::XMMATRIX XProj = ViewCamera->GetProj();
	DirectX::XMMATRIX XViewProj = DirectX::XMMatrixMultiply(XView, XProj);

	PassConstBuffer PassConstBufferData;
	// Transpose before sending to GPU! Which changes row majour to column majour
	DirectX::XMStoreFloat4x4(&PassConstBufferData.View, DirectX::XMMatrixTranspose(XView));
	DirectX::XMStoreFloat4x4(&PassConstBufferData.Proj, DirectX::XMMatrixTranspose(XProj));
	DirectX::XMStoreFloat4x4(&PassConstBufferData.ViewProj, DirectX::XMMatrixTranspose(XViewProj));
	PassConstBufferRes->CopyData(0, PassConstBufferData);

	for (auto& RenderItem : RenderItems)
	{
		DirectX::XMMATRIX XWorld = DirectX::XMLoadFloat4x4(&RenderItem->World);
		ObjConstBuffer  ObjConstBufferData;
		DirectX::XMStoreFloat4x4(&ObjConstBufferData.World, DirectX::XMMatrixTranspose(XWorld));
		ObjConstBufferRes->CopyData(ObjConstBufferIndex, ObjConstBufferData);
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

	CurrentFrameResource->FenceValue = ++GlobalFenceValue;
	CommandQueue->Signal(Fence.Get(), GlobalFenceValue);

}

void ShapesApp::DrawRenderItems(ID3D12GraphicsCommandList* CommandList, std::vector<std::unique_ptr<RenderItem>>& RenderItem)
{
	auto PassConstBufferRes = GetCurrentFrameResource()->PassConstBufferRes.get() ;
	auto ConstBufferGpuAddress = PassConstBufferRes->GetResourceGpuAddress();
	CommandList->SetGraphicsRootConstantBufferView(0, ConstBufferGpuAddress);

	auto ObjConstBufferRes = GetCurrentFrameResource()->ObjConstBufferRes.get() ;
	for (auto& RItem : RenderItem)
	{
		auto vbv = RItem->MeshGeometryData->VertexBufferView();
		CommandList->IASetVertexBuffers(0, 1, &vbv);
		auto ibv = RItem->MeshGeometryData->IndexBufferView();
		CommandList->IASetIndexBuffer(&ibv);
		CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		auto ObjConstBufferSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjConstBuffer));
		auto ObjBufferGpuAddress = ObjConstBufferRes->GetResourceGpuAddress() + ObjConstBufferSize * RItem->ObjConstBufferIndex;
		CommandList->SetGraphicsRootConstantBufferView(1, ObjBufferGpuAddress);

		CommandList->DrawIndexedInstanced(RItem->IndexCount, 1, RItem->IndexStartLocation, RItem->VertexStartLocation, 0);
	}
}

FrameResource<ShapesApp::PassConstBuffer,ShapesApp::ObjConstBuffer>* ShapesApp::GetCurrentFrameResource() const
{
	assert((CurrentFrameResourceIndex>=0 && CurrentFrameResourceIndex < FrameResources.size()) && "Trying to get FrameRes REF with an invalid Index");
	return FrameResources[CurrentFrameResourceIndex].get();
}

void ShapesApp::BuildRootSignature()
{
	CD3DX12_ROOT_PARAMETER RootParameter[2];
	RootParameter[0].InitAsConstantBufferView(0,0);
	RootParameter[1].InitAsConstantBufferView(1,0);

	CD3DX12_ROOT_SIGNATURE_DESC RootSignatureDesc(2, RootParameter,0,nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	Microsoft::WRL::ComPtr<ID3DBlob> SignatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> ErrorBlob;
	ThrowIfFailed( D3D12SerializeRootSignature(&RootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1 , SignatureBlob.GetAddressOf(), ErrorBlob.GetAddressOf()));

	ThrowIfFailed( DxDevice3D->CreateRootSignature(0, SignatureBlob->GetBufferPointer(), SignatureBlob->GetBufferSize(), IID_PPV_ARGS(RootSignature.GetAddressOf() )));
}

void ShapesApp::BuildShadersAndInputLayout()
{
	InputLayouts.push_back(
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 
			0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}	);

	InputLayouts.push_back(
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT,
			0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}	);

	Shaders["Vertex"] = d3dUtil::CompileShader(L"src\\Shaders\\ShapesApp.hlsl", nullptr, "VS", "vs_5_1");
	Shaders["Pixel"] = d3dUtil::CompileShader(L"src\\Shaders\\ShapesApp.hlsl", nullptr, "PS", "ps_5_1");
}

void ShapesApp::BuildGeometry()
{
	auto PlaneMeshGeo = std::make_unique<MeshGeometry>();
	PlaneMeshGeo->Name = "Surface";
	const std::vector<float> Vertices =
	{
		-0.5f, -0.5f, 0.0f,  1.0f, 0.0f, 0.0f, 1.0f,  // bottom-left
		 0.5f, -0.5f, 0.0f,  0.0f, 1.0f, 0.0f, 1.0f,  // bottom-right
		-0.5f,  0.5f, 0.0f,  0.0f, 0.0f, 1.0f, 1.0f,  // top-left
		 0.5f,  0.5f, 0.0f,  1.0f, 1.0f, 1.0f, 1.0f   // top-right
	};
	const std::vector<uint16_t> Indices =
	{
		0, 2, 1,   
		1, 2, 3    
	};

	PlaneMeshGeo->VertexByteStride	   = 7 * sizeof(float);
	PlaneMeshGeo->VertexBufferByteSize = static_cast<UINT>(Vertices.size() * sizeof(float));
	PlaneMeshGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(DxDevice3D.Get(), CommandList.Get(),
									(void*)Vertices.data(), PlaneMeshGeo->VertexBufferByteSize, PlaneMeshGeo->VertexBufferUploader);
	
	PlaneMeshGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
	PlaneMeshGeo->IndexBufferByteSize  = static_cast<UINT>(Indices.size() * sizeof(uint16_t));
	PlaneMeshGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(DxDevice3D.Get(), CommandList.Get(),
									(void*)Indices.data(), PlaneMeshGeo->IndexBufferByteSize, PlaneMeshGeo->IndexBufferUploader);

	SubmeshGeometry MeshPartition;
	MeshPartition.BaseVertexLocation = 0;
	MeshPartition.IndexCount = static_cast<UINT>(Indices.size());
	PlaneMeshGeo->DrawArgs["Base"] = MeshPartition;

	MeshGeometries[PlaneMeshGeo->Name] = move(PlaneMeshGeo);
}

void ShapesApp::BuildRenderItem()
{
	std::unique_ptr<RenderItem> PlaneMesh = std::make_unique<RenderItem>();
	DirectX::XMStoreFloat4x4(&PlaneMesh->World, DirectX::XMMatrixIdentity());
	PlaneMesh->ObjConstBufferIndex = 0;
	PlaneMesh->MeshGeometryData = MeshGeometries["Surface"].get();
	PlaneMesh->IndexCount = PlaneMesh->MeshGeometryData->DrawArgs["Base"].IndexCount;
	PlaneMesh->IndexStartLocation = PlaneMesh->MeshGeometryData->DrawArgs["Base"].StartIndexLocation;
	PlaneMesh->VertexStartLocation = PlaneMesh->MeshGeometryData->DrawArgs["Base"].BaseVertexLocation;
	RenderItems.push_back( move(PlaneMesh) );
	
	std::unique_ptr<RenderItem> SurfaceMesh = std::make_unique<RenderItem>();
	DirectX::XMStoreFloat4x4(&SurfaceMesh->World, DirectX::XMMatrixScaling(10,10,1)
		*DirectX::XMMatrixRotationX(DirectX::XM_PIDIV2)*DirectX::XMMatrixTranslation(0.0f,-2.0f,0.0f));
	SurfaceMesh->ObjConstBufferIndex = 1;
	SurfaceMesh->MeshGeometryData = MeshGeometries["Surface"].get();
	SurfaceMesh->IndexCount = SurfaceMesh->MeshGeometryData->DrawArgs["Base"].IndexCount;
	SurfaceMesh->IndexStartLocation = SurfaceMesh->MeshGeometryData->DrawArgs["Base"].StartIndexLocation;
	SurfaceMesh->VertexStartLocation = SurfaceMesh->MeshGeometryData->DrawArgs["Base"].BaseVertexLocation;
	RenderItems.push_back( move(SurfaceMesh) );
}

void ShapesApp::BuildFrameResources()
{
	UINT RenderItemCount = static_cast<UINT>(RenderItems.size()); //Total Const Buffer Data we needed
	UINT TotalPass = 1;
	for (UINT i = 0; i < TotalFrameResources; i++)
		FrameResources.push_back(std::make_unique<FrameResource<PassConstBuffer,ObjConstBuffer>>(DxDevice3D.Get(), TotalPass, RenderItemCount));
}

void ShapesApp::BuildDescriptorHeap()
{
	D3D12_DESCRIPTOR_HEAP_DESC HeapDesc;
	HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	HeapDesc.NodeMask = 0;
	HeapDesc.NumDescriptors = 2;
	HeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	ThrowIfFailed( DxDevice3D->CreateDescriptorHeap(&HeapDesc, IID_PPV_ARGS(&DescriptorHeap)) );
}

void ShapesApp::BuildDescriptors()
{
	/*auto ConstBufferSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ConstBuffer));

	D3D12_CONSTANT_BUFFER_VIEW_DESC CBVdesriptor;
	CBVdesriptor.BufferLocation = ConstBufferResource->GetResourceGpuAddress();
	CBVdesriptor.SizeInBytes = ConstBufferSize;

	auto HeapAddress = CD3DX12_CPU_DESCRIPTOR_HANDLE(DescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	DxDevice3D->CreateConstantBufferView(&CBVdesriptor, HeapAddress);*/
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

