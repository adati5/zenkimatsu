//ウィンドウ表示＆DirectX初期化
#include<Windows.h>
#include<tchar.h>
#include <DirectXMath.h>
#include<d3d12.h>
#include<dxgi1_6.h>
#include <d3dcompiler.h>
#include<vector>
#ifdef _DEBUG
#include<iostream>
#endif

#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using namespace std;
using namespace DirectX;

// @brief コンソール画面にフォーマット付き文字列を表示
// @param formatフォーマット（%dとか%fとかの）
// @param 可変長引数
// @remarks この関数はデバッグ用です。デバッグ時にしか動作しません
void DebugOutputFormatString(const char* format, ...) {
#ifdef _DEBUG
	va_list valist;
	va_start(valist, format);
	vprintf(format, valist);
	va_end(valist);
#endif
}

LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	//ウィンドウが破棄されたら呼ばれる
	if (msg == WM_DESTROY) {
		PostQuitMessage(0); //OSに対してこのOSは終わると伝える
		return 0;
	}
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

const unsigned int window_width = 1280;
const unsigned int window_height = 720;

ID3D12Device* _dev = nullptr;
IDXGIFactory6* _dxgiFactory = nullptr;
IDXGISwapChain4* _swapchain = nullptr;
ID3D12CommandAllocator* _cmdAllocator = nullptr;
ID3D12GraphicsCommandList* _cmdList = nullptr;
ID3D12CommandQueue* _cmdQueue = nullptr;

void EnableDebugLayer() {
	ID3D12Debug* debugLayer = nullptr;
	//if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer)))) {
	//	debugLayer->EnableDebugLayer(); //  デバッグレイヤーを有効化する
	//	debugLayer->Release(); //  有効化したらインターフェイスを解放する
	//}
	auto result = D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer));
	if (result == S_OK) {
		debugLayer->EnableDebugLayer(); //  デバッグレイヤーを有効化する
		debugLayer->Release(); //  有効化したらインターフェイスを解放する
	}
}



#ifdef _DEBUG
int main() {
#else
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
#endif
	DebugOutputFormatString("Show window test.");

	//　ウィンドウクラスの生成＆登録
	WNDCLASSEX w = {};
	w.cbSize = sizeof(WNDCLASSEX);
	w.lpfnWndProc = (WNDPROC)WindowProcedure; // コールバック関数の指定
	w.lpszClassName = _T("DX12Sample");       // アプリケーションクラス名（適当でよい）
	w.hInstance = GetModuleHandle(nullptr);   // ハンドルの取得
	RegisterClassEx(&w); // アプリケーションクラス（ウィンドウクラスの指定をOSに伝える）
	RECT wrc = { 0, 0, window_width, window_height };  // ウィンドウサイズを決める

	// 関数を使ってウィンドウのサイズを補正する
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);// ウィンドウオブジェクトの生成
	HWND hwnd = CreateWindow(w.lpszClassName,      // クラス名指定
		_T("DX12テスト"),                                               // タイトルバーの文字
		WS_OVERLAPPEDWINDOW,                               // タイトルバーと境界線があるウィンドウ
		CW_USEDEFAULT,                                             // 表示x座標はOSにお任せ
		CW_USEDEFAULT,                                             // 表示y座標はOSにお任せ 
		wrc.right - wrc.left,                                           // ウィンドウ幅
		wrc.bottom - wrc.top,                                       // ウィンドウ高
		nullptr,                // 親ウィンドウハンドル
		nullptr,                                 // メニューハンドル  
		w.hInstance,            // 呼び出しアプリケーションハンドル
		nullptr);               // 追加パラメーター// ウィンドウ表示ShowWindow(hwnd, SW_SHOW);

#ifdef _DEBUG
//デバッグレイヤーをオンに
//デバイス生成時前にやっておかないと、デバイス生成後にやると
//デバイスがロスとしてしまうので注意
	EnableDebugLayer();
#endif

	// DirectX12の導入
	//1.IDXGIFactoryを生成
	auto result = S_OK;
#ifdef _DEBUG
	result = CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&_dxgiFactory));
#else
	result = CreateDXGIFactory1(IID_PPV_ARGS(&_dxgiFactory));
#endif
	if (result != S_OK) {
		DebugOutputFormatString("FAILED CreateDXGIFactory");
		return -1;
	}

	//2.VGAアダプタIDXGIAdapterの配列をIDXGIFactory6から取り出す

	std::vector <IDXGIAdapter*> adapters;
	IDXGIAdapter* tmpAdapter = nullptr;
	for (int i = 0; _dxgiFactory->EnumAdapters(i, &tmpAdapter) != DXGI_ERROR_NOT_FOUND; ++i) {
		adapters.push_back(tmpAdapter);
	}

	//3.使いたいアダプタをVGAのメーカーで選ぶ
	for (auto adpt : adapters) {
		DXGI_ADAPTER_DESC adesc = {};
		adpt->GetDesc(&adesc); // アダプターの説明オブジェクト取得
		std::wstring strDesc = adesc.Description;    // 探したいアダプターの名前を確認
		if (strDesc.find(L"NVIDIA") != std::string::npos) {
			tmpAdapter = adpt;
			break;
		}
	}
	//4.ID3D12Deviceを選んだアダプタを用いて初期化し生成する
	D3D_FEATURE_LEVEL levels[] =
	{
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};
	D3D_FEATURE_LEVEL featureLevel;
	for (auto l : levels) {
		if (D3D12CreateDevice(tmpAdapter, l, IID_PPV_ARGS(&_dev)) == S_OK) {
			featureLevel = l;
			break;
		}
	}

	//コマンド周りの準備
	// コマンドアロケーターID3D12CommandAllocatorを生成
	result = _dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_cmdAllocator));
	if (result != S_OK) {
		DebugOutputFormatString("FAILED CreateCommandAllocator");
		return -1;
	}
	// コマンドリストID3D12GraphicsCommandListを生成
	result = _dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _cmdAllocator, nullptr, IID_PPV_ARGS(&_cmdList));
	if (result != S_OK) {
		DebugOutputFormatString("FAILED CreateCommandList");
		return -1;
	}

	// コマンドキューD3D12_COMMAND_QUEUE_DESCに関する設定を作成 
	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
	cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;//タイムアウトなし
	cmdQueueDesc.NodeMask = 0;
	cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;//プライオリティ特に指定なし
	cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;//ここはコマンドリストと合わせてください
	// コマンドキューID3D12CommandQueueを生成
	result = _dev->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(&_cmdQueue));//コマンドキュー生成
	if (result != S_OK) {
		DebugOutputFormatString("FAILED CreateCommandQueue");
		return -1;
	}

	/////////////
	//スワップチェーン

	DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};
	swapchainDesc.Width = window_width;
	swapchainDesc.Height = window_height;
	swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapchainDesc.Stereo = false;
	swapchainDesc.SampleDesc.Count = 1;
	swapchainDesc.SampleDesc.Quality = 0;
	swapchainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
	swapchainDesc.BufferCount = 2;// バックバッファーは伸び縮み可能
	swapchainDesc.Scaling = DXGI_SCALING_STRETCH;// フリップ後は速やかに破棄
	swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;// 特に指定なし
	swapchainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;// ウィンドウ⇔フルスクリーン切り替え可能
	swapchainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	result = _dxgiFactory->CreateSwapChainForHwnd(
		_cmdQueue,
		hwnd,
		&swapchainDesc,
		nullptr,
		nullptr,
		(IDXGISwapChain1**)&_swapchain);
	if (result != S_OK) {
		DebugOutputFormatString("FAILED CreateSwapChainForHwnd");
		return -1;
	}

	///////////
	// ディスクリプターヒープの確保

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;//レンダーターゲットビューなので当然RTV
	heapDesc.NodeMask = 0;
	heapDesc.NumDescriptors = 2;//表裏の２つ
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;//特に指定なし
	ID3D12DescriptorHeap* rtvHeaps = nullptr;
	result = _dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&rtvHeaps));
	if (result != S_OK) {
		DebugOutputFormatString("FAILED CreateDescriptorHeap");
		return -1;
	}

	////////////
	//ディスクリプターヒープ内にレンダーターゲットビューを作成
	DXGI_SWAP_CHAIN_DESC swcDesc = {};
	result = _swapchain->GetDesc(&swcDesc);
	if (result != S_OK) {
		DebugOutputFormatString("FAILED IDXGISwapChain4->GetDesc");
		return -1;
	}
	std::vector<ID3D12Resource*> _backBuffers(swcDesc.BufferCount);
	D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
	for (UINT i = 0; i < swcDesc.BufferCount; ++i) {
		result = _swapchain->GetBuffer(static_cast<UINT>(i), IID_PPV_ARGS(&_backBuffers[i]));	//バッファの位置のハンドルを取り出す
		if (result != S_OK) {
			DebugOutputFormatString("FAILED IDXGISwapChain4->GetBuffer");
			return -1;
		}
		_dev->CreateRenderTargetView(_backBuffers[i], nullptr, handle);//RTVをディスクリプターヒープに作成
		handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);// ディスクリプタの先頭アドレズをRTVのサイズ分、後ろへずらず
	}

	// フェンスを用意
	ID3D12Fence* _fence = nullptr;
	UINT64 _fenceVal = 0;
	result = _dev->CreateFence(_fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence));

	ShowWindow(hwnd, SW_SHOW);

	// 座標の定義
	XMFLOAT3 vertices[] = {
	{-0.5f, -0.7f, 0.0f} , //  左下
		{ -0.5f,  0.7f, 0.0f} , //  左上 
		{ 0.5f, -0.7f, 0.0f} , //  右下

		{ -0.5f, 0.7f, 0.0f} ,
		{ 0.5f, 0.7f, 0.0f} ,
		{ 0.5f, -0.7f, 0.0f},
	};


	D3D12_HEAP_PROPERTIES heapprop = {};
	heapprop.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapprop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapprop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	D3D12_RESOURCE_DESC resdesc = {};
	resdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resdesc.Width = sizeof(vertices);
	resdesc.Height = 1;
	resdesc.DepthOrArraySize = 1;
	resdesc.MipLevels = 1;
	resdesc.Format = DXGI_FORMAT_UNKNOWN;
	resdesc.SampleDesc.Count = 1;
	resdesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	resdesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	ID3D12Resource* vertBuff = nullptr;
	result = _dev->CreateCommittedResource(
		&heapprop,
		D3D12_HEAP_FLAG_NONE,
		&resdesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr, IID_PPV_ARGS(&vertBuff));

	if (result != S_OK) {
		DebugOutputFormatString("FAILED ID3D12Device::CreateCommittedResource");
		return -1;
	}
	XMFLOAT3* vertMap = nullptr;
	result = vertBuff->Map(0, nullptr, (void**)&vertMap);
	if (result != S_OK) {
		DebugOutputFormatString("FAILED ID3D12Resource::Map at transmitting Vertex Buffer");
		return -1;
	}
	std::copy(std::begin(vertices), std::end(vertices), vertMap);
	vertBuff->Unmap(0, nullptr);

	// 頂点バッファビューを用意
	D3D12_VERTEX_BUFFER_VIEW vbView = {};
	vbView.BufferLocation = vertBuff->GetGPUVirtualAddress(); // バッファーの仮想アドレス
	vbView.SizeInBytes = sizeof(vertices);      // 全バイト数
	vbView.StrideInBytes = sizeof(vertices[0]); // 1頂点あたりのバイト数

	//　Shader用のBlobを用意
	ID3DBlob* _vsBlob = nullptr;	//　頂点シェーダー
	ID3DBlob* _psBlob = nullptr;	// ピクセルシェーダー
	ID3DBlob* errorBlob = nullptr;	//　エラー格納用
	//　頂点シェーダーをコンパイルする
	result = D3DCompileFromFile(
		L"BasicVertexShader.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"BasicVS",
		"vs_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0, &_vsBlob, &errorBlob);
	if (SUCCEEDED(result))
	{	//　頂点シェーダーのコンパイル成功、ピクセルシェーダーのコンパイル
		result = D3DCompileFromFile(
			L"BasicPixelShader.hlsl",
			nullptr,
			D3D_COMPILE_STANDARD_FILE_INCLUDE,
			"BasicPS",
			"ps_5_0",
			D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
			0, &_psBlob, &errorBlob);
		if (FAILED(result)) {// コンパイルエラーの場合
			if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
				::OutputDebugStringA("シェーダーファイルが見当たりません");
			}
			else {
				std::string errstr;
				errstr.resize(errorBlob->GetBufferSize());
				std::copy_n((char*)errorBlob->GetBufferPointer(), errorBlob->GetBufferSize(), errstr.begin());
				errstr += "\n";
				OutputDebugStringA(errstr.c_str());
			}
			exit(1);
		}
	}
	else {
		if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
			::OutputDebugStringA("シェーダーファイルが見当たりません");
		}
		else {
			std::string errstr;
			errstr.resize(errorBlob->GetBufferSize());
			std::copy_n((char*)errorBlob->GetBufferPointer(), errorBlob->GetBufferSize(), errstr.begin());
			errstr += "\n";
			OutputDebugStringA(errstr.c_str());
		}
		exit(1);
	}
	// 頂点レイアウト
	D3D12_INPUT_ELEMENT_DESC inputLayout[] =
	{
		{
			"POSITION",
			0,
			DXGI_FORMAT_R32G32B32_FLOAT,
			0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},
	};

	// パイプラインステートの設定
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeline = {};
	gpipeline.pRootSignature = nullptr;
	gpipeline.VS.pShaderBytecode = _vsBlob->GetBufferPointer();
	gpipeline.VS.BytecodeLength = _vsBlob->GetBufferSize();
	gpipeline.PS.pShaderBytecode = _psBlob->GetBufferPointer();
	gpipeline.PS.BytecodeLength = _psBlob->GetBufferSize();
	// gpipeline.StreamOutput.NumEntriesについては未指定

//  デフォルトのサンプルマスクを表す定数（0xffffffff）
	gpipeline.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	gpipeline.BlendState.AlphaToCoverageEnable = false;
	gpipeline.BlendState.IndependentBlendEnable = false;

	D3D12_RENDER_TARGET_BLEND_DESC renderTargetBlendDesc = {};

	//ひとまず加算や乗算やαブレンディングは使用しない
	renderTargetBlendDesc.BlendEnable = false;
	renderTargetBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	//ひとまず論理演算は使用しない
	renderTargetBlendDesc.LogicOpEnable = false;

	gpipeline.BlendState.RenderTarget[0] = renderTargetBlendDesc;

	// D3D12_RASTERIZER_DESC の設定
	gpipeline.RasterizerState.MultisampleEnable = false;//まだアンチェリは使わない
	gpipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;//カリングしない
	gpipeline.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;//中身を塗りつぶす
	gpipeline.RasterizerState.DepthClipEnable = true;//深度方向のクリッピングは有効に

	gpipeline.RasterizerState.FrontCounterClockwise = false;
	gpipeline.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	gpipeline.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	gpipeline.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	gpipeline.RasterizerState.AntialiasedLineEnable = false;
	gpipeline.RasterizerState.ForcedSampleCount = 0;
	gpipeline.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	// D3D12_DEPTH_STENCIL_DESC　深度ステンシル   
	gpipeline.DepthStencilState.DepthEnable = false;
	gpipeline.DepthStencilState.StencilEnable = false;

	// 予め用意した頂点レイアウトを設定
	gpipeline.InputLayout.pInputElementDescs = inputLayout;//レイアウト先頭アドレス
	gpipeline.InputLayout.NumElements = _countof(inputLayout);//レイアウト配列数

	gpipeline.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED; //  カットなし

	//  三角形で構成
	gpipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	//　レンダーターゲットの設定
	gpipeline.NumRenderTargets = 1; // 今は1つのみ
	gpipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM; // 0～1に正規化されたRGBA

	gpipeline.SampleDesc.Count = 1;   // サンプリングは1ピクセルにつき1
	gpipeline.SampleDesc.Quality = 0; // クオリティは最低

	// ルートシグネチャの作成
	ID3D12RootSignature* rootsignature = nullptr;
	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	ID3DBlob* rootSigBlob = nullptr;
	result = D3D12SerializeRootSignature(
		&rootSignatureDesc,									//ルートシグネチャ設定        
		D3D_ROOT_SIGNATURE_VERSION_1_0,		// ルートシグネチャバージョン        
		&rootSigBlob,												// シェーダーを作ったときと同じ        
		&errorBlob);												// エラー処理も同じ
	if (!SUCCEEDED(result)) {
		DebugOutputFormatString("FAILED SerializeRootSignature");
		return -1;
	}

	result = _dev->CreateRootSignature(
		0, // nodemask。0でよい    
		rootSigBlob->GetBufferPointer(), // シェーダーのときと同様    
		rootSigBlob->GetBufferSize(),    // シェーダーのときと同様    
		IID_PPV_ARGS(&rootsignature));
	if (!SUCCEEDED(result)) {
		DebugOutputFormatString("FAILED ID3D12Device::CreateRootSignature");
		return -1;
	}

	rootSigBlob->Release(); // 不要になったので解放
	gpipeline.pRootSignature = rootsignature;

	ID3D12PipelineState* _pipelinestate = nullptr;
	result = _dev->CreateGraphicsPipelineState(&gpipeline, IID_PPV_ARGS(&_pipelinestate));
	if (result != S_OK) {
		DebugOutputFormatString("FAILED ID3D12Device::CreateGraphicsPipelineState method");
		return -1;
	}

	//　ビューポート
	D3D12_VIEWPORT viewport = {};
	viewport.Width = window_width;   // 出力先の幅（ピクセル数）
	viewport.Height = window_height; // 出力先の高さ（ピクセル数）
	viewport.TopLeftX = 0;    // 出力先の左上座標X
	viewport.TopLeftY = 0;    // 出力先の左上座標Y
	viewport.MaxDepth = 1.0f; // 深度最大値
	viewport.MinDepth = 0.0f; // 深度最小値

	// シザー矩形
	D3D12_RECT scissorrect = {};
	scissorrect.top = 0;  // 切り抜き上座標
	scissorrect.left = 0; //  切り抜き左座標
	scissorrect.right = scissorrect.left + window_width;  //  切り抜き右座標
	scissorrect.bottom = scissorrect.top + window_height; //  切り抜き下座標

	MSG msg = {};
	while (true) {
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}    //アプリケーションが終わるときにmessageがWM_QUITになる 
		if (msg.message == WM_QUIT)
		{
			break;
		}

		/////////////
		// DirextXでのフレーム描画
		auto bbIdx = _swapchain->GetCurrentBackBufferIndex();	//現在の有効なレンダーターゲットのIDを取得

		D3D12_RESOURCE_BARRIER BarrierDesc = {};
		BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		BarrierDesc.Transition.pResource = _backBuffers[bbIdx];
		BarrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		_cmdList->ResourceBarrier(1, &BarrierDesc);

		//　パイプラインの設定
		_cmdList->SetPipelineState(_pipelinestate);

		// レンダーターゲットの設定
		auto rtvH = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
		rtvH.ptr += static_cast<ULONG_PTR>(bbIdx * _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
		_cmdList->OMSetRenderTargets(1, &rtvH, true, nullptr);

		//画面クリア
		float clearColor[] = { 0.125f,0.125f,0.125f,1.0f };//12.5%グレー
		_cmdList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);

		// ビューポート、シザー、ルートシグネチャの設定
		_cmdList->RSSetViewports(1, &viewport);
		_cmdList->RSSetScissorRects(1, &scissorrect);
		_cmdList->SetGraphicsRootSignature(rootsignature);
		// トポロジ
		_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		// 頂点
		_cmdList->IASetVertexBuffers(0, 1, &vbView);
		//　頂点を描画
		_cmdList->DrawInstanced(6, 1, 0, 0);


		BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		_cmdList->ResourceBarrier(1, &BarrierDesc);


		// 命令のクローズ
		_cmdList->Close();


		//  コマンドリストの実行
		ID3D12CommandList* cmdlists[] = { _cmdList };
		_cmdQueue->ExecuteCommandLists(1, cmdlists);

		////フェンスで処理を待つ
		_cmdQueue->Signal(_fence, ++_fenceVal);

		if (_fence->GetCompletedValue() != _fenceVal) {
			auto event = CreateEvent(nullptr, false, false, nullptr);
			_fence->SetEventOnCompletion(_fenceVal, event);
			WaitForSingleObject(event, INFINITE);
			CloseHandle(event);
		}

		_cmdAllocator->Reset(); //  キューをクリア
		_cmdList->Reset(_cmdAllocator, nullptr); //  再びコマンドリストをためる準備
		// フリップ
		_swapchain->Present(1, 0);
		/*
		if (result != S_OK) {
			DebugOutputFormatString("FAILED ID3D12CommandAllocator::Reset");
			return -1;
		}
		*/

	}
	//もうクラスは使わないので登録解除する
	UnregisterClass(w.lpszClassName, w.hInstance);
	char c = getchar();
	return 0;
}