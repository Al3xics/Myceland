// Copyright 2026 matiasgql. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Runtime/Launch/Resources/Version.h"
#include "GlobalShader.h"
#include "RenderGraphUtils.h"
#include "SceneRendering.h"
#include "SceneView.h"
#include "ShaderParameterStruct.h"

#if ENGINE_MINOR_VERSION >= 2 && ENGINE_MINOR_VERSION <= 4
#include "DataDrivenShaderPlatformInfo.h"
#endif

// Substrate is only available from UE 5.5+.
// On 5.4 and below, we always use the plain GBuffer path.
#define HBAOPLUS_SUBSTRATE_SUPPORTED (ENGINE_MINOR_VERSION >= 5)

#if ENGINE_MINOR_VERSION >= 3
#define HBAOPLUS_SCENE_UNIFORM_PARAM SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneUniformParameters, Scene)
#else
#define HBAOPLUS_SCENE_UNIFORM_PARAM
#endif

#define HBAO_PLUS_SCENE_PARAMS \
	SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureShaderParameters, SceneTextures) \
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View) \
	HBAOPLUS_SCENE_UNIFORM_PARAM \
	SHADER_PARAMETER(FIntPoint, HBAO_OutputExtent) \
	SHADER_PARAMETER(FIntPoint, HBAO_ViewRectMin) \
	SHADER_PARAMETER(FIntPoint, HBAO_ViewRectSize) \
	SHADER_PARAMETER(float, HBAO_Radius) \
	SHADER_PARAMETER(float, HBAO_Intensity) \
	SHADER_PARAMETER(float, HBAO_Bias) \
	SHADER_PARAMETER(float, HBAO_BlurSharpness) \
	SHADER_PARAMETER(float, HBAO_NormalSharpness) \
	SHADER_PARAMETER(float, HBAO_DistanceFadeStart) \
	SHADER_PARAMETER(float, HBAO_DistanceFadeEnd) \
	SHADER_PARAMETER(float, HBAO_MaxPixelRadius) \
	SHADER_PARAMETER(float, HBAO_MipFactor) \
	SHADER_PARAMETER(int32, HBAO_Quality) \
	SHADER_PARAMETER(int32, HBAO_DebugMode) \
	SHADER_PARAMETER(int32, HBAO_HalfRes) \
	SHADER_PARAMETER(int32, HBAO_TemporalEnabled) \
	SHADER_PARAMETER(int32, HBAO_FrameIndex) \
	SHADER_PARAMETER(FVector2f, HBAO_BlurDirection)

class FHBAOPlusCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FHBAOPlusCS);
	SHADER_USE_PARAMETER_STRUCT(FHBAOPlusCS, FGlobalShader);

	class FSubstrateEnabled : SHADER_PERMUTATION_BOOL("SUBSTRATE_ENABLED");
	using FPermutationDomain = TShaderPermutationDomain<FSubstrateEnabled>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
#if !HBAOPLUS_SUBSTRATE_SUPPORTED
		if (PermutationVector.Get<FSubstrateEnabled>())
		{
			return false;
		}
#endif
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		HBAO_PLUS_SCENE_PARAMS
#if HBAOPLUS_SUBSTRATE_SUPPORTED
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSubstrateGlobalUniformParameters, Substrate)
#endif
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, HBAO_Output)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, HBAO_HZBTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, HBAO_HZBSampler)
		SHADER_PARAMETER(FVector4f, HBAO_HZBUvFactorAndInvFactor)
		SHADER_PARAMETER(uint32, HBAO_bIsHZBValid)
	END_SHADER_PARAMETER_STRUCT()
};

class FHBAOPlusBlurCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FHBAOPlusBlurCS);
	SHADER_USE_PARAMETER_STRUCT(FHBAOPlusBlurCS, FGlobalShader);

	class FSubstrateEnabled : SHADER_PERMUTATION_BOOL("SUBSTRATE_ENABLED");
	class FBlurDirectionX   : SHADER_PERMUTATION_BOOL("BLUR_DIRECTION_X");
	using FPermutationDomain = TShaderPermutationDomain<FSubstrateEnabled, FBlurDirectionX>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
#if !HBAOPLUS_SUBSTRATE_SUPPORTED
		if (PermutationVector.Get<FSubstrateEnabled>())
		{
			return false;
		}
#endif
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		HBAO_PLUS_SCENE_PARAMS
#if HBAOPLUS_SUBSTRATE_SUPPORTED
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSubstrateGlobalUniformParameters, Substrate)
#endif
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, HBAO_Input)
		SHADER_PARAMETER_SAMPLER(SamplerState, HBAO_InputSampler)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, HBAO_Output)
	END_SHADER_PARAMETER_STRUCT()
};

class FHBAOPlusCompositePS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FHBAOPlusCompositePS);
	SHADER_USE_PARAMETER_STRUCT(FHBAOPlusCompositePS, FGlobalShader);

	class FSubstrateEnabled : SHADER_PERMUTATION_BOOL("SUBSTRATE_ENABLED");
	using FPermutationDomain = TShaderPermutationDomain<FSubstrateEnabled>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
#if !HBAOPLUS_SUBSTRATE_SUPPORTED
		if (PermutationVector.Get<FSubstrateEnabled>())
		{
			return false;
		}
#endif
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		HBAO_PLUS_SCENE_PARAMS
#if HBAOPLUS_SUBSTRATE_SUPPORTED
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSubstrateGlobalUniformParameters, Substrate)
#endif
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, HBAO_AO)
		SHADER_PARAMETER_SAMPLER(SamplerState, HBAO_AOSampler)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, HBAO_SceneColor)
		SHADER_PARAMETER_SAMPLER(SamplerState, HBAO_SceneColorSampler)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};

class FHBAOPlusTemporalPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FHBAOPlusTemporalPS);
	SHADER_USE_PARAMETER_STRUCT(FHBAOPlusTemporalPS, FGlobalShader);

	class FSubstrateEnabled : SHADER_PERMUTATION_BOOL("SUBSTRATE_ENABLED");
	using FPermutationDomain = TShaderPermutationDomain<FSubstrateEnabled>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
#if !HBAOPLUS_SUBSTRATE_SUPPORTED
		if (PermutationVector.Get<FSubstrateEnabled>())
		{
			return false;
		}
#endif
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		HBAO_PLUS_SCENE_PARAMS
#if HBAOPLUS_SUBSTRATE_SUPPORTED
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSubstrateGlobalUniformParameters, Substrate)
#endif
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, HBAO_CurrentAO)
		SHADER_PARAMETER_SAMPLER(SamplerState, HBAO_CurrentAOSampler)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, HBAO_HistoryAO)
		SHADER_PARAMETER_SAMPLER(SamplerState, HBAO_HistoryAOSampler)
		SHADER_PARAMETER(float, HBAO_TemporalBlend)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};
