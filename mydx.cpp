#include "mydx.hpp"
#include "ShaderCollection.hpp"
#include "lpngDX.hpp"

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <DirectXColors.h>
#include <DirectXMath.h>
#include <d3dcompiler.h>

#include <iostream>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "dxguid.lib")



template <UINT TDebugNameLength>
inline void SetDebugName(_In_ ID3D11DeviceChild* deviceResource, _In_z_ const char(&debugName)[TDebugNameLength])
{
	deviceResource->SetPrivateData(WKPDID_D3DDebugObjectName, TDebugNameLength - 1, debugName);
}

mydx::mydx(const std::string& title)
	: Application(title)
{

}

mydx::~mydx()
{
	_deviceContext->Flush();
	_textureSrv.Reset();
	_triangleVertices.Reset();
	_linearSamplerState.Reset();
	_rasterState.Reset();
	_shaderCollection.Destroy();
	DestroySwapchainResources();
	_swapChain.Reset();
	_dxgiFactory.Reset();
	_deviceContext.Reset();
#if !defined(NDEBUG)
	_debug->ReportLiveDeviceObjects(D3D11_RLDO_FLAGS::D3D11_RLDO_DETAIL);
	_debug.Reset();
#endif
	_device.Reset();
}

bool mydx::Initialize() {
	if (!Application::Initialize()) {
		std::cerr << "Application init fail\n";
		return false;
	}
	std::cout << "Debug window initialized\n";
	if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&_dxgiFactory)))) {
		std::cerr << "DXGI: Failed to create factory\n";
		return false;
	}
	constexpr D3D_FEATURE_LEVEL deviceFeatureLevel = D3D_FEATURE_LEVEL::D3D_FEATURE_LEVEL_11_0;
	uint32_t deviceFlags = 0;
#if !defined(NDEBUG)
	deviceFlags |= D3D11_CREATE_DEVICE_FLAG::D3D11_CREATE_DEVICE_DEBUG;
#endif

	WRL::ComPtr<ID3D11DeviceContext> deviceContext;
	if (FAILED(D3D11CreateDevice(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		deviceFlags,
		&deviceFeatureLevel,
		1,
		D3D11_SDK_VERSION,
		&_device,
		nullptr,
		&deviceContext))) 
	{
		std::cerr << "Failed to create device context.\n";
		return false;
	}

#if !defined(NDEBUG)
	if (FAILED(_device.As(&_debug))) 
	{
		std::cerr << "D3D11: Failed to get the debug layer from the device\n";
		return false;
	}
#endif

	_deviceContext = deviceContext;


	constexpr char deviceName[] = "MainDevice1";
	_device->SetPrivateData(WKPDID_D3DDebugObjectName, sizeof(deviceName), deviceName);
	SetDebugName(_deviceContext.Get(), "DeviceContext1");

	DXGI_SWAP_CHAIN_DESC1 swapChainDescriptor = {};
	swapChainDescriptor.Width = GetWindowWidth();
	swapChainDescriptor.Height = GetWindowHeight();
	swapChainDescriptor.Format = DXGI_FORMAT::DXGI_FORMAT_B8G8R8A8_UNORM;
	swapChainDescriptor.SampleDesc.Count = 1;
	swapChainDescriptor.SampleDesc.Quality = 0;
	swapChainDescriptor.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDescriptor.BufferCount = 2;
	swapChainDescriptor.SwapEffect = DXGI_SWAP_EFFECT::DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDescriptor.Scaling = DXGI_SCALING::DXGI_SCALING_STRETCH;
	swapChainDescriptor.Flags = {};//DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;//{};

	DXGI_SWAP_CHAIN_FULLSCREEN_DESC swapChainFullscreenDescriptor = {};
	swapChainFullscreenDescriptor.Windowed = true;


	if (FAILED(_dxgiFactory->CreateSwapChainForHwnd(
		_device.Get(),
		glfwGetWin32Window(GetWindow()),
		&swapChainDescriptor,
		&swapChainFullscreenDescriptor,
		nullptr,
		&_swapChain)))
	{
		std::cerr << "DXGI: Failed to create SwapChain\n";
		return false;
	}

	CreateSwapchainResources();

	CreateConstantBuffers();

	// i guess it is not needed, leave it in case thoughTODO: init libpng?(needed?)

	return true;
}

void mydx::OnResize(const int32_t width, const int32_t height)
{
	Application::OnResize(width, height);
	
	_deviceContext->Flush();
	DestroySwapchainResources();

	if (FAILED(_swapChain->ResizeBuffers(
		0,
		width,
		height,
		DXGI_FORMAT::DXGI_FORMAT_B8G8R8A8_UNORM,
		0)))
	{
		std::cerr << "D3D11: Failed to recreate swapchain buffers\n";
		return;
	}

	CreateSwapchainResources();
}

WRL::ComPtr<ID3D11ShaderResourceView> CreateTextureView(ID3D11Device* device, const std::wstring& pathToTexture)
{
	uint8_t** image = new(uint8_t*);
	int width = 0, height = 0; // Written to in the loadPngImage function
	constexpr int bitPerPixel = 32;
	bool loadSuccess = loadPngImage(pathToTexture.c_str(), width, height, image);
	if (loadSuccess != true) {
		std::cerr << "loadPngImage Failed, error\n";
		return nullptr;
	}

	D3D11_TEXTURE2D_DESC textureDesc = {};
	D3D11_SUBRESOURCE_DATA initialData = {};
	WRL::ComPtr<ID3D11Texture2D> texture = nullptr;

	DXGI_FORMAT textureFormat;
	if (bitPerPixel == 32) { // for now only 32 bit PNG Support
		textureFormat = DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM;
	} 
	else {
		//we could try to handle some weird bitcount, but these will probably be HDR or some antique format, just exit instead..
		std::cerr << "CreateTextureView: Texture has nontrivial bits per pixel ( " << bitPerPixel << " )\n";
		return nullptr;
	}
	textureDesc.Format = textureFormat;
	textureDesc.ArraySize = 1;
	textureDesc.MipLevels = 1;
	textureDesc.Height = height;
	textureDesc.Width = width;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.Usage = D3D11_USAGE_IMMUTABLE;
	textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	//populate initial data
	initialData.pSysMem = *image;
	initialData.SysMemPitch = (bitPerPixel / 8) * width;
	initialData.SysMemSlicePitch = 0;

	if (FAILED(device->CreateTexture2D(&textureDesc, &initialData, texture.GetAddressOf())))
	{
		std::cerr << "Issue creating 2D Texture from image\n";
		free(*image);
		return nullptr;
	}
	free(*image);

	ID3D11ShaderResourceView* srv = nullptr;
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = textureDesc.Format;
	srvDesc.Texture2D.MipLevels = textureDesc.MipLevels;

	if (FAILED(device->CreateShaderResourceView(texture.Get(), &srvDesc, &srv)))
	{
		std::wcerr << "CreateTextureView: Failed to create SRV from texture: '" << pathToTexture << "'\n";
		return nullptr;
	}
	return srv;
}

bool mydx::Load() {
	ShaderCollectionDescriptor shaderCollDesc = {};
	shaderCollDesc.VertexShaderFilePath = L"Assets/Shaders/Main.VS.hlsl";
	shaderCollDesc.PixelShaderFilePath = L"Assets/Shaders/Main.PS.hlsl";
	shaderCollDesc.VertexType = VertexType::PositionColorUv;

	_shaderCollection = ShaderCollection::CreateShaderCollection(shaderCollDesc, _device.Get());

	constexpr VertexPositionColorUv vertices[] = {
	{  Position{ 0.01f, 0.5f, 0.01f }, Color{ 0.20f, 0.29f, 0.09f }, Uv{ 0.5f, 0.0f }},
	{ Position{ 0.5f, -0.5f, 0.01f }, Color{ 0.33f, 0.55f, 0.19f }, Uv{ 1.0f, 1.0f }},
	{Position{ -0.5f, -0.5f, 0.01f }, Color{ 0.28f, 0.45f, 0.13f }, Uv{ 0.0f, 1.0f }},
	};

	D3D11_BUFFER_DESC bufferInfo = {};
	bufferInfo.ByteWidth = sizeof(vertices);
	bufferInfo.Usage = D3D11_USAGE_IMMUTABLE;
	bufferInfo.BindFlags = D3D11_BIND_FLAG::D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA resourceData = {};
	resourceData.pSysMem = vertices;

	if (FAILED(_device->CreateBuffer(
		&bufferInfo,
		&resourceData,
		&_triangleVertices)))
	{
		std::cerr << "D3D11: Failed to create triangle Vertex Buffer.\n";
		return false;
	}

	const std::wstring imagePath = L"Assets/images/image.png";
	_textureSrv = CreateTextureView(_device.Get(), imagePath);
	if (_textureSrv == nullptr)
	{
		//this is "fine", we can use our fallback!
		//_textureSrv = _fallbackTextureSrv;
		//actually i dont have a fallback, panic
		std::wcerr << "Failed to create TextureView from file: '" << imagePath << "'\n";
		return false;
	}

	D3D11_SAMPLER_DESC linearSamplerStateDescriptor = {};
	linearSamplerStateDescriptor.Filter = D3D11_FILTER::D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
	linearSamplerStateDescriptor.AddressU = D3D11_TEXTURE_ADDRESS_MODE::D3D11_TEXTURE_ADDRESS_WRAP;
	linearSamplerStateDescriptor.AddressV = D3D11_TEXTURE_ADDRESS_MODE::D3D11_TEXTURE_ADDRESS_WRAP;
	linearSamplerStateDescriptor.AddressW = D3D11_TEXTURE_ADDRESS_MODE::D3D11_TEXTURE_ADDRESS_WRAP;


	if (FAILED(_device->CreateSamplerState(&linearSamplerStateDescriptor, &_linearSamplerState)))
	{
		std::cerr << "D3D11: Failed to create linear sampler state\n";
		return false;
	}

	return true;
}

void mydx::CreateConstantBuffers()
{
	D3D11_BUFFER_DESC desc = {};
	desc.Usage = D3D11_USAGE::D3D11_USAGE_DYNAMIC;
	desc.BindFlags = D3D11_BIND_FLAG::D3D11_BIND_CONSTANT_BUFFER;
	desc.ByteWidth = sizeof(PerFrameConstantBuffer);
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_FLAG::D3D11_CPU_ACCESS_WRITE;

	_device->CreateBuffer(&desc, nullptr, &_perFrameConstantBuffer);

	desc.ByteWidth = sizeof(PerObjectConstantBuffer);
	_device->CreateBuffer(&desc, nullptr, &_perObjectConstantBuffer);
}

bool mydx::CreateSwapchainResources() {
	WRL::ComPtr<ID3D11Texture2D> backBuffer = nullptr;
	if (FAILED(_swapChain->GetBuffer(
		0,
		IID_PPV_ARGS(&backBuffer))))
	{
		std::cerr << "D3D11: Failed to get back buffer from swapchain\n";
	}

	if (FAILED(_device->CreateRenderTargetView(
		backBuffer.Get(),
		nullptr,
		&_renderTarget)))
	{
		std::cerr << "D3D11: Failed to create rendertarget view from back buffer\n";
		return false;
	}

	return true;
}

void mydx::DestroySwapchainResources()
{
	_renderTarget.Reset();
}

void mydx::Update()
{
	Application::Update();

	using namespace DirectX;

	static float _xRotation = -0.140f;
	static float _yRotation = 3.140f;
	static float _scale = .5f;
	static XMFLOAT3 _cameraPosition = { 0.0f, 0.0f, -1.0f };

	_scale += _deltaTime / 10000.0;
	_xRotation += _deltaTime / 10000.0;
	_yRotation += _deltaTime / 1000.0;

	/*		   CAMERA			*/
	XMVECTOR camPos = XMLoadFloat3(&_cameraPosition);

	XMMATRIX view = XMMatrixLookAtRH(camPos, g_XMZero, { 0, 1, 0, 1 });
	XMMATRIX proj = XMMatrixPerspectiveFovRH(90.0f * 0.0174533f,
		static_cast<float>(_width) / static_cast<float>(_height),
		0.01f,
		100.0f);

	XMMATRIX viewProjection = XMMatrixMultiply(view, proj);
	XMStoreFloat4x4(&_perFrameConstantBufferData.viewProjectionMatrix, viewProjection);

	/*			OBJECT			*/
	XMMATRIX translation = XMMatrixTranslation(0, 0, 0);
	XMMATRIX scaling = XMMatrixScaling(_scale, _scale, _scale);
	XMMATRIX rotation = XMMatrixRotationRollPitchYaw(_xRotation, _yRotation, 0.0f);

	XMMATRIX modelMatrix = XMMatrixMultiply(translation, XMMatrixMultiply(scaling, rotation));
	XMStoreFloat4x4(&_perObjectConstantBufferData.modelMatrix, modelMatrix);

	// update Constant Buffer
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	_deviceContext->Map(_perFrameConstantBuffer.Get(), 0, D3D11_MAP::D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	memcpy(mappedResource.pData, &_perFrameConstantBufferData, sizeof(PerFrameConstantBuffer));
	_deviceContext->Unmap(_perFrameConstantBuffer.Get(), 0);

	_deviceContext->Map(_perObjectConstantBuffer.Get(), 0, D3D11_MAP::D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	memcpy(mappedResource.pData, &_perObjectConstantBufferData, sizeof(PerObjectConstantBuffer));
	_deviceContext->Unmap(_perObjectConstantBuffer.Get(), 0);
}

void mydx::Render() {
	constexpr float clearColor[] = { .0858823f, .0019607f, .0227451f };
	constexpr UINT vertexOffset = 0;

	constexpr ID3D11RenderTargetView* nullTarget = nullptr;

	//set to 0 so we can clear properly
	_deviceContext->OMSetRenderTargets(1, &nullTarget, nullptr);
	_deviceContext->ClearRenderTargetView(_renderTarget.Get(), clearColor);
	_deviceContext->OMSetRenderTargets(1, _renderTarget.GetAddressOf(), nullptr);

	_deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY::D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	D3D11_BUFFER_DESC description = {};
	_triangleVertices->GetDesc(&description);
	UINT stride = _shaderCollection.GetLayoutByteSize(VertexType::PositionColorUv);
	_deviceContext->IASetVertexBuffers(
		0,
		1,
		_triangleVertices.GetAddressOf(),
		&stride,
		&vertexOffset);

	_shaderCollection.ApplyToContext(_deviceContext.Get());


	D3D11_VIEWPORT viewport = {
		0.0f,
		0.0f,
		static_cast<float>(GetWindowWidth()),
		static_cast<float>(GetWindowHeight()),
		0.0f,
		1.0f
	};

	_deviceContext->RSSetViewports(1, &viewport);
	_deviceContext->RSSetState(_rasterState.Get());

	_deviceContext->PSSetShaderResources(0, 1, _textureSrv.GetAddressOf());
	_deviceContext->PSSetSamplers(0, 1, _linearSamplerState.GetAddressOf());

	ID3D11Buffer* constantBuffers[2] =
	{
		_perFrameConstantBuffer.Get(),
		_perObjectConstantBuffer.Get()
	};

	_deviceContext->VSSetConstantBuffers(1, 2, constantBuffers);

	//constexpr UINT presentFlags = 0x00000200UL;
	_deviceContext->Draw(3, 0);
	_swapChain->Present(1, 0);
}

