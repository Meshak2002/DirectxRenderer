#include "ShapesApp.h"
#include "DirectXMath.h"
#include "iostream"
#include "Utility/ModelImporter.h"
#include "Utility/TextureConverter.h"
#include <filesystem>
#include "Utility/GeometryGenerator.h"
#include "Base/CubeMapRT.h"

const int gNumFrameResources = 3;


void ConvertToDDsTexturesOnStartup()
{
	std::cout << "\n===== AUTO-CONVERTING MODEL TEXTURES =====" << std::endl;

	// Set up conversion options
	TextureConverter::ConversionOptions options;
	options.Format = TextureConverter::CompressionFormat::BC7_UNORM;  // High quality compression
	options.Speed = TextureConverter::CompressionSpeed::QUICK;
	options.GenerateMipmaps = true;
	options.OverwriteExisting = false;
	options.FlipVertical = false;

	auto results = TextureConverter::ConvertDirectory(
		"Assets",				// Search in this folder
		"Assets\\DDS",			// Output Directory
		options,
		true                    // true = search subdirectories recursively (Assets\Models\SMG\*.jpg, etc.)
	);

	int successCount = 0;
	for (const auto& result : results)
	{
		if (result.Success)
			successCount++;
	}

	std::cout << "✓ Converted " << successCount << " / " << results.size() << " textures" << std::endl;
	std::cout << "===== CONVERSION COMPLETE =====" << std::endl;
}

ShapesApp::ShapesApp(HINSTANCE ScreenInstance) : DxRenderBase(ScreenInstance), SkyBox{ "Tex_sunsetcube1024" }
, bDebugShadowMap{ false }
{
	ViewCamera = std::make_unique<Camera>();
	for (int i = 0; i < 6; i++)
	{
		CubeMapCameras[i] = std::make_unique<Camera>();
	}
}

ShapesApp::~ShapesApp()
{
	SaveRenderItemsData();
	if (DxDevice3D)
		FlushCommandQueue();
}

void ShapesApp::ProcessKeyboardInput(float DeltaTime)
{
	float CamMovementSpeed = 6 * DeltaTime;
	float ObjDragSpeed = 2 * DeltaTime;
	float ObjRotateSpeed = 2 * DeltaTime;

	if (bLeftMouseDown && PickedRenderItem)
	{
		if (GetAsyncKeyState('W') & 0x8000)
			MovePickedObj(0, 0, ObjDragSpeed,false);
		if (GetAsyncKeyState('S') & 0x8000)
			MovePickedObj(0, 0, -ObjDragSpeed, false);
		if (GetAsyncKeyState('A') & 0x8000)
			MovePickedObj(-ObjDragSpeed, 0, 0, false);
		if (GetAsyncKeyState('D') & 0x8000)
			MovePickedObj(ObjDragSpeed, 0, 0, false);
		if (GetAsyncKeyState('Q') & 0x8000)
			MovePickedObj(0, ObjDragSpeed, 0, false);
		if (GetAsyncKeyState('E') & 0x8000)
			MovePickedObj(0, -ObjDragSpeed, 0, false);

		if (GetAsyncKeyState('R') & 0x8000)
			RotatePickedObj(ObjRotateSpeed, 0, 0);
		if (GetAsyncKeyState('F') & 0x8000)
			RotatePickedObj(-ObjRotateSpeed, 0, 0);
		if (GetAsyncKeyState('C') & 0x8000)
			RotatePickedObj(0, -ObjRotateSpeed, 0);
		if (GetAsyncKeyState('V') & 0x8000)
			RotatePickedObj(0, ObjRotateSpeed, 0);
		if (GetAsyncKeyState('Z') & 0x8000)
			RotatePickedObj(0, 0, -ObjRotateSpeed);
		if (GetAsyncKeyState('X') & 0x8000)
			RotatePickedObj(0, 0, ObjRotateSpeed);
	}
	else
	{
		if (GetAsyncKeyState('W') & 0x8000)
			ViewCamera->Walk(CamMovementSpeed);
		if (GetAsyncKeyState('S') & 0x8000)
			ViewCamera->Walk(-CamMovementSpeed);
		if (GetAsyncKeyState('A') & 0x8000)
			ViewCamera->Strafe(-CamMovementSpeed);
		if (GetAsyncKeyState('D') & 0x8000)
			ViewCamera->Strafe(CamMovementSpeed);
		if (GetAsyncKeyState('Q') & 0x8000)
			ViewCamera->Fly(-CamMovementSpeed);
		if (GetAsyncKeyState('E') & 0x8000)
			ViewCamera->Fly(CamMovementSpeed);
	}

	ViewCamera->UpdateViewMatrix();
}

void ShapesApp::OnMouseDown(WPARAM BtnState, int X, int Y)
{
	MouseLastPos.x = X;
	MouseLastPos.y = Y;

	SetCapture(MainWindowHandle);

	if ((BtnState & MK_LBUTTON) != 0)
	{
		bLeftMouseDown = true;
		Pick(X, Y);
	}
}

void ShapesApp::OnMouseUp(WPARAM BtnState, int X, int Y)
{
	bLeftMouseDown = false;
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

void ShapesApp::Pick(int X, int Y)
{
	PickedRenderItem = nullptr;
	//Convert to NDC
	float Xndc = (2.0f * X / ScreenWidth) - 1;
	float Yndc = 1 - (2.0f * Y / ScreenHeight);

	//Convert to View Space
	auto ProjMatrix = ViewCamera->GetProj4x4f();
	float XViewSpace = Xndc / ProjMatrix(0, 0);
	float YViewSpace = Yndc / ProjMatrix(1, 1);

	DirectX::XMVECTOR CamRayOrigin = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
	DirectX::XMVECTOR CamRayDir = DirectX::XMVectorSet(XViewSpace, YViewSpace, 1.0f, 0.0f);

	auto View = ViewCamera->GetView();
	DirectX::XMVECTOR ViewDet = DirectX::XMMatrixDeterminant(View);
	auto InvView = DirectX::XMMatrixInverse(&ViewDet, View);

	auto OpaqRenderItems = RenderLayerItems[(int)RenderLayer::Opaque];
	for (auto RenderItem : OpaqRenderItems)
	{
		auto World4x4 = RenderItem->World;
		auto World = DirectX::XMLoadFloat4x4(&World4x4);
		DirectX::XMVECTOR WorldDet = DirectX::XMMatrixDeterminant(World);
		auto InvWorld = DirectX::XMMatrixInverse(&WorldDet, World);

		DirectX::XMMATRIX ToLocal = DirectX::XMMatrixMultiply(InvView, InvWorld);
		auto LocalRayOrigin = DirectX::XMVector3TransformCoord(CamRayOrigin, ToLocal);
		auto LocalRayDir = DirectX::XMVector3TransformNormal(CamRayDir, ToLocal);
		LocalRayDir = DirectX::XMVector3Normalize(LocalRayDir);

		float Dist = 0.0f;
		if (RenderItem->Bounds.Intersects(LocalRayOrigin, LocalRayDir, Dist))
		{
			auto VertexData = (Vertex*)RenderItem->MeshGeometryRef->VertexBufferCPU->GetBufferPointer();
			auto IndexData = (GeometryGenerator::uint16*)RenderItem->MeshGeometryRef->IndexBufferCPU->GetBufferPointer();
			auto Indices = IndexData + RenderItem->IndexStartLocation;
			auto TotalIndices = RenderItem->IndexCount;
			UINT TrisCount = TotalIndices / 3;

			for (int i = 0; i < TrisCount; i++)
			{
				GeometryGenerator::uint16 Index0 = Indices[i * 3 + 0 + RenderItem->VertexStartLocation];
				GeometryGenerator::uint16 Index1 = Indices[i * 3 + 1 + RenderItem->VertexStartLocation];
				GeometryGenerator::uint16 Index2 = Indices[i * 3 + 2 + RenderItem->VertexStartLocation];

				auto vPos0 = VertexData[Index0].Position;
				auto vPos1 = VertexData[Index1].Position;
				auto vPos2 = VertexData[Index2].Position;
				DirectX::XMVECTOR Pos0 = DirectX::XMLoadFloat3(&vPos0);
				DirectX::XMVECTOR Pos1 = DirectX::XMLoadFloat3(&vPos1);
				DirectX::XMVECTOR Pos2 = DirectX::XMLoadFloat3(&vPos2);

				float Dist2 = 0.0f;
				if (DirectX::TriangleTests::Intersects(LocalRayOrigin, LocalRayDir, Pos0, Pos1, Pos2, Dist2))
				{
					PickedRenderItem = RenderItem;
					::OutputDebugStringA("Picked");
					return;
				}
			}
		}

	}

}

void ShapesApp::MovePickedObj(float X, float Y, float Z, bool bInLocalSpace)
{
	if (!PickedRenderItem)	return;

	auto RiWorldTransform = DirectX::XMLoadFloat4x4(&PickedRenderItem->World);
	DirectX::XMMATRIX Translation;
	if (bInLocalSpace)
		Translation = DirectX::XMMatrixMultiply(DirectX::XMMatrixTranslation(X, Y, Z), RiWorldTransform);
	else  //In World Space
		Translation = DirectX::XMMatrixMultiply(RiWorldTransform, DirectX::XMMatrixTranslation(X, Y, Z));

	DirectX::XMStoreFloat4x4(&PickedRenderItem->World, Translation);

}

void ShapesApp::RotatePickedObj(float Pitch, float Yaw, float Roll)
{
	if (!PickedRenderItem)	return;
	auto RiWorldTransform = DirectX::XMLoadFloat4x4(&PickedRenderItem->World);
	DirectX::XMMATRIX Rotation;
	DirectX::XMMATRIX Result;
	DirectX::XMVECTOR CachedPosition = RiWorldTransform.r[3];
	RiWorldTransform.r[3] = DirectX::XMVectorSet(0, 0, 0, 1);

	Rotation = DirectX::XMMatrixRotationRollPitchYaw(Pitch, Yaw, Roll);
	Result = DirectX::XMMatrixMultiply(RiWorldTransform, Rotation);
	Result.r[3] = CachedPosition;

	DirectX::XMStoreFloat4x4(&PickedRenderItem->World, Result);
}

void ShapesApp::OnResize()
{
	DxRenderBase::OnResize();
	ViewCamera->SetLens(0.25f * DirectX::XM_PI, AspectRatio(), 0.1f, 1000.0f);
}

void ShapesApp::CreateRtvDsvHeap()
{
	D3D12_DESCRIPTOR_HEAP_DESC RtvHeapDesc;
	RtvHeapDesc.NumDescriptors = SwapChainBuffferCount + 6; //Main buffers + CubeMap faces
	RtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	RtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	RtvHeapDesc.NodeMask = 0;
	ThrowIfFailed(DxDevice3D->CreateDescriptorHeap(&RtvHeapDesc, IID_PPV_ARGS(&RtvHeap)));
	D3D12_DESCRIPTOR_HEAP_DESC DsvHeapDesc;
	DsvHeapDesc.NumDescriptors = 3;  // Main depth buffer + Shadow + CubeMap
	DsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	DsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	DsvHeapDesc.NodeMask = 0;
	ThrowIfFailed(DxDevice3D->CreateDescriptorHeap(&DsvHeapDesc, IID_PPV_ARGS(&DsvHeap)));

}

bool ShapesApp::Initialize()
{
	if (!DxRenderBase::Initialize())
		return false;
	ConvertToDDsTexturesOnStartup();
	InitCamera();
	InitCubeMapCameras(0, 0, 0);

	UINT DepthTextureWidth = 2048;
	UINT DepthTextureHeight = 2048;
	ShadowMapObj = std::make_unique<ShadowMap>(DxDevice3D.Get(), DepthTextureWidth, DepthTextureHeight);
	UINT CubeMapWidth = 512;
	UINT CubeMapHeight = 512;
	CubeMapObj = std::make_unique<CubeMapRT>(DxDevice3D.Get(), CubeMapWidth, CubeMapHeight, BackBufferFormat, DepthStencilFormat);

	SceneSphereBound.Center = DirectX::XMFLOAT3(0.0f, -1.5f, 0.0f);
	SceneSphereBound.Radius = 10.0f;

	ThrowIfFailed(CommandList->Reset(CommandAlloc.Get(), nullptr));
	BuildRootSignature();
	BuildShadersAndInputLayout();
	BuildDescriptorHeap();

	BuildGeometryResource();
	BuildTextures();
	BuildDescriptors();
	BuildRenderItems();
	LoadRenderItemsData();

	BuildFrameResources();
	BuildPSO();

	CommandList->Close();
	ID3D12CommandList* Commands[] = { CommandList.Get() };
	CommandQueue->ExecuteCommandLists(_countof(Commands), Commands);

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

void ShapesApp::InitCubeMapCameras(float CenterX, float CenterY, float CenterZ)
{
	float PositionX{ CenterX };
	float PositionY{ CenterY };
	float PositionZ{ CenterZ };
	DirectX::XMFLOAT3 Position{ PositionX, PositionY, PositionZ };
	DirectX::XMFLOAT3 Target;
	DirectX::XMFLOAT3 Up;

	DirectX::XMFLOAT3 Targets[6] =
	{
		DirectX::XMFLOAT3(PositionX + 1, PositionY, PositionZ) ,
		DirectX::XMFLOAT3(PositionX - 1, PositionY, PositionZ) ,
		DirectX::XMFLOAT3(PositionX, PositionY + 1, PositionZ) ,
		DirectX::XMFLOAT3(PositionX, PositionY - 1, PositionZ) ,
		DirectX::XMFLOAT3(PositionX, PositionY, PositionZ + 1) ,
		DirectX::XMFLOAT3(PositionX, PositionY, PositionZ - 1)
	};
	DirectX::XMFLOAT3 Ups[6] =
	{
		 DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f),
		 DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f),
		 DirectX::XMFLOAT3(0.0f, 0.0f, -1.0f),
		 DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f),
		 DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f),
		 DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f)
	};

	for (int i = 0; i < 6; i++)
	{
		CubeMapCameras[i]->SetPosition(Position);
		CubeMapCameras[i]->LookAt(CubeMapCameras[i]->GetPosition3f(), Targets[i], Ups[i]);
		CubeMapCameras[i]->SetLens(0.5f * DirectX::XM_PI, 1.0f, 0.1f, 1000.0f);  // 90 degrees FOV for seamless cube mapping
		CubeMapCameras[i]->UpdateViewMatrix();
	}
}

void ShapesApp::BuildTextures()
{
	std::string TextureDirectory = "Assets\\DDS";
	assert(std::filesystem::exists(TextureDirectory));

	//Create Diffuse Textures
	for (auto Entry : std::filesystem::recursive_directory_iterator(TextureDirectory))
	{
		if (!Entry.is_regular_file() || Entry.path().extension() != ".dds")
			continue;
		auto NewTexture = std::make_unique<Texture>();
		auto OriginalFileName = Entry.path().stem().string();
		NewTexture->Name = "Tex_" + OriginalFileName;
		NewTexture->Filename = Entry.path().wstring();
		ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(DxDevice3D.Get(), CommandList.Get(),
			NewTexture->Filename.c_str(), NewTexture->Resource, NewTexture->UploadHeap));

		// Set debug names for easier tracking
		std::wstring ResourceName = L"Texture_" + std::wstring(NewTexture->Name.begin(), NewTexture->Name.end());
		NewTexture->Resource->SetName(ResourceName.c_str());
		if (NewTexture->UploadHeap)
		{
			std::wstring UploadName = ResourceName + L"_Upload";
			NewTexture->UploadHeap->SetName(UploadName.c_str());
		}

		NewTexture->bIsNormal = TextureConverter::IsGivenFileaNormalMap(OriginalFileName);
		NewTexture->bIsCubeTexture = TextureConverter::IsGivenFileaCubeMap(OriginalFileName);

		NewTexture->bIsDiffusedTexture = (!NewTexture->bIsNormal && !NewTexture->bIsCubeTexture);

		// Store pointer before moving for Texture2DStack
		Texture* TexturePtr = NewTexture.get();

		// Add texture with duplicate checking
		if (AddTexture(std::move(NewTexture)))
		{
			// Only add to Texture2DStack if it's not a cubemap and was successfully added
			if (!TexturePtr->bIsCubeTexture)
				Texture2DStack.push_back(TexturePtr);
		}
	}

}
void ShapesApp::BuildDescriptors()
{
	UINT DescriptorsSlot = 0;
	auto HeapStart = CD3DX12_CPU_DESCRIPTOR_HANDLE(SrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	for (auto TextureData : Texture2DStack)
	{
		TextureData->DescriptorHeapIndex = DescriptorsSlot;

		auto DescHeapHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(HeapStart, DescriptorsSlot++, CbvSrvUavDescriptorSize);
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = TextureData->Resource.Get()->GetDesc().Format;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = TextureData->Resource.Get()->GetDesc().MipLevels;
		srvDesc.Texture2D.ResourceMinLODClamp = 0;
		DxDevice3D->CreateShaderResourceView(TextureData->Resource.Get(), &srvDesc, DescHeapHandle);
	}
	//ShadowMap
	ShadowSkyMapHeapIndex = DescriptorsSlot;
	auto ShadowMapCpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(HeapStart, DescriptorsSlot++, CbvSrvUavDescriptorSize);
	auto DepthHeapCpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(GetDsvHeapCpuHandle(), 1, DsvDescriptorSize);
	ShadowMapObj->BuildDescriptors(ShadowMapCpuHandle, DepthHeapCpuHandle);

	//Skybox
	auto TextureData = GetTexture(SkyBox);
	TextureData->DescriptorHeapIndex = DescriptorsSlot;
	auto SKyboxDescHeapHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(HeapStart, DescriptorsSlot++, CbvSrvUavDescriptorSize);
	D3D12_SHADER_RESOURCE_VIEW_DESC SkyboxSrvDesc = {};
	SkyboxSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	SkyboxSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SkyboxSrvDesc.Format = TextureData->Resource.Get()->GetDesc().Format;
	SkyboxSrvDesc.TextureCube.MostDetailedMip = 0;
	SkyboxSrvDesc.TextureCube.MipLevels = TextureData->Resource.Get()->GetDesc().MipLevels;
	SkyboxSrvDesc.TextureCube.ResourceMinLODClamp = 0;
	DxDevice3D->CreateShaderResourceView(TextureData->Resource.Get(), &SkyboxSrvDesc, SKyboxDescHeapHandle);

	//ShadowMap2
	ShadowCubeMapHeapIndex = DescriptorsSlot;
	ShadowMapCpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(HeapStart, DescriptorsSlot++, CbvSrvUavDescriptorSize);
	ShadowMapObj->BuildDescriptors(ShadowMapCpuHandle, DepthHeapCpuHandle);

	//CubeMap
	SrvCubeMapHeapIndex = DescriptorsSlot;
	auto SrvCubeMapCpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(HeapStart, DescriptorsSlot++, CbvSrvUavDescriptorSize);
	auto DepthCubeMpaCpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(GetDsvHeapCpuHandle(), 2, DsvDescriptorSize);
	CD3DX12_CPU_DESCRIPTOR_HANDLE RtvCubeMapCpuHandles[6];
	for (int i = 0; i < 6; i++)
	{
		RtvCubeMapCpuHandles[i] = CD3DX12_CPU_DESCRIPTOR_HANDLE(GetRtvHeapCpuHandle(),
			SwapChainBuffferCount + i, RtvDescriptorSize);
	}
	CubeMapObj->BuildDescriptors(SrvCubeMapCpuHandle, RtvCubeMapCpuHandles, DepthCubeMpaCpuHandle);

	//NullSrv
	UINT NullSrvSlot = DescriptorsSlot;
	auto NullSrvCpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(HeapStart, DescriptorsSlot++, CbvSrvUavDescriptorSize);
	D3D12_SHADER_RESOURCE_VIEW_DESC NullSrvDesc = {};
	NullSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	NullSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	NullSrvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	NullSrvDesc.Texture2D.MostDetailedMip = 0;
	NullSrvDesc.Texture2D.MipLevels = 1;
	NullSrvDesc.Texture2D.ResourceMinLODClamp = 0;
	DxDevice3D->CreateShaderResourceView(nullptr, &NullSrvDesc, NullSrvCpuHandle);
	NullSrvGpuHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(SrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart(),
		NullSrvSlot, CbvSrvUavDescriptorSize);
}


Material* ShapesApp::BuildMaterial(std::string aMatName, std::string aDiffuseTexName, std::string aNormalTexName,
	float aDiffuseAlbedo, float aFresnalRO, float aShininess, float aUvTileValue)
{
	auto MaterialName = "Mat_" + aMatName;
	if (Materials.find(MaterialName) != Materials.end())
	{
		std::string ErrorMsg = "[Error] Material already Exists: " + MaterialName + "\n";
		::OutputDebugStringA(ErrorMsg.c_str());
		assert(false && "Building an existing material with same name.");
		return nullptr;
	}
	auto DiffTexture = GetTexture(aDiffuseTexName);
	if (!DiffTexture)
		return nullptr;

	if (!DiffTexture->bIsDiffusedTexture)
	{
		std::string ErrorMsg = "[Error] Given texture is not a Diffuse texture: Tex_" + aDiffuseTexName + "\n";
		::OutputDebugStringA(ErrorMsg.c_str());
		assert(false && "Given texture is not a Diffuse texture");
		return nullptr;
	}

	auto NormTexture = GetTexture(aNormalTexName);
	if (!NormTexture)
		return nullptr;

	if (!NormTexture->bIsNormal)
	{
		std::string ErrorMsg = "[Error] Given texture is not a Normal texture: Tex_" + aNormalTexName + "\n";
		::OutputDebugStringA(ErrorMsg.c_str());
		assert(false && "Given texture is not a Normal texture");
		return nullptr;
	}
	auto NewMaterial = std::make_unique<Material>();
	NewMaterial->DiffuseSrvHeapIndex = DiffTexture->DescriptorHeapIndex;
	NewMaterial->NormalSrvHeapIndex = NormTexture->DescriptorHeapIndex;
	NewMaterial->DiffuseAlbedo = DirectX::XMFLOAT4(aDiffuseAlbedo, aDiffuseAlbedo, aDiffuseAlbedo, 1);
	NewMaterial->FresnelR0 = DirectX::XMFLOAT3(aFresnalRO, aFresnalRO, aFresnalRO);  // Increase for more Reflection
	NewMaterial->Shininess = aShininess;
	NewMaterial->UvTileValue = aUvTileValue;
	Materials[MaterialName] = move(NewMaterial);
	return Materials[MaterialName].get();
}

Material* ShapesApp::GetMaterial(std::string aMaterialName)
{
	auto MaterialName = "Mat_" + aMaterialName;
	if (Materials.find(MaterialName) == Materials.end())
	{
		std::string ErrorMsg = "[Error] Material ain't Exists: " + MaterialName + "\n";
		::OutputDebugStringA(ErrorMsg.c_str());
		assert(false && "Material ain't Exists");
		return nullptr;
	}
	return Materials[MaterialName].get();
}

Texture* ShapesApp::GetTexture(std::string aTextureName)
{
	// Handle both "Tex_name" and "name" formats
	std::string TextureName = aTextureName;
	if (aTextureName.find("Tex_") != 0)
	{
		TextureName = "Tex_" + aTextureName;
	}

	if (Textures.find(TextureName) == Textures.end())
	{
		std::string ErrorMsg = "[Error] Texture doesn't exist: " + TextureName + "\n";
		::OutputDebugStringA(ErrorMsg.c_str());
		assert(false && "Texture doesn't exist");
		return nullptr;
	}
	return Textures[TextureName].get();
}

bool ShapesApp::AddTexture(std::unique_ptr<Texture> aTexture)
{
	if (!aTexture)
	{
		std::string ErrorMsg = "[Error] Attempted to add null Texture\n";
		::OutputDebugStringA(ErrorMsg.c_str());
		assert(false && "Attempted to add null Texture");
		return false;
	}

	// Check for duplicate texture name
	if (Textures.find(aTexture->Name) != Textures.end())
	{
		std::string ErrorMsg = "[Error] Texture with name '" + aTexture->Name + "' already exists. ";
		ErrorMsg += "Possible duplicate file: " + std::string(aTexture->Filename.begin(), aTexture->Filename.end()) + "\n";
		::OutputDebugStringA(ErrorMsg.c_str());
		assert(false && "Texture with duplicate name already exists");
		return false;
	}

	Textures[aTexture->Name] = std::move(aTexture);
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
	// Main shadow-casting light (key light)
	PassConstBufferData.Lights[0].Direction = { -0.57735f, -0.57735f, -0.57735f };
	PassConstBufferData.Lights[0].Strength = { 0.7f, 0.7f, 0.7f };

	// Subtle fill light from above (no shadows) - keep VERY dim
	PassConstBufferData.Lights[1].Direction = { 0, .5f, -.5f };
	PassConstBufferData.Lights[1].Strength = { 0.55f, 0.55f, 0.55f };

	// Subtle rim light (no shadows) - keep VERY dim
	PassConstBufferData.Lights[2].Direction = { 0.7071f, -0.0f, 0.7071f };
	PassConstBufferData.Lights[2].Strength = { 0.35f, 0.35f, 0.35f };



	DirectX::XMVECTOR LightDir = DirectX::XMLoadFloat3(&PassConstBufferData.Lights[0].Direction);
	DirectX::XMVECTOR LightPos = DirectX::XMVectorScale(LightDir, -2 * SceneSphereBound.Radius);
	DirectX::XMVECTOR FocusPt = DirectX::XMVectorZero();
	DirectX::XMVECTOR UpDir = DirectX::XMVectorSet(0, 1, 0, 0);
	XView = DirectX::XMMatrixLookAtLH(LightPos, FocusPt, UpDir);
	DirectX::XMFLOAT3 LightSpacePos;
	DirectX::XMStoreFloat3(&LightSpacePos, DirectX::XMVector3TransformCoord(FocusPt, XView));
	float Left = LightSpacePos.x - SceneSphereBound.Radius;
	float Right = LightSpacePos.x + SceneSphereBound.Radius;
	float Top = LightSpacePos.y + SceneSphereBound.Radius;
	float Bottom = LightSpacePos.y - SceneSphereBound.Radius;
	float Near = LightSpacePos.z - SceneSphereBound.Radius;
	float Far = LightSpacePos.z + SceneSphereBound.Radius;
	XProj = DirectX::XMMatrixOrthographicOffCenterLH(Left, Right, Bottom, Top, Near, Far);
	XViewProj = DirectX::XMMatrixMultiply(XView, XProj);
	DirectX::XMStoreFloat3(&EyePos, LightPos);
	DirectX::XMMATRIX T(		// Transform NDC space [-1,+1]^2 to texture space [0,1]^2
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f);
	DirectX::XMMATRIX ShadowTransform = XViewProj * T;
	PassConstBuffer ShadowPassBufferData = {};
	DirectX::XMStoreFloat4x4(&ShadowPassBufferData.View, DirectX::XMMatrixTranspose(XView));
	DirectX::XMStoreFloat4x4(&ShadowPassBufferData.Proj, DirectX::XMMatrixTranspose(XProj));
	DirectX::XMStoreFloat4x4(&ShadowPassBufferData.ViewProj, DirectX::XMMatrixTranspose(XViewProj));
	DirectX::XMStoreFloat4x4(&ShadowPassBufferData.ShadowTransform, DirectX::XMMatrixTranspose(ShadowTransform));
	ShadowPassBufferData.Eye = EyePos;


	DirectX::XMStoreFloat4x4(&PassConstBufferData.ShadowTransform, DirectX::XMMatrixTranspose(ShadowTransform));
	PassConstBufferRes->CopyData(0, PassConstBufferData);
	PassConstBufferRes->CopyData(1, ShadowPassBufferData);

	for (int i = 0; i < 6; i++)
	{
		XView = CubeMapCameras[i]->GetView();
		XProj = CubeMapCameras[i]->GetProj();
		XViewProj = DirectX::XMMatrixMultiply(XView, XProj);
		EyePos = CubeMapCameras[i]->GetPosition3f();

		PassConstBuffer CubeMapPassBufferData = {};
		//Transpose before sending to GPU! Which changes row majour to column majour
		DirectX::XMStoreFloat4x4(&CubeMapPassBufferData.View, DirectX::XMMatrixTranspose(XView));
		DirectX::XMStoreFloat4x4(&CubeMapPassBufferData.Proj, DirectX::XMMatrixTranspose(XProj));
		DirectX::XMStoreFloat4x4(&CubeMapPassBufferData.ViewProj, DirectX::XMMatrixTranspose(XViewProj));
		CubeMapPassBufferData.Eye = EyePos;
		// Copy lights and shadow transform from main pass
		std::memcpy(CubeMapPassBufferData.Lights, PassConstBufferData.Lights, sizeof(PassConstBufferData.Lights));
		CubeMapPassBufferData.ShadowTransform = PassConstBufferData.ShadowTransform;
		PassConstBufferRes->CopyData(2 + i, CubeMapPassBufferData);
	}

	for (auto& RenderItem : RenderItems)
	{
		DirectX::XMMATRIX XWorld = DirectX::XMLoadFloat4x4(&RenderItem->World);
		ObjConstBuffer  ObjConstBufferData;
		DirectX::XMStoreFloat4x4(&ObjConstBufferData.World, DirectX::XMMatrixTranspose(XWorld));
		ObjConstBufferRes->CopyData(ObjConstBufferIndex, ObjConstBufferData);

		assert(RenderItem->MaterialRef->DiffuseSrvHeapIndex >= 0 && RenderItem->MaterialRef->NormalSrvHeapIndex >= 0);
		MaterialConstBuffer MatConstBufferData
		{
			RenderItem->MaterialRef->DiffuseAlbedo,
			RenderItem->MaterialRef->FresnelR0,
			RenderItem->MaterialRef->Shininess,
			RenderItem->MaterialRef->UvTileValue,
			(UINT)RenderItem->MaterialRef->DiffuseSrvHeapIndex,
			(UINT)RenderItem->MaterialRef->NormalSrvHeapIndex
		};
		MatConstBufferRes->CopyData(ObjConstBufferIndex, MatConstBufferData);
		ObjConstBufferIndex++;
	}
}

void ShapesApp::DrawSceneToShadowMap()
{
	auto Barier = CD3DX12_RESOURCE_BARRIER::Transition(
		ShadowMapObj->GetResourcePtr(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	CommandList->ResourceBarrier(1, &Barier);

	auto ShadowViewport = ShadowMapObj->GetViewport();
	auto ShadowScissorRect = ShadowMapObj->GetRect();
	CommandList->RSSetViewports(1, &ShadowViewport);
	CommandList->RSSetScissorRects(1, &ShadowScissorRect);

	CommandList->ClearDepthStencilView(ShadowMapObj->GetDsvHeapCpuHandle(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
		1, 0, 0, nullptr);

	auto Dsv = ShadowMapObj->GetDsvHeapCpuHandle();
	CommandList->OMSetRenderTargets(0, nullptr, false, &Dsv);

	CommandList->SetPipelineState(PSO["ShadowOpaque"].Get());
	DrawRenderItems(CommandList.Get(), RenderLayerItems[(UINT)RenderLayer::Opaque]);

	auto Barier2 = CD3DX12_RESOURCE_BARRIER::Transition(
		ShadowMapObj->GetResourcePtr(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ);
	CommandList->ResourceBarrier(1, &Barier2);

}

void ShapesApp::DrawSceneToCubeMap()
{
	UINT PassSize = d3dUtil::CalcConstantBufferByteSize(sizeof(PassConstBuffer));
	auto CamPassConstBufferRes = GetCurrentFrameResource()->PassConstBufferRes.get();
	auto CubeMapViewport = CubeMapObj->GetViewport();
	auto CubeMapScissorRect = CubeMapObj->GetRect();
	CommandList->RSSetViewports(1, &CubeMapViewport);
	CommandList->RSSetScissorRects(1, &CubeMapScissorRect);

	CD3DX12_RESOURCE_BARRIER Barriers[2];
	Barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
		CubeMapObj->GetRtResourcePtr(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
	Barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(
		CubeMapObj->GetDsResourcePtr(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	CommandList->ResourceBarrier(2, Barriers);

	for (int i = 0; i < 6; i++)
	{
		CommandList->ClearRenderTargetView(CubeMapObj->GetRtvCpuHandle((size_t)i), DirectX::Colors::Black, 0, nullptr);
		CommandList->ClearDepthStencilView(CubeMapObj->GetDsvCpuHandle(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
			1, 0, 0, nullptr);

		auto Dsv = CubeMapObj->GetDsvCpuHandle();
		auto Rtv = CubeMapObj->GetRtvCpuHandle((size_t)i);
		CommandList->OMSetRenderTargets(1, &Rtv, true, &Dsv);

		auto CamPassBufferGpuAddress = CamPassConstBufferRes->GetResourceGpuAddress() + ((2 + i) * PassSize);
		CommandList->SetGraphicsRootConstantBufferView(0, CamPassBufferGpuAddress);

		DrawRenderItems(CommandList.Get(), RenderLayerItems[(UINT)RenderLayer::Opaque]);

		CommandList->SetPipelineState(PSO["Sky"].Get());
		DrawRenderItems(CommandList.Get(), RenderLayerItems[(UINT)RenderLayer::Skybox]);
		CommandList->SetPipelineState(PSO["Opaque"].Get());
	}
	CD3DX12_RESOURCE_BARRIER EndBarriers[2];
	EndBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
		CubeMapObj->GetRtResourcePtr(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
	EndBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(
		CubeMapObj->GetDsResourcePtr(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_COMMON);
	CommandList->ResourceBarrier(2, EndBarriers);

}

void ShapesApp::Draw(const GameTime& Gt)
{
	auto CurrentFrameResource = GetCurrentFrameResource();

	ThrowIfFailed(CurrentFrameResource->CommandAlloc->Reset());
	ThrowIfFailed(CommandList->Reset(CurrentFrameResource->CommandAlloc.Get(), PSO["Opaque"].Get()));

	UINT PassSize = d3dUtil::CalcConstantBufferByteSize(sizeof(PassConstBuffer));
	auto CamPassConstBufferRes = GetCurrentFrameResource()->PassConstBufferRes.get();

	ID3D12DescriptorHeap* DescHeap[] = { SrvDescriptorHeap.Get() };
	CommandList->SetDescriptorHeaps(_countof(DescHeap), DescHeap);
	CommandList->SetGraphicsRootSignature(RootSignature.Get());

	auto DescHeapGpuAddress = SrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
	CommandList->SetGraphicsRootDescriptorTable(2, DescHeapGpuAddress);  // TexTable

	CommandList->SetGraphicsRootDescriptorTable(4, NullSrvGpuHandle);

	auto CamPassBufferGpuAddress = CamPassConstBufferRes->GetResourceGpuAddress() + 1 * PassSize;
	CommandList->SetGraphicsRootConstantBufferView(0, CamPassBufferGpuAddress);
	DrawSceneToShadowMap();

	//--------------------------
	CommandList->SetPipelineState(PSO["Opaque"].Get());

	CD3DX12_GPU_DESCRIPTOR_HANDLE ShadowSkyGpuHandle(SrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	ShadowSkyGpuHandle.Offset(ShadowSkyMapHeapIndex, CbvSrvUavDescriptorSize);
	CommandList->SetGraphicsRootDescriptorTable(4, ShadowSkyGpuHandle);
	DrawSceneToCubeMap();

	auto Barier = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBufferResource(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	CommandList->ResourceBarrier(1, &Barier);

	CommandList->RSSetViewports(1, &Viewport);
	CommandList->RSSetScissorRects(1, &ScissorRect);

	CommandList->ClearRenderTargetView(CurrentBackBufferHeapDescHandle(), DirectX::Colors::Black, 0, nullptr);
	CommandList->ClearDepthStencilView(GetDsvHeapCpuHandle(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
		1, 0, 0, nullptr);

	auto Rtv = CurrentBackBufferHeapDescHandle();
	auto Dsv = GetDsvHeapCpuHandle();
	CommandList->OMSetRenderTargets(1, &Rtv, true, &Dsv);

	auto PassConstBufferRes = GetCurrentFrameResource()->PassConstBufferRes.get();
	auto PassBufferGpuAddress = PassConstBufferRes->GetResourceGpuAddress();
	CommandList->SetGraphicsRootConstantBufferView(0, PassBufferGpuAddress);

	CommandList->SetPipelineState(PSO["Opaque"].Get());
	DrawRenderItems(CommandList.Get(), RenderLayerItems[(UINT)RenderLayer::Opaque]);

	if (bDebugShadowMap)
	{
		CommandList->SetPipelineState(PSO["ShadowDebug"].Get());
		DrawRenderItems(CommandList.Get(), RenderLayerItems[(UINT)RenderLayer::ShadowDebug]);
	}
	CommandList->SetPipelineState(PSO["Sky"].Get());
	DrawRenderItems(CommandList.Get(), RenderLayerItems[(UINT)RenderLayer::Skybox]);

	//Render the CubeMap Reflection
	CommandList->SetPipelineState(PSO["Opaque"].Get());
	CD3DX12_GPU_DESCRIPTOR_HANDLE CubeMap(SrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	CubeMap.Offset(ShadowCubeMapHeapIndex, CbvSrvUavDescriptorSize);
	CommandList->SetGraphicsRootDescriptorTable(4, CubeMap);
	DrawRenderItems(CommandList.Get(), RenderLayerItems[(UINT)RenderLayer::Reflection]);

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

void ShapesApp::DrawRenderItems(ID3D12GraphicsCommandList* CommandList, std::vector<RenderItem*>& RenderItem)
{
	auto ObjConstBufferRes = GetCurrentFrameResource()->ObjConstBufferRes.get();
	auto MatConstBufferRes = GetCurrentFrameResource()->MatConstBufferRes.get();

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

		auto MatConstBufferSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstBuffer));
		auto MatBufferGpuAddress = MatConstBufferRes->GetResourceGpuAddress() + MatConstBufferSize * RItem->ObjConstBufferIndex;
		CommandList->SetGraphicsRootConstantBufferView(3, MatBufferGpuAddress);

		CommandList->DrawIndexedInstanced(RItem->IndexCount, 1, RItem->IndexStartLocation, RItem->VertexStartLocation, 0);
	}
}

FrameResource<ShapesApp::PassConstBuffer, ShapesApp::ObjConstBuffer, ShapesApp::MaterialConstBuffer>* ShapesApp::GetCurrentFrameResource() const
{
	assert((CurrentFrameResourceIndex >= 0 && CurrentFrameResourceIndex < FrameResources.size()) && "Trying to get FrameRes REF with an invalid Index");
	return FrameResources[CurrentFrameResourceIndex].get();
}

void ShapesApp::BuildRootSignature()
{
	const size_t TotalRootParameters = 5;
	CD3DX12_ROOT_PARAMETER RootParameter[TotalRootParameters];
	RootParameter[0].InitAsConstantBufferView(0, 0);
	RootParameter[1].InitAsConstantBufferView(1, 0);

	CD3DX12_DESCRIPTOR_RANGE TextureDescTable;
	TextureDescTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, MAX_TEXTURES, 0, 0);		//Textures
	RootParameter[2].InitAsDescriptorTable(1, &TextureDescTable, D3D12_SHADER_VISIBILITY_PIXEL);
	RootParameter[3].InitAsConstantBufferView(2, 0, D3D12_SHADER_VISIBILITY_PIXEL);	//Material

	CD3DX12_DESCRIPTOR_RANGE ShadowSkyDescTable;
	ShadowSkyDescTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 1);  // 2 SRVs at t0-t1 in space1
	RootParameter[4].InitAsDescriptorTable(1, &ShadowSkyDescTable, D3D12_SHADER_VISIBILITY_PIXEL);


	auto Samplers = d3dUtil::GetStaticSamplers();
	CD3DX12_ROOT_SIGNATURE_DESC RootSignatureDesc(TotalRootParameters, RootParameter, static_cast<UINT>(Samplers.size()), Samplers.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	Microsoft::WRL::ComPtr<ID3DBlob> SignatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> ErrorBlob;
	HRESULT HR = (D3D12SerializeRootSignature(&RootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1
		, SignatureBlob.GetAddressOf(), ErrorBlob.GetAddressOf()));
	if (ErrorBlob != nullptr)
	{
		::OutputDebugStringA((char*)ErrorBlob->GetBufferPointer());
	}
	ThrowIfFailed(HR);

	ThrowIfFailed(DxDevice3D->CreateRootSignature(0, SignatureBlob->GetBufferPointer(),
		SignatureBlob->GetBufferSize(), IID_PPV_ARGS(RootSignature.GetAddressOf())));
}

void ShapesApp::BuildShadersAndInputLayout()
{
	InputLayouts.push_back(
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,
			0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 });

	InputLayouts.push_back(
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,
			0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 });

	InputLayouts.push_back(
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT,
			0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 });

	InputLayouts.push_back(
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT,
			0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 });

	Shaders["Vertex"] = d3dUtil::CompileShader(L"src\\Shaders\\ShapesApp.hlsl", nullptr, "VS", "vs_5_1");
	Shaders["Pixel"] = d3dUtil::CompileShader(L"src\\Shaders\\ShapesApp.hlsl", nullptr, "PS", "ps_5_1");

	Shaders["SkyVertex"] = d3dUtil::CompileShader(L"src\\Shaders\\Skybox.hlsl", nullptr, "VS", "vs_5_1");
	Shaders["SkyPixel"] = d3dUtil::CompileShader(L"src\\Shaders\\Skybox.hlsl", nullptr, "PS", "ps_5_1");

	Shaders["ShadowVS"] = d3dUtil::CompileShader(L"src\\Shaders\\ShadowMap.hlsl", nullptr, "VS", "vs_5_1");
	Shaders["ShadowPS"] = d3dUtil::CompileShader(L"src\\Shaders\\ShadowMap.hlsl", nullptr, "PS", "ps_5_1");

	Shaders["ShadowDebugVS"] = d3dUtil::CompileShader(L"src\\Shaders\\ShadowMapDebug.hlsl", nullptr, "VS", "vs_5_1");
	Shaders["ShadowDebugPS"] = d3dUtil::CompileShader(L"src\\Shaders\\ShadowMapDebug.hlsl", nullptr, "PS", "ps_5_1");
}

void ShapesApp::BuildGeometryResource()
{
	ModelImporter::ModelData smgModelData;
	bool smgLoaded = ModelImporter::LoadModel(
		"Assets\\Models\\SMG\\M24_R_Low_Poly_Version_fbx.fbx",
		smgModelData, true, false, false);

	if (smgLoaded)
	{
		auto smgMeshGeo = ModelImporter::CreateMeshGeometry(smgModelData, DxDevice3D.Get(), CommandList.Get(), "SMG");
		MeshGeometries[smgMeshGeo->Name] = std::move(smgMeshGeo);
	}
	else
	{
		std::cerr << "Failed to load SMG model!" << std::endl;
	}

	GeometryGenerator GeoGen;
	//SkyBox
	GeometryGenerator::MeshData SphereGeo = GeoGen.CreateSphere(1.0f, 24, 24);
	auto SkyboxSphere = std::make_unique<MeshGeometry>();
	SkyboxSphere->Name = "Skybox";
	SkyboxSphere->VertexByteStride = sizeof(Vertex);
	SkyboxSphere->VertexBufferByteSize = static_cast<UINT>(SphereGeo.Vertices.size() * sizeof(Vertex));

	ThrowIfFailed(D3DCreateBlob(SkyboxSphere->VertexBufferByteSize, &SkyboxSphere->VertexBufferCPU));
	memcpy(SkyboxSphere->VertexBufferCPU->GetBufferPointer(), SphereGeo.Vertices.data(), SkyboxSphere->VertexBufferByteSize);

	SkyboxSphere->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(DxDevice3D.Get(), CommandList.Get(),
		SphereGeo.Vertices.data(), SkyboxSphere->VertexBufferByteSize, SkyboxSphere->VertexBufferUploader);
	SkyboxSphere->VertexBufferGPU->SetName(L"Skybox_VB");
	if (SkyboxSphere->VertexBufferUploader)
		SkyboxSphere->VertexBufferUploader->SetName(L"Skybox_VB_Upload");

	SkyboxSphere->IndexFormat = DXGI_FORMAT_R16_UINT;
	SkyboxSphere->IndexBufferByteSize = static_cast<UINT>(SphereGeo.GetIndices16().size() * sizeof(uint16_t));

	ThrowIfFailed(D3DCreateBlob(SkyboxSphere->IndexBufferByteSize, &SkyboxSphere->IndexBufferCPU));
	memcpy(SkyboxSphere->IndexBufferCPU->GetBufferPointer(), SphereGeo.GetIndices16().data(), SkyboxSphere->IndexBufferByteSize);

	SkyboxSphere->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(DxDevice3D.Get(), CommandList.Get(),
		SphereGeo.GetIndices16().data(), SkyboxSphere->IndexBufferByteSize, SkyboxSphere->IndexBufferUploader);
	SkyboxSphere->IndexBufferGPU->SetName(L"Skybox_IB");
	if (SkyboxSphere->IndexBufferUploader)
		SkyboxSphere->IndexBufferUploader->SetName(L"Skybox_IB_Upload");

	SubmeshGeometry SphereDrawArg;
	SphereDrawArg.BaseVertexLocation = 0;
	SphereDrawArg.StartIndexLocation = 0;
	SphereDrawArg.IndexCount = static_cast<UINT>(SphereGeo.GetIndices16().size());
	SphereDrawArg.Bounds = GeometryGenerator::CalculateBounds(SphereGeo.Vertices);
	SkyboxSphere->DrawArgs["Base"] = SphereDrawArg;
	MeshGeometries[SkyboxSphere->Name] = move(SkyboxSphere);


	// Cube
	GeometryGenerator::MeshData CubeGeo = GeoGen.CreateBox(1.0f, 1.0f, 1.0f, 0);
	auto CubeMeshGeo = std::make_unique<MeshGeometry>();
	CubeMeshGeo->Name = "Cube";
	CubeMeshGeo->VertexByteStride = sizeof(Vertex);
	CubeMeshGeo->VertexBufferByteSize = static_cast<UINT>(CubeGeo.Vertices.size() * sizeof(Vertex));

	ThrowIfFailed(D3DCreateBlob(CubeMeshGeo->VertexBufferByteSize, &CubeMeshGeo->VertexBufferCPU));
	memcpy(CubeMeshGeo->VertexBufferCPU->GetBufferPointer(), CubeGeo.Vertices.data(), CubeMeshGeo->VertexBufferByteSize);

	CubeMeshGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(DxDevice3D.Get(), CommandList.Get(),
		CubeGeo.Vertices.data(), CubeMeshGeo->VertexBufferByteSize, CubeMeshGeo->VertexBufferUploader);

	CubeMeshGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
	CubeMeshGeo->IndexBufferByteSize = static_cast<UINT>(CubeGeo.GetIndices16().size() * sizeof(uint16_t));

	ThrowIfFailed(D3DCreateBlob(CubeMeshGeo->IndexBufferByteSize, &CubeMeshGeo->IndexBufferCPU));
	memcpy(CubeMeshGeo->IndexBufferCPU->GetBufferPointer(), CubeGeo.GetIndices16().data(), CubeMeshGeo->IndexBufferByteSize);

	CubeMeshGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(DxDevice3D.Get(), CommandList.Get(),
		CubeGeo.GetIndices16().data(), CubeMeshGeo->IndexBufferByteSize, CubeMeshGeo->IndexBufferUploader);

	SubmeshGeometry CubeMeshPartition;
	CubeMeshPartition.BaseVertexLocation = 0;
	CubeMeshPartition.StartIndexLocation = 0;
	CubeMeshPartition.IndexCount = static_cast<UINT>(CubeGeo.GetIndices16().size());
	CubeMeshPartition.Bounds = GeometryGenerator::CalculateBounds(CubeGeo.Vertices);
	CubeMeshGeo->DrawArgs["Base"] = CubeMeshPartition;
	MeshGeometries[CubeMeshGeo->Name] = move(CubeMeshGeo);

	// Surface geometry (1x1 quad, will be scaled and tiled in BuildRenderItems)
	GeometryGenerator::MeshData SurfaceGeo = GeoGen.CreateQuad(-0.5f, -0.5f, 1.0f, 1.0f, 0.0f);
	auto SurfaceMeshGeo = std::make_unique<MeshGeometry>();
	SurfaceMeshGeo->Name = "Surface";
	SurfaceMeshGeo->VertexByteStride = sizeof(Vertex);
	SurfaceMeshGeo->VertexBufferByteSize = static_cast<UINT>(SurfaceGeo.Vertices.size() * sizeof(Vertex));

	ThrowIfFailed(D3DCreateBlob(SurfaceMeshGeo->VertexBufferByteSize, &SurfaceMeshGeo->VertexBufferCPU));
	memcpy(SurfaceMeshGeo->VertexBufferCPU->GetBufferPointer(), SurfaceGeo.Vertices.data(), SurfaceMeshGeo->VertexBufferByteSize);

	SurfaceMeshGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(DxDevice3D.Get(), CommandList.Get(),
		SurfaceGeo.Vertices.data(), SurfaceMeshGeo->VertexBufferByteSize, SurfaceMeshGeo->VertexBufferUploader);

	SurfaceMeshGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
	SurfaceMeshGeo->IndexBufferByteSize = static_cast<UINT>(SurfaceGeo.GetIndices16().size() * sizeof(uint16_t));

	ThrowIfFailed(D3DCreateBlob(SurfaceMeshGeo->IndexBufferByteSize, &SurfaceMeshGeo->IndexBufferCPU));
	memcpy(SurfaceMeshGeo->IndexBufferCPU->GetBufferPointer(), SurfaceGeo.GetIndices16().data(), SurfaceMeshGeo->IndexBufferByteSize);

	SurfaceMeshGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(DxDevice3D.Get(), CommandList.Get(),
		SurfaceGeo.GetIndices16().data(), SurfaceMeshGeo->IndexBufferByteSize, SurfaceMeshGeo->IndexBufferUploader);

	SubmeshGeometry SurfaceMeshPartition;
	SurfaceMeshPartition.BaseVertexLocation = 0;
	SurfaceMeshPartition.StartIndexLocation = 0;
	SurfaceMeshPartition.IndexCount = static_cast<UINT>(SurfaceGeo.GetIndices16().size());
	SurfaceMeshPartition.Bounds = GeometryGenerator::CalculateBounds(SurfaceGeo.Vertices);
	SurfaceMeshGeo->DrawArgs["Base"] = SurfaceMeshPartition;
	MeshGeometries[SurfaceMeshGeo->Name] = move(SurfaceMeshGeo);

	//ShadowDebug Plane Layer
	GeometryGenerator::MeshData QuadGeo = GeoGen.CreateQuad(0, 0, 1, 1, 0);
	auto DebugQuad = std::make_unique<MeshGeometry>();
	DebugQuad->Name = "DebugQuad";
	DebugQuad->VertexByteStride = sizeof(Vertex);
	DebugQuad->VertexBufferByteSize = static_cast<UINT>(QuadGeo.Vertices.size() * sizeof(Vertex));

	ThrowIfFailed(D3DCreateBlob(DebugQuad->VertexBufferByteSize, &DebugQuad->VertexBufferCPU));
	memcpy(DebugQuad->VertexBufferCPU->GetBufferPointer(), QuadGeo.Vertices.data(), DebugQuad->VertexBufferByteSize);

	DebugQuad->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(DxDevice3D.Get(), CommandList.Get(),
		QuadGeo.Vertices.data(), DebugQuad->VertexBufferByteSize, DebugQuad->VertexBufferUploader);

	DebugQuad->IndexFormat = DXGI_FORMAT_R16_UINT;
	DebugQuad->IndexBufferByteSize = static_cast<UINT>(QuadGeo.GetIndices16().size() * sizeof(uint16_t));

	ThrowIfFailed(D3DCreateBlob(DebugQuad->IndexBufferByteSize, &DebugQuad->IndexBufferCPU));
	memcpy(DebugQuad->IndexBufferCPU->GetBufferPointer(), QuadGeo.GetIndices16().data(), DebugQuad->IndexBufferByteSize);

	DebugQuad->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(DxDevice3D.Get(), CommandList.Get(),
		QuadGeo.GetIndices16().data(), DebugQuad->IndexBufferByteSize, DebugQuad->IndexBufferUploader);

	SubmeshGeometry QuadQuad;
	QuadQuad.BaseVertexLocation = 0;
	QuadQuad.StartIndexLocation = 0;
	QuadQuad.IndexCount = static_cast<UINT>(QuadGeo.GetIndices16().size());
	QuadQuad.Bounds = GeometryGenerator::CalculateBounds(QuadGeo.Vertices);
	DebugQuad->DrawArgs["Base"] = QuadQuad;
	MeshGeometries[DebugQuad->Name] = move(DebugQuad);
}

void ShapesApp::BuildRenderItems()
{
	UINT objIndex = 0;

	if (MeshGeometries.find("SMG") != MeshGeometries.end())
	{
		auto smgMeshGeo = MeshGeometries["SMG"].get();
		for (const auto& [submeshName, submesh] : smgMeshGeo->DrawArgs)
		{
			std::unique_ptr<RenderItem> smgRenderItem = std::make_unique<RenderItem>();
			smgRenderItem->Name = std::string("SMG_") + submeshName;
			DirectX::XMStoreFloat4x4(&smgRenderItem->World,
				DirectX::XMMatrixScaling(0.1f, 0.1f, 0.1f) *
				DirectX::XMMatrixRotationY(DirectX::XM_PI / 2) *
				DirectX::XMMatrixRotationZ(DirectX::XM_PI / 2) *
				DirectX::XMMatrixTranslation(0.0f, -1.5f, 0.0f)
			);
			smgRenderItem->ObjConstBufferIndex = objIndex++;
			smgRenderItem->MeshGeometryRef = smgMeshGeo;
			smgRenderItem->MaterialRef = BuildMaterial("SMG", "M24R_C", "M24R_N",
				1, .7f, .7f, 1);
			smgRenderItem->IndexCount = submesh.IndexCount;
			smgRenderItem->IndexStartLocation = submesh.StartIndexLocation;
			smgRenderItem->VertexStartLocation = submesh.BaseVertexLocation;
			smgRenderItem->Bounds = submesh.Bounds;
			AddRenderItem(std::move(smgRenderItem), RenderLayer::Opaque);
		}
	}

	if (MeshGeometries.find("DebugQuad") != MeshGeometries.end())
	{
		auto DebugQuadMeshGeo = MeshGeometries["DebugQuad"].get();
		for (const auto& [submeshName, submesh] : DebugQuadMeshGeo->DrawArgs)
		{
			std::unique_ptr<RenderItem> DebugQuadRI = std::make_unique<RenderItem>();
			DebugQuadRI->Name = std::string("DebugQuad_") + submeshName;

			DebugQuadRI->World = MathHelper::Identity4x4();
			DebugQuadRI->ObjConstBufferIndex = objIndex++;
			DebugQuadRI->MeshGeometryRef = DebugQuadMeshGeo;
			DebugQuadRI->MaterialRef = BuildMaterial("DepthDebugQuad", "tile", "tile_nmap",
				1, .1f, .1f, 1);
			DebugQuadRI->IndexCount = submesh.IndexCount;
			DebugQuadRI->IndexStartLocation = submesh.StartIndexLocation;
			DebugQuadRI->VertexStartLocation = submesh.BaseVertexLocation;
			DebugQuadRI->Bounds = submesh.Bounds;
			AddRenderItem(std::move(DebugQuadRI), RenderLayer::ShadowDebug);
		}
	}

	std::unique_ptr<RenderItem> CubeMesh = std::make_unique<RenderItem>();
	CubeMesh->Name = std::string("CubeMesh_") + "Base";
	DirectX::XMStoreFloat4x4(&CubeMesh->World, DirectX::XMMatrixTranslation(3.0f, -1.5f, 2.0f));  // Move cube away from z=0
	CubeMesh->ObjConstBufferIndex = objIndex++;
	CubeMesh->MeshGeometryRef = MeshGeometries["Cube"].get();
	CubeMesh->MaterialRef = BuildMaterial("CubeMesh", "bricks2", "bricks2_nmap",
		.5f, .2f, .2f, 1);
	CubeMesh->IndexCount = CubeMesh->MeshGeometryRef->DrawArgs["Base"].IndexCount;
	CubeMesh->IndexStartLocation = CubeMesh->MeshGeometryRef->DrawArgs["Base"].StartIndexLocation;
	CubeMesh->VertexStartLocation = CubeMesh->MeshGeometryRef->DrawArgs["Base"].BaseVertexLocation;
	CubeMesh->Bounds = CubeMesh->MeshGeometryRef->DrawArgs["Base"].Bounds;
	AddRenderItem(std::move(CubeMesh), RenderLayer::Opaque);

	std::unique_ptr<RenderItem> SurfaceMesh = std::make_unique<RenderItem>();
	SurfaceMesh->Name = std::string("SurfaceMesh_") + "Base";
	DirectX::XMStoreFloat4x4(&SurfaceMesh->World, DirectX::XMMatrixScaling(10, 10, 1)
		* DirectX::XMMatrixRotationX(DirectX::XM_PIDIV2) * DirectX::XMMatrixTranslation(0.0f, -2.0f, 0.0f));
	SurfaceMesh->ObjConstBufferIndex = objIndex++;
	SurfaceMesh->MeshGeometryRef = MeshGeometries["Surface"].get();
	SurfaceMesh->MaterialRef = BuildMaterial("SurfaceMesh", "tile", "tile_nmap",
		1.0f, .6f, .5f, 10);
	SurfaceMesh->IndexCount = SurfaceMesh->MeshGeometryRef->DrawArgs["Base"].IndexCount;
	SurfaceMesh->IndexStartLocation = SurfaceMesh->MeshGeometryRef->DrawArgs["Base"].StartIndexLocation;
	SurfaceMesh->VertexStartLocation = SurfaceMesh->MeshGeometryRef->DrawArgs["Base"].BaseVertexLocation;
	SurfaceMesh->Bounds = SurfaceMesh->MeshGeometryRef->DrawArgs["Base"].Bounds;
	AddRenderItem(std::move(SurfaceMesh), RenderLayer::Opaque);

	std::unique_ptr<RenderItem> SkyBoxMesh = std::make_unique<RenderItem>();
	SkyBoxMesh->Name = std::string("SkyBoxMesh_") + "Base";
	DirectX::XMStoreFloat4x4(&SkyBoxMesh->World, DirectX::XMMatrixScaling(500, 500, 500));
	SkyBoxMesh->ObjConstBufferIndex = objIndex++;
	SkyBoxMesh->MeshGeometryRef = MeshGeometries["Skybox"].get();
	SkyBoxMesh->MaterialRef = BuildMaterial("Reflection", "white1x1", "default_nmap",
		.05f, .95f, .95f, 1);
	SkyBoxMesh->IndexCount = SkyBoxMesh->MeshGeometryRef->DrawArgs["Base"].IndexCount;
	SkyBoxMesh->IndexStartLocation = SkyBoxMesh->MeshGeometryRef->DrawArgs["Base"].StartIndexLocation;
	SkyBoxMesh->VertexStartLocation = SkyBoxMesh->MeshGeometryRef->DrawArgs["Base"].BaseVertexLocation;
	SkyBoxMesh->Bounds = SkyBoxMesh->MeshGeometryRef->DrawArgs["Base"].Bounds;
	AddRenderItem(std::move(SkyBoxMesh), RenderLayer::Skybox);

	std::unique_ptr<RenderItem> ReflectionSphere = std::make_unique<RenderItem>();
	ReflectionSphere->Name = std::string("ReflectionSphere_") + "Base";
	DirectX::XMStoreFloat4x4(&ReflectionSphere->World, DirectX::XMMatrixScaling(.5f, .5f, .5f));
	ReflectionSphere->ObjConstBufferIndex = objIndex++;
	ReflectionSphere->MeshGeometryRef = MeshGeometries["Skybox"].get();
	ReflectionSphere->MaterialRef = GetMaterial("Reflection");
	ReflectionSphere->IndexCount = ReflectionSphere->MeshGeometryRef->DrawArgs["Base"].IndexCount;
	ReflectionSphere->IndexStartLocation = ReflectionSphere->MeshGeometryRef->DrawArgs["Base"].StartIndexLocation;
	ReflectionSphere->VertexStartLocation = ReflectionSphere->MeshGeometryRef->DrawArgs["Base"].BaseVertexLocation;
	ReflectionSphere->Bounds = ReflectionSphere->MeshGeometryRef->DrawArgs["Base"].Bounds;
	AddRenderItem(std::move(ReflectionSphere), RenderLayer::Reflection);
}

void ShapesApp::BuildFrameResources()
{
	UINT RenderItemCount = static_cast<UINT>(RenderItems.size()); //Total Const Buffer Data we needed
	UINT TotalPass = 8; // MainPass(1) + ShadowPass(1) + CubeMapPass(6)
	for (UINT i = 0; i < TotalFrameResources; i++)
		FrameResources.push_back(std::make_unique<FrameResource<PassConstBuffer, ObjConstBuffer, MaterialConstBuffer>>(DxDevice3D.Get(), TotalPass, RenderItemCount, RenderItemCount));
}

void ShapesApp::BuildDescriptorHeap()
{
	// Allocate descriptor heap with max size to match root signature
	// Unused slots cost minimal memory (~8-32 bytes per descriptor)
	D3D12_DESCRIPTOR_HEAP_DESC HeapDesc;
	HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	HeapDesc.NodeMask = 0;
	HeapDesc.NumDescriptors = MAX_TEXTURES;
	HeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	ThrowIfFailed(DxDevice3D->CreateDescriptorHeap(&HeapDesc, IID_PPV_ARGS(&SrvDescriptorHeap)));
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
	{ reinterpret_cast<BYTE*>(Shaders["Vertex"]->GetBufferPointer()),
		Shaders["Vertex"]->GetBufferSize() };
	OpaquePsoDesc.PS =
	{ reinterpret_cast<BYTE*>(Shaders["Pixel"]->GetBufferPointer()),
		Shaders["Pixel"]->GetBufferSize() };

	OpaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	OpaquePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;

	OpaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	OpaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	OpaquePsoDesc.DSVFormat = DepthStencilFormat;
	OpaquePsoDesc.NumRenderTargets = 1;
	OpaquePsoDesc.RTVFormats[0] = BackBufferFormat;

	OpaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	OpaquePsoDesc.SampleMask = UINT_MAX;
	OpaquePsoDesc.SampleDesc.Count = 1;
	OpaquePsoDesc.SampleDesc.Quality = 0;
	ThrowIfFailed(DxDevice3D->CreateGraphicsPipelineState(&OpaquePsoDesc, IID_PPV_ARGS(&PSO["Opaque"])));

	//
	 // PSO for shadow map pass.
	 //
	D3D12_GRAPHICS_PIPELINE_STATE_DESC smapPsoDesc = OpaquePsoDesc;
	smapPsoDesc.RasterizerState.DepthBias = 1000;
	smapPsoDesc.RasterizerState.DepthBiasClamp = 0.0f;
	smapPsoDesc.RasterizerState.SlopeScaledDepthBias = 2.0f;
	smapPsoDesc.pRootSignature = RootSignature.Get();
	smapPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(Shaders["ShadowVS"]->GetBufferPointer()),
		Shaders["ShadowVS"]->GetBufferSize()
	};
	smapPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(Shaders["ShadowPS"]->GetBufferPointer()),
		Shaders["ShadowPS"]->GetBufferSize()
	};
	smapPsoDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
	smapPsoDesc.NumRenderTargets = 0;
	ThrowIfFailed(DxDevice3D->CreateGraphicsPipelineState(&smapPsoDesc, IID_PPV_ARGS(&PSO["ShadowOpaque"])));

	//ShdaowMap Debug Layer
	D3D12_GRAPHICS_PIPELINE_STATE_DESC SmapDebugPsoDesc = OpaquePsoDesc;
	SmapDebugPsoDesc.pRootSignature = RootSignature.Get();
	SmapDebugPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(Shaders["ShadowDebugVS"]->GetBufferPointer()),
		Shaders["ShadowDebugVS"]->GetBufferSize()
	};
	SmapDebugPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(Shaders["ShadowDebugPS"]->GetBufferPointer()),
		Shaders["ShadowDebugPS"]->GetBufferSize()
	};
	ThrowIfFailed(DxDevice3D->CreateGraphicsPipelineState(&SmapDebugPsoDesc, IID_PPV_ARGS(&PSO["ShadowDebug"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC SkyPsoDesc = OpaquePsoDesc;
	SkyPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	SkyPsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	SkyPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(Shaders["SkyVertex"]->GetBufferPointer()),
		Shaders["SkyVertex"]->GetBufferSize()
	};
	SkyPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(Shaders["SkyPixel"]->GetBufferPointer()),
		Shaders["SkyPixel"]->GetBufferSize()
	};
	ThrowIfFailed(DxDevice3D->CreateGraphicsPipelineState(&SkyPsoDesc, IID_PPV_ARGS(&PSO["Sky"])));
}


void ShapesApp::SaveRenderItemsData()
{
	std::ofstream OfileStream("RenderItems_metadata.txt");
	if (!OfileStream.is_open())
		return;
	auto OpaqRenderItems = RenderLayerItems[(int)RenderLayer::Opaque];
	for (auto RenderItem : OpaqRenderItems)
	{
		auto world = RenderItem->World;
		OfileStream << RenderItem->Name << " "
			<< world._11 << " " << world._12 << " " << world._13 << " " << world._14 << " "
			<< world._21 << " " << world._22 << " " << world._23 << " " << world._24 << " "
			<< world._31 << " " << world._32 << " " << world._33 << " " << world._34 << " "
			<< world._41 << " " << world._42 << " " << world._43 << " " << world._44 << "\n";
	}
	OfileStream.close();
	OutputDebugStringA("Rendered Items Location Cached");
}

void ShapesApp::LoadRenderItemsData()
{
	std::ifstream IfileStream("RenderItems_metadata.txt");
	if (!IfileStream.is_open())
		return;
	auto OpaqRenderItems = RenderLayerItems[(int)RenderLayer::Opaque];

	std::string StringLine;
	while (std::getline(IfileStream, StringLine))
	{
		std::istringstream IStringStream(StringLine);

		std::string RiName;
		DirectX::XMFLOAT4X4 RiWorld;
		if (IStringStream >> RiName
			>> RiWorld._11 >> RiWorld._12 >> RiWorld._13 >> RiWorld._14
			>> RiWorld._21 >> RiWorld._22 >> RiWorld._23 >> RiWorld._24
			>> RiWorld._31 >> RiWorld._32 >> RiWorld._33 >> RiWorld._34
			>> RiWorld._41 >> RiWorld._42 >> RiWorld._43 >> RiWorld._44)
		{
			for (auto& RenderItem : OpaqRenderItems)
			{
				if (RenderItem->Name == RiName)
				{
					RenderItem->World = RiWorld;
					break;
				}
			}
		}
	}
	IfileStream.close();
	OutputDebugStringA("Rendered Items World Location Loaded");
}

ShapesApp::RenderItem* ShapesApp::AddRenderItem(std::unique_ptr<RenderItem> aRenderItem, RenderLayer aLayer)
{
	if (!aRenderItem)
	{
		std::string ErrorMsg = "[Error] Attempted to add null RenderItem\n";
		::OutputDebugStringA(ErrorMsg.c_str());
		assert(false && "Attempted to add null RenderItem");
		return nullptr;
	}

	// Check for duplicate name
	for (const auto& ExistingItem : RenderItems)
	{
		if (ExistingItem->Name == aRenderItem->Name)
		{
			std::string ErrorMsg = "[Error] RenderItem with name '" + aRenderItem->Name + "' already exists\n";
			::OutputDebugStringA(ErrorMsg.c_str());
			assert(false && "RenderItem with duplicate name already exists");
			return nullptr;
		}
	}

	// Get raw pointer before moving
	RenderItem* RawPtr = aRenderItem.get();

	// Add to main RenderItems vector
	RenderItems.push_back(std::move(aRenderItem));

	// Add to appropriate layer
	RenderLayerItems[(int)aLayer].push_back(RawPtr);

	return RawPtr;
}
