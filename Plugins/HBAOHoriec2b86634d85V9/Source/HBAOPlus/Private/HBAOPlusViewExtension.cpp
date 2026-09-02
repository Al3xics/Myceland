// Copyright 2026 matiasgql. All Rights Reserved.
#include "HBAOPlusViewExtension.h"
#include "HBAOPlusShader.h"
#include "HAL/IConsoleManager.h"
#if ENGINE_MINOR_VERSION >= 4
#include "PostProcess/PostProcessMaterialInputs.h"
#else
enum class EPostProcessMaterialInput : uint32
{
	SceneColor = 0,
	SeparateTranslucency = 1,
	CombinedBloom = 2,
	PreTonemapHDRColor = 2,
	PostTonemapHDRColor = 3,
	Velocity = 4
};

struct FPostProcessMaterialInputs
{
	FScreenPassRenderTarget OverrideOutput;
	TStaticArray<FScreenPassTexture, 5> Textures;

	inline FScreenPassTexture GetInput(EPostProcessMaterialInput Input) const
	{
		return Textures[(uint32)Input];
	}
};
#endif
#include "RenderGraphUtils.h"
#include "SceneFilterRendering.h"
#include "ScenePrivate.h"
#include "SceneTextureParameters.h"
#include "SceneViewExtension.h"
#include "ScreenPass.h"
#include "SystemTextures.h"
#include "PixelShaderUtils.h"
#include "ClearQuad.h"

DECLARE_GPU_STAT_NAMED(HBAOPlus, TEXT("HBAO+ Total"));
DECLARE_GPU_STAT_NAMED(HBAOPlusBlur, TEXT("HBAO+ Blur"));

static TSharedPtr<FHBAOPlusViewExtension, ESPMode::ThreadSafe> GHBAOPlusViewExtension;

static TAutoConsoleVariable<int32> CVarHBAOPlusEnabled(
	TEXT("r.HBAOPlus.Enabled"),
	1,
	TEXT("Enable or disable HBAO+.\n")
	TEXT("0: Disabled\n")
	TEXT("1: Enabled"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarHBAOPlusMode(
	TEXT("r.HBAOPlus.Mode"),
	0,
	TEXT("HBAO+ Execution Mode.\n")
	TEXT("0: Post Process Pass, Composited on SceneColor\n")
	TEXT("1: Replace ScreenSpaceAO buffer during Deferred Base Pass"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarHBAOPlusRadius(
	TEXT("r.HBAOPlus.Radius"),
	100.0f,
	TEXT("HBAO+ Radius in world units."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarHBAOPlusBias(
	TEXT("r.HBAOPlus.Bias"),
	0.1f,
	TEXT("HBAO+ Bias."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarHBAOPlusIntensity(
	TEXT("r.HBAOPlus.Intensity"),
	1.5f,
	TEXT("HBAO+ Intensity."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarHBAOPlusQuality(
	TEXT("r.HBAOPlus.Quality"),
	2,
	TEXT("HBAO+ Quality (0-3)."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarHBAOPlusBlurEnabled(
	TEXT("r.HBAOPlus.BlurEnabled"),
	1,
	TEXT("HBAO+ Bilateral Blur.\n")
	TEXT("0: Disabled (raw AO, m\xC3\xA1s barato)\n")
	TEXT("1: Enabled"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarHBAOPlusBlurSharpness(
	TEXT("r.HBAOPlus.BlurSharpness"),
	16.0f,
	TEXT("HBAO+ Blur Depth Sharpness."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarHBAOPlusNormalSharpness(
	TEXT("r.HBAOPlus.NormalSharpness"),
	8.0f,
	TEXT("HBAO+ Blur Normal Sharpness."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarHBAOPlusMaxPixelRadius(
	TEXT("r.HBAOPlus.MaxPixelRadius"),
	100.0f,
	TEXT("HBAO+ Maximum pixel radius to prevent over-sampling."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarHBAOPlusHalfRes(
	TEXT("r.HBAOPlus.HalfRes"),
	0,
	TEXT("HBAO+ Half Resolution.\n")
	TEXT("0: Full Res\n")
	TEXT("1: Half Res"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarHBAOPlusDistanceFadeStart(
	TEXT("r.HBAOPlus.DistanceFadeStart"),
	15000.0f,
	TEXT("HBAO+ Distance Fade Start."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarHBAOPlusDistanceFadeEnd(
	TEXT("r.HBAOPlus.DistanceFadeEnd"),
	30000.0f,
	TEXT("HBAO+ Distance Fade End."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarHBAOPlusDebugMode(
	TEXT("r.HBAOPlus.DebugMode"),
	0,
	TEXT("HBAO+ Debug Mode.\n")
	TEXT("0: Off\n")
	TEXT("1: AO Only\n"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarHBAOPlusTemporalAccumulation(
	TEXT("r.HBAOPlus.TemporalAccumulation"),
	1,
	TEXT("Enable Temporal Accumulation and Jittering for HBAO+ (Only active when TAA or TSR is on).\n")
	TEXT("0: Disabled\n")
	TEXT("1: Enabled"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarHBAOPlusTemporalBlend(
	TEXT("r.HBAOPlus.TemporalBlend"),
	0.95f,
	TEXT("Temporal Blend Weight (0..1) - Higher means more history accumulation"),
	ECVF_RenderThreadSafe);

FHBAOPlusViewExtension::FHBAOPlusViewExtension(const FAutoRegister& AutoRegister)
	: FSceneViewExtensionBase(AutoRegister)
{
	auto UpdateAOCvars = [](IConsoleVariable* Var)
	{
		IConsoleVariable* AOLevelsCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.AmbientOcclusionLevels"));
		IConsoleVariable* ShortRangeAOCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.ScreenProbeGather.ShortRangeAO"));
		IConsoleVariable* DiffuseIndirectSSAOCvar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.DiffuseIndirect.SSAO"));

		const bool bIsActive = (CVarHBAOPlusEnabled.GetValueOnAnyThread() > 0 && CVarHBAOPlusMode.GetValueOnAnyThread() == 1);

		if (AOLevelsCVar)
		{
			// Disable built-in SSAO only if the plugin is Enabled AND in Replace mode (Mode 1)
			AOLevelsCVar->Set(bIsActive ? 0 : -1, ECVF_SetByCode);
		}

		if (ShortRangeAOCVar)
		{
			ShortRangeAOCVar->Set(bIsActive ? 0 : 1, ECVF_SetByCode);
		}

		if (DiffuseIndirectSSAOCvar)
		{
			DiffuseIndirectSSAOCvar->Set(bIsActive ? 1 : 0, ECVF_SetByCode);
		}
	};

	CVarHBAOPlusEnabled.AsVariable()->SetOnChangedCallback(FConsoleVariableDelegate::CreateLambda(UpdateAOCvars));
	CVarHBAOPlusMode.AsVariable()->SetOnChangedCallback(FConsoleVariableDelegate::CreateLambda(UpdateAOCvars));
}

FHBAOPlusViewExtension::~FHBAOPlusViewExtension()
{
}

bool FHBAOPlusViewExtension::IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const
{
	return CVarHBAOPlusEnabled.GetValueOnAnyThread() > 0;
}

void FHBAOPlusViewExtension::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
	if (CVarHBAOPlusEnabled.GetValueOnAnyThread() > 0 && CVarHBAOPlusMode.GetValueOnAnyThread() == 1)
	{
		IConsoleVariable* AOMethodCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.AmbientOcclusion.Method"));
		if (AOMethodCVar)
		{
			int32 CurrentAOMethod = AOMethodCVar->GetInt();
			if (CurrentAOMethod != 0)
			{
				PreviousAOMethod = CurrentAOMethod;
				AOMethodCVar->Set(0, ECVF_SetByCode);
			}
		}

		IConsoleVariable* AOMipLevelFactorCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.AmbientOcclusionMipLevelFactor"));
		if (AOMipLevelFactorCVar)
		{
			float CurrentAOMipLevelFactor = AOMipLevelFactorCVar->GetFloat();
			if (CurrentAOMipLevelFactor != 0.0f)
			{
				PreviousAOMipLevelFactor = CurrentAOMipLevelFactor;
				AOMipLevelFactorCVar->Set(0.0f, ECVF_SetByCode);
			}
		}

	}
	else
	{
		if (PreviousAOMethod != -1)
		{
			IConsoleVariable* AOMethodCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.AmbientOcclusion.Method"));
			if (AOMethodCVar) AOMethodCVar->Set(PreviousAOMethod, ECVF_SetByCode);
			PreviousAOMethod = -1;
		}

		if (PreviousAOMipLevelFactor >= 0.0f)
		{
			IConsoleVariable* AOMipLevelFactorCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.AmbientOcclusionMipLevelFactor"));
			if (AOMipLevelFactorCVar) AOMipLevelFactorCVar->Set(PreviousAOMipLevelFactor, ECVF_SetByCode);
			PreviousAOMipLevelFactor = -1.0f;
		}

	}
}

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
void FHBAOPlusViewExtension::SubscribeToPostProcessingPass(ISceneViewExtension::EPostProcessingPass Pass, const FSceneView& InView, FAfterPassCallbackDelegateArray& InOutPassCallbacks, bool bIsPassEnabled)
{
#if ENGINE_MINOR_VERSION >= 6
    if (Pass == ISceneViewExtension::EPostProcessingPass::BeforeDOF)
#else
    if (Pass == ISceneViewExtension::EPostProcessingPass::MotionBlur)
#endif
    {
        InOutPassCallbacks.Add(FAfterPassCallbackDelegate::CreateLambda(
            [this](FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& InInputs) -> FScreenPassTexture
            {
                if (CVarHBAOPlusMode.GetValueOnRenderThread() != 0)
                {
                    return FScreenPassTexture(InInputs.GetInput(EPostProcessMaterialInput::SceneColor));
                }
                return RenderHBAOPlus_RenderThread(GraphBuilder, View, InInputs);
            }));
    }
}
#else
void FHBAOPlusViewExtension::SubscribeToPostProcessingPass(EPostProcessingPass Pass, FAfterPassCallbackDelegateArray& InOutPassCallbacks, bool bIsPassEnabled)
{
    if (Pass == EPostProcessingPass::MotionBlur)
    {
        InOutPassCallbacks.Add(FAfterPassCallbackDelegate::CreateLambda(
            [this](FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& InInputs) -> FScreenPassTexture
            {
#if ENGINE_MINOR_VERSION == 0
                return RenderHBAOPlus_RenderThread(GraphBuilder, View, InInputs);
#else
                if (CVarHBAOPlusMode.GetValueOnRenderThread() != 0)
                {
                    return FScreenPassTexture(InInputs.GetInput(EPostProcessMaterialInput::SceneColor));
                }
                return RenderHBAOPlus_RenderThread(GraphBuilder, View, InInputs);
#endif
            }));
    }
}
#endif

#if ENGINE_MINOR_VERSION >= 1
void FHBAOPlusViewExtension::PostRenderBasePassDeferred_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView, const FRenderTargetBindingSlots& RenderTargets, TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTextures)
{
	if (CVarHBAOPlusEnabled.GetValueOnRenderThread() == 0 || CVarHBAOPlusMode.GetValueOnRenderThread() != 1)
	{
		return;
	}

	const FViewInfo& ViewInfo = static_cast<const FViewInfo&>(InView);
	if (ViewInfo.bIsSceneCapture || ViewInfo.bIsReflectionCapture)
	{
		return;
	}

#if ENGINE_MINOR_VERSION >= 3
		const FSceneTextureShaderParameters SceneTextureParams = CreateSceneTextureShaderParameters(GraphBuilder, ViewInfo);
#else
		const FSceneTextureShaderParameters SceneTextureParams = CreateSceneTextureShaderParameters(GraphBuilder, ViewInfo.GetSceneTexturesChecked(), ViewInfo.GetFeatureLevel(), ESceneTextureSetupMode::All);
#endif
	FRDGTextureRef FinalAOTexture = RenderHBAO_Internal_RenderThread(GraphBuilder, ViewInfo, SceneTextureParams);
	UE_LOG(LogTemp, Warning, TEXT("HBAO MODE 1: AO GENERATED"));
	if (FinalAOTexture)
	{UE_LOG(LogTemp, Warning, TEXT("HBAO MODE 1: WRITING SCREEN SPACE AO"));
		const FSceneTextures* SceneTexturesPtr = ViewInfo.GetSceneTexturesChecked();
		if (SceneTexturesPtr)
		{
			FSceneTextures& SceneTexturesMut = const_cast<FSceneTextures&>(*SceneTexturesPtr);
			if (!SceneTexturesMut.ScreenSpaceAO)
			{
				// ScreenSpaceAO must match the full render target extent (same as depth/gbuffer),
				// NOT just the ViewRect — otherwise the engine reads garbage outside the viewport.
				const FIntPoint FullExtent = SceneTexturesMut.Depth.Target
					? SceneTexturesMut.Depth.Target->Desc.Extent
					: FinalAOTexture->Desc.Extent;

				FRDGTextureDesc Desc = FRDGTextureDesc::Create2D(
					FullExtent,
					FinalAOTexture->Desc.Format,
					FClearValueBinding::White,
					TexCreate_RenderTargetable | TexCreate_ShaderResource);
				SceneTexturesMut.ScreenSpaceAO = GraphBuilder.CreateTexture(Desc, TEXT("ScreenSpaceAO"));
			}

#if ENGINE_MINOR_VERSION >= 4
		AddDrawTexturePass(GraphBuilder, ViewInfo,
			FinalAOTexture, SceneTexturesMut.ScreenSpaceAO,
			FIntPoint::ZeroValue, FinalAOTexture->Desc.Extent, // Input Pos & Size
			ViewInfo.ViewRect.Min, ViewInfo.ViewRect.Size());  // Output Pos & Size
#else
		FScreenPassTexture InputTex(FinalAOTexture);
		FScreenPassRenderTarget OutputTex(SceneTexturesMut.ScreenSpaceAO, ViewInfo.ViewRect, ERenderTargetLoadAction::ELoad);
		AddDrawTexturePass(GraphBuilder, ViewInfo, InputTex, OutputTex);
#endif
		}
	}
}
#endif

FRDGTextureRef FHBAOPlusViewExtension::RenderHBAO_Internal_RenderThread(FRDGBuilder& GraphBuilder, const FViewInfo& ViewInfo, const FSceneTextureShaderParameters& SceneTextureParams)
{
#if ENGINE_MINOR_VERSION >= 8
	RDG_EVENT_SCOPE_STAT(GraphBuilder, HBAOPlus, "HBAO+");
#else
	RDG_GPU_STAT_SCOPE(GraphBuilder, HBAOPlus);
	RDG_EVENT_SCOPE(GraphBuilder, "HBAO+");
#endif
	FRDGTextureRef TopLayerTexture = nullptr;
#if HBAOPLUS_SUBSTRATE_SUPPORTED
	if (ViewInfo.SubstrateViewData.SceneData)
	{
		TopLayerTexture = ViewInfo.SubstrateViewData.SceneData->TopLayerTexture;
	}
#endif
	if (!TopLayerTexture) TopLayerTexture = GraphBuilder.RegisterExternalTexture(GSystemTextures.BlackDummy);

	FIntPoint ViewSize = ViewInfo.ViewRect.Size();
	const bool bHalfRes = CVarHBAOPlusHalfRes.GetValueOnRenderThread() > 0;
	const bool bTemporalRequested = CVarHBAOPlusTemporalAccumulation.GetValueOnRenderThread() > 0;
	const bool bTemporalActive = bTemporalRequested && (ViewInfo.AntiAliasingMethod == AAM_TSR || ViewInfo.AntiAliasingMethod == AAM_TemporalAA);
	
	FIntPoint AOBufferSize = bHalfRes 
		? FIntPoint(FMath::DivideAndRoundUp(ViewSize.X, 2), FMath::DivideAndRoundUp(ViewSize.Y, 2))
		: ViewSize;

	// Remove FastVRAM on PC as it can cause allocation stalls and frame spikes
	ETextureCreateFlags TempTexFlags = TexCreate_UAV | TexCreate_ShaderResource | TexCreate_RenderTargetable;

	// 1. MAIN HBAO PASS (Compute)
	FRDGTextureDesc AODesc = FRDGTextureDesc::Create2D(AOBufferSize, PF_R16F, FClearValueBinding::White, TempTexFlags);
	FRDGTextureRef AOTexture = GraphBuilder.CreateTexture(AODesc, TEXT("HBAO_AO"));

	{
		FHBAOPlusCS::FParameters* PassParams = GraphBuilder.AllocParameters<FHBAOPlusCS::FParameters>();
		
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(AOTexture), 1.0f);

		PassParams->HBAO_Output = GraphBuilder.CreateUAV(AOTexture);
		PassParams->HBAO_Radius = CVarHBAOPlusRadius.GetValueOnRenderThread();
		PassParams->HBAO_Bias = CVarHBAOPlusBias.GetValueOnRenderThread();
		PassParams->HBAO_Intensity = CVarHBAOPlusIntensity.GetValueOnRenderThread();
		PassParams->HBAO_Quality = CVarHBAOPlusQuality.GetValueOnRenderThread();
		PassParams->HBAO_MaxPixelRadius = CVarHBAOPlusMaxPixelRadius.GetValueOnRenderThread();
		PassParams->HBAO_MipFactor = CVarHBAOPlusMaxPixelRadius.GetValueOnRenderThread() / 4.0f;
		PassParams->HBAO_DistanceFadeStart = CVarHBAOPlusDistanceFadeStart.GetValueOnRenderThread();
		PassParams->HBAO_DistanceFadeEnd = CVarHBAOPlusDistanceFadeEnd.GetValueOnRenderThread();
		PassParams->HBAO_DebugMode = CVarHBAOPlusDebugMode.GetValueOnRenderThread();
		PassParams->HBAO_HalfRes = bHalfRes ? 1 : 0;
		PassParams->HBAO_TemporalEnabled = bTemporalActive ? 1 : 0;
		PassParams->HBAO_FrameIndex = ViewInfo.Family->FrameNumber;
		PassParams->HBAO_ViewRectMin = ViewInfo.ViewRect.Min;
		PassParams->HBAO_ViewRectSize = ViewInfo.ViewRect.Size();
		PassParams->View = ViewInfo.ViewUniformBuffer;
		PassParams->SceneTextures = SceneTextureParams;
#if HBAOPLUS_SUBSTRATE_SUPPORTED
		PassParams->Substrate = ViewInfo.SubstrateViewData.SubstrateGlobalUniformParameters;
#endif

		// HZB far-tap sampling parameters (Use ClosestHZB for conservative foreground occlusion, fallback to HZB if unavailable)
		FRDGTextureRef HBAO_HZBTextureLocal = ViewInfo.ClosestHZB ? ViewInfo.ClosestHZB : ViewInfo.HZB;
		PassParams->HBAO_bIsHZBValid = HBAO_HZBTextureLocal ? 1u : 0u;
		if (!HBAO_HZBTextureLocal)
		{
			HBAO_HZBTextureLocal = GraphBuilder.RegisterExternalTexture(GSystemTextures.BlackDummy);
		}
		// The HZB texture is allocated from the RDG pool and its physical extent might be padded.
		// We MUST use the logical HZBMipmap0Size for accurate UV scaling, otherwise we get resize artifacts.
		FIntPoint HBAO_HZBRes = ViewInfo.ClosestHZB || ViewInfo.HZB ? ViewInfo.HZBMipmap0Size : HBAO_HZBTextureLocal->Desc.Extent;

		FVector2D HBAO_HZBUvFactor(
			float(ViewInfo.ViewRect.Width())  / float(2 * HBAO_HZBRes.X),
			float(ViewInfo.ViewRect.Height()) / float(2 * HBAO_HZBRes.Y)
		);

		PassParams->HBAO_HZBTexture = HBAO_HZBTextureLocal;
		PassParams->HBAO_HZBSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		PassParams->HBAO_HZBUvFactorAndInvFactor = FVector4f(
			HBAO_HZBUvFactor.X, HBAO_HZBUvFactor.Y,
			1.0f / HBAO_HZBUvFactor.X, 1.0f / HBAO_HZBUvFactor.Y
		);

		FHBAOPlusCS::FPermutationDomain PermutationVector;
#if HBAOPLUS_SUBSTRATE_SUPPORTED
		PermutationVector.Set<FHBAOPlusCS::FSubstrateEnabled>(ViewInfo.SubstrateViewData.SubstrateGlobalUniformParameters != nullptr);
#else
		PermutationVector.Set<FHBAOPlusCS::FSubstrateEnabled>(false);
#endif

		TShaderMapRef<FHBAOPlusCS> ComputeShader(ViewInfo.ShaderMap, PermutationVector);
		FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("HBAO+ Main"), ComputeShader, PassParams, 
			FComputeShaderUtils::GetGroupCount(AOBufferSize, FIntPoint(8, 8)));
	}

	// 2. BILATERAL BLUR PASS (Compute)
	FRDGTextureRef BlurredAOTextureX = nullptr;
	FRDGTextureRef BlurredAOTextureFinal = nullptr;
	const bool bBlurEnabled = CVarHBAOPlusBlurEnabled.GetValueOnRenderThread() > 0;

	if (bBlurEnabled)
	{
		BlurredAOTextureX     = GraphBuilder.CreateTexture(AODesc, TEXT("HBAOPlus_BlurredAO_X"));
		BlurredAOTextureFinal = GraphBuilder.CreateTexture(AODesc, TEXT("HBAOPlus_BlurredAO_Final"));
#if HBAOPLUS_SUBSTRATE_SUPPORTED
		const bool bSubstrateEnabled = ViewInfo.SubstrateViewData.SubstrateGlobalUniformParameters != nullptr;
#else
		const bool bSubstrateEnabled = false;
#endif

#if ENGINE_MINOR_VERSION >= 8
	    RDG_EVENT_SCOPE_STAT(GraphBuilder, HBAOPlusBlur, "HBAO+ Blur");
#else
	    RDG_GPU_STAT_SCOPE(GraphBuilder, HBAOPlusBlur);
	    RDG_EVENT_SCOPE(GraphBuilder, "HBAO+ Blur");
#endif

	    // X Pass
	    {
	        FHBAOPlusBlurCS::FPermutationDomain PermX;
#if HBAOPLUS_SUBSTRATE_SUPPORTED
	        PermX.Set<FHBAOPlusBlurCS::FSubstrateEnabled>(bSubstrateEnabled);
#else
	        PermX.Set<FHBAOPlusBlurCS::FSubstrateEnabled>(false);
#endif
	        PermX.Set<FHBAOPlusBlurCS::FBlurDirectionX>(true);
	        TShaderMapRef<FHBAOPlusBlurCS> BlurCSX(ViewInfo.ShaderMap, PermX);

	        FHBAOPlusBlurCS::FParameters* PassParams = GraphBuilder.AllocParameters<FHBAOPlusBlurCS::FParameters>();
	        PassParams->HBAO_Input          = AOTexture;
	        PassParams->HBAO_InputSampler   = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	        PassParams->HBAO_Output         = GraphBuilder.CreateUAV(BlurredAOTextureX);
	        PassParams->HBAO_BlurDirection  = FVector2f(1.0f, 0.0f);
	        PassParams->HBAO_OutputExtent   = AOBufferSize;
	        PassParams->HBAO_ViewRectMin    = ViewInfo.ViewRect.Min;
	        PassParams->HBAO_ViewRectSize   = ViewInfo.ViewRect.Size();
	        PassParams->HBAO_BlurSharpness  = CVarHBAOPlusBlurSharpness.GetValueOnRenderThread();
	        PassParams->HBAO_NormalSharpness= CVarHBAOPlusNormalSharpness.GetValueOnRenderThread();
	        PassParams->HBAO_DistanceFadeStart = CVarHBAOPlusDistanceFadeStart.GetValueOnRenderThread();
	        PassParams->HBAO_DistanceFadeEnd   = CVarHBAOPlusDistanceFadeEnd.GetValueOnRenderThread();
	        PassParams->HBAO_DebugMode      = CVarHBAOPlusDebugMode.GetValueOnRenderThread();
	        PassParams->HBAO_HalfRes        = bHalfRes ? 1 : 0;
	        PassParams->HBAO_Radius         = CVarHBAOPlusRadius.GetValueOnRenderThread();
	        PassParams->View                  = ViewInfo.ViewUniformBuffer;
	        PassParams->SceneTextures         = SceneTextureParams;
#if HBAOPLUS_SUBSTRATE_SUPPORTED
	        PassParams->Substrate             = ViewInfo.SubstrateViewData.SubstrateGlobalUniformParameters;
#endif
	        PassParams->HBAO_Intensity      = 0.0f;
	        PassParams->HBAO_Bias           = 0.0f;
	        PassParams->HBAO_MaxPixelRadius = 0.0f;
	        PassParams->HBAO_Quality        = 0;
	        PassParams->HBAO_TemporalEnabled= 0;
	        PassParams->HBAO_FrameIndex     = 0;

	        FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("HBAO+ Blur X"),
	            BlurCSX, PassParams,
	            FIntVector(FMath::DivideAndRoundUp(AOBufferSize.X, 64), AOBufferSize.Y, 1));
	    }

	    // Y Pass
	    {
	        FHBAOPlusBlurCS::FPermutationDomain PermY;
#if HBAOPLUS_SUBSTRATE_SUPPORTED
	        PermY.Set<FHBAOPlusBlurCS::FSubstrateEnabled>(bSubstrateEnabled);
#else
	        PermY.Set<FHBAOPlusBlurCS::FSubstrateEnabled>(false);
#endif
	        PermY.Set<FHBAOPlusBlurCS::FBlurDirectionX>(false);
	        TShaderMapRef<FHBAOPlusBlurCS> BlurCSY(ViewInfo.ShaderMap, PermY);

	        FHBAOPlusBlurCS::FParameters* PassParams = GraphBuilder.AllocParameters<FHBAOPlusBlurCS::FParameters>();
	        PassParams->HBAO_Input          = BlurredAOTextureX;
	        PassParams->HBAO_InputSampler   = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	        PassParams->HBAO_Output         = GraphBuilder.CreateUAV(BlurredAOTextureFinal);
	        PassParams->HBAO_BlurDirection  = FVector2f(0.0f, 1.0f);
	        PassParams->HBAO_OutputExtent   = AOBufferSize;
	        PassParams->HBAO_ViewRectMin    = ViewInfo.ViewRect.Min;
	        PassParams->HBAO_ViewRectSize   = ViewInfo.ViewRect.Size();
	        PassParams->HBAO_BlurSharpness  = CVarHBAOPlusBlurSharpness.GetValueOnRenderThread();
	        PassParams->HBAO_NormalSharpness= CVarHBAOPlusNormalSharpness.GetValueOnRenderThread();
	        PassParams->HBAO_DistanceFadeStart = CVarHBAOPlusDistanceFadeStart.GetValueOnRenderThread();
	        PassParams->HBAO_DistanceFadeEnd   = CVarHBAOPlusDistanceFadeEnd.GetValueOnRenderThread();
	        PassParams->HBAO_DebugMode      = CVarHBAOPlusDebugMode.GetValueOnRenderThread();
	        PassParams->HBAO_HalfRes        = bHalfRes ? 1 : 0;
	        PassParams->HBAO_Radius         = CVarHBAOPlusRadius.GetValueOnRenderThread();
	        PassParams->View                  = ViewInfo.ViewUniformBuffer;
	        PassParams->SceneTextures         = SceneTextureParams;
#if HBAOPLUS_SUBSTRATE_SUPPORTED
	        PassParams->Substrate             = ViewInfo.SubstrateViewData.SubstrateGlobalUniformParameters;
#endif
	        PassParams->HBAO_Intensity      = 0.0f;
	        PassParams->HBAO_Bias           = 0.0f;
	        PassParams->HBAO_MaxPixelRadius = 0.0f;
	        PassParams->HBAO_Quality        = 0;
	        PassParams->HBAO_TemporalEnabled= 0;
	        PassParams->HBAO_FrameIndex     = 0;

	        FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("HBAO+ Blur Y"),
	            BlurCSY, PassParams,
	            FIntVector(AOBufferSize.X, FMath::DivideAndRoundUp(AOBufferSize.Y, 64), 1));
	    }
	}

	// 2.5 TEMPORAL ACCUMULATION
	FIntRect AORect(FIntPoint::ZeroValue, AOBufferSize);
	FRDGTextureRef TemporalAOTexture = bBlurEnabled ? BlurredAOTextureFinal : AOTexture;
	if (bTemporalActive && ViewInfo.State)
	{
		TRefCountPtr<IPooledRenderTarget>& HistoryRT = HistoryRTs.FindOrAdd(ViewInfo.State);

		FRDGTextureRef HistoryTexture = nullptr;
		if (HistoryRT.IsValid() && HistoryRT->GetDesc().Extent == AOBufferSize)
		{
			HistoryTexture = GraphBuilder.RegisterExternalTexture(HistoryRT);
		}
		else
		{
			// Initialize history to white
			HistoryTexture = GraphBuilder.RegisterExternalTexture(GSystemTextures.WhiteDummy);
		}

		FRDGTextureRef NewHistoryTexture = GraphBuilder.CreateTexture(AODesc, TEXT("HBAOPlus_TemporalAO"));

		FHBAOPlusTemporalPS::FParameters* TemporalParams = GraphBuilder.AllocParameters<FHBAOPlusTemporalPS::FParameters>();
		TemporalParams->HBAO_CurrentAO = TemporalAOTexture;
		TemporalParams->HBAO_CurrentAOSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		TemporalParams->HBAO_HistoryAO = HistoryTexture;
		TemporalParams->HBAO_HistoryAOSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		TemporalParams->HBAO_TemporalBlend = CVarHBAOPlusTemporalBlend.GetValueOnRenderThread();
		TemporalParams->HBAO_HalfRes = bHalfRes ? 1 : 0;
		TemporalParams->HBAO_ViewRectMin = ViewInfo.ViewRect.Min;
		TemporalParams->HBAO_ViewRectSize = ViewInfo.ViewRect.Size();
		TemporalParams->View = ViewInfo.ViewUniformBuffer;
		TemporalParams->SceneTextures = SceneTextureParams;
#if HBAOPLUS_SUBSTRATE_SUPPORTED
		TemporalParams->Substrate = ViewInfo.SubstrateViewData.SubstrateGlobalUniformParameters;
#endif
		TemporalParams->RenderTargets[0] = FRenderTargetBinding(NewHistoryTexture, ERenderTargetLoadAction::ENoAction);

		FHBAOPlusTemporalPS::FPermutationDomain TemporalPermutationVector;
#if HBAOPLUS_SUBSTRATE_SUPPORTED
		TemporalPermutationVector.Set<FHBAOPlusTemporalPS::FSubstrateEnabled>(ViewInfo.SubstrateViewData.SubstrateGlobalUniformParameters != nullptr);
#else
		TemporalPermutationVector.Set<FHBAOPlusTemporalPS::FSubstrateEnabled>(false);
#endif
		TShaderMapRef<FHBAOPlusTemporalPS> TemporalShader(ViewInfo.ShaderMap, TemporalPermutationVector);

		FScreenPassTextureViewport TemporalViewport(NewHistoryTexture, AORect);
#if ENGINE_MINOR_VERSION >= 5
		AddDrawScreenPass(GraphBuilder, RDG_EVENT_NAME("HBAO+ Temporal Accumulation"), FScreenPassViewInfo(ViewInfo), TemporalViewport, TemporalViewport, TemporalShader, TemporalParams, EScreenPassDrawFlags::None);
#else
		AddDrawScreenPass(GraphBuilder, RDG_EVENT_NAME("HBAO+ Temporal Accumulation"), ViewInfo, TemporalViewport, TemporalViewport, TemporalShader, TemporalParams, EScreenPassDrawFlags::None);
#endif

		GraphBuilder.QueueTextureExtraction(NewHistoryTexture, &HistoryRT);
		TemporalAOTexture = NewHistoryTexture;
	}

	return TemporalAOTexture;
}

FScreenPassTexture FHBAOPlusViewExtension::RenderHBAOPlus_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& Inputs)
{
#if ENGINE_MINOR_VERSION >= 8
	RDG_EVENT_SCOPE_STAT(GraphBuilder, HBAOPlus, "HBAO+");
#else
	RDG_GPU_STAT_SCOPE(GraphBuilder, HBAOPlus);
	RDG_EVENT_SCOPE(GraphBuilder, "HBAO+");
#endif
	
	const FViewInfo& ViewInfo = static_cast<const FViewInfo&>(View);
	FScreenPassTexture SceneColor = FScreenPassTexture(Inputs.GetInput(EPostProcessMaterialInput::SceneColor));
	if (!SceneColor.IsValid()) return FScreenPassTexture();
	
#if ENGINE_MINOR_VERSION >= 3
	const FSceneTextureShaderParameters SceneTextureParams = CreateSceneTextureShaderParameters(GraphBuilder, ViewInfo);
#elif ENGINE_MINOR_VERSION >= 1
	const FSceneTextureShaderParameters SceneTextureParams = CreateSceneTextureShaderParameters(GraphBuilder, ViewInfo.GetSceneTexturesChecked(), ViewInfo.GetFeatureLevel(), ESceneTextureSetupMode::All);
#else
	const FSceneTextureShaderParameters SceneTextureParams = CreateSceneTextureShaderParameters(GraphBuilder, ViewInfo.GetFeatureLevel(), ESceneTextureSetupMode::All);
#endif

	FRDGTextureRef FinalAOTexture = RenderHBAO_Internal_RenderThread(GraphBuilder, ViewInfo, SceneTextureParams);

	// 3. COMPOSITE PASS
	FScreenPassRenderTarget OutputTarget;
	{
		FRDGTextureDesc OutputDesc = SceneColor.Texture->Desc;
		OutputDesc.Flags |= TexCreate_RenderTargetable | TexCreate_ShaderResource;
		// Explicitly bind a clear color: UE 5.5 D3D12RHI asserts EClearBinding::EColorBound
		// when ERenderTargetLoadAction::EClear is used. Copying SceneColor's desc may
		// carry FClearValueBinding::None on some engine versions, causing a crash.
		OutputDesc.ClearValue = FClearValueBinding::Black;
		OutputTarget = FScreenPassRenderTarget(
			GraphBuilder.CreateTexture(OutputDesc, TEXT("HBAOPlus.Output")),
			SceneColor.ViewRect,
			ERenderTargetLoadAction::EClear);
	}

	{
		FHBAOPlusCompositePS::FParameters* PassParams = GraphBuilder.AllocParameters<FHBAOPlusCompositePS::FParameters>();
		PassParams->HBAO_SceneColor = SceneColor.Texture;
		PassParams->HBAO_SceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		PassParams->HBAO_AO = (CVarHBAOPlusDebugMode.GetValueOnRenderThread() == 1) ? FinalAOTexture : FinalAOTexture;
		PassParams->HBAO_AOSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		PassParams->HBAO_DebugMode = CVarHBAOPlusDebugMode.GetValueOnRenderThread();
		PassParams->HBAO_HalfRes = CVarHBAOPlusHalfRes.GetValueOnRenderThread();
		PassParams->HBAO_DistanceFadeStart = CVarHBAOPlusDistanceFadeStart.GetValueOnRenderThread();
		PassParams->HBAO_DistanceFadeEnd = CVarHBAOPlusDistanceFadeEnd.GetValueOnRenderThread();
		PassParams->HBAO_ViewRectMin = SceneColor.ViewRect.Min;
		PassParams->HBAO_ViewRectSize = SceneColor.ViewRect.Size();
		PassParams->View = ViewInfo.ViewUniformBuffer;
		PassParams->SceneTextures = SceneTextureParams;
#if HBAOPLUS_SUBSTRATE_SUPPORTED
		PassParams->Substrate = ViewInfo.SubstrateViewData.SubstrateGlobalUniformParameters;
#endif
		PassParams->RenderTargets[0] = OutputTarget.GetRenderTargetBinding();

		FHBAOPlusCompositePS::FPermutationDomain PermutationVector;
#if HBAOPLUS_SUBSTRATE_SUPPORTED
		PermutationVector.Set<FHBAOPlusCompositePS::FSubstrateEnabled>(ViewInfo.SubstrateViewData.SubstrateGlobalUniformParameters != nullptr);
#else
		PermutationVector.Set<FHBAOPlusCompositePS::FSubstrateEnabled>(false);
#endif
		TShaderMapRef<FHBAOPlusCompositePS> PixelShader(ViewInfo.ShaderMap, PermutationVector);

		FScreenPassTextureViewport CompositeViewport(OutputTarget);
		FScreenPassTextureViewport SceneColorViewport(SceneColor);
#if ENGINE_MINOR_VERSION >= 5
		AddDrawScreenPass(GraphBuilder, RDG_EVENT_NAME("HBAO+ Composite"), FScreenPassViewInfo(ViewInfo), CompositeViewport, SceneColorViewport, PixelShader, PassParams, EScreenPassDrawFlags::None);
#else
		AddDrawScreenPass(GraphBuilder, RDG_EVENT_NAME("HBAO+ Composite"), ViewInfo, CompositeViewport, SceneColorViewport, PixelShader, PassParams, EScreenPassDrawFlags::None);
#endif
	}

	return MoveTemp(OutputTarget);
}

void FHBAOPlusViewExtension::Register()
{
	if (!GHBAOPlusViewExtension.IsValid())
	{
		GHBAOPlusViewExtension = FSceneViewExtensions::NewExtension<FHBAOPlusViewExtension>();
	}
}

void FHBAOPlusViewExtension::Unregister()
{
	GHBAOPlusViewExtension.Reset();
}
