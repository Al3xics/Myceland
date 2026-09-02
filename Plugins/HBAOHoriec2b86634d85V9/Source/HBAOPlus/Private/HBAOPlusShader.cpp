// Copyright 2026 matiasgql. All Rights Reserved.
#include "HBAOPlusShader.h"
#include "ShaderParameterUtils.h"
#include "Runtime/Launch/Resources/Version.h"
#include "ShaderCompilerCore.h"

// ── HBAO+ COMPUTE SHADER ────────────────────────────────────────────────────────
void FHBAOPlusCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("ENGINE_MAJOR_VERSION"), ENGINE_MAJOR_VERSION);
	OutEnvironment.SetDefine(TEXT("ENGINE_MINOR_VERSION"), ENGINE_MINOR_VERSION);
	FPermutationDomain PermutationVector(Parameters.PermutationId);
	OutEnvironment.SetDefine(TEXT("SUBSTRATE_ENABLED"), PermutationVector.Get<FSubstrateEnabled>() ? 1 : 0);
	OutEnvironment.SetDefine(TEXT("HBAO_PLUS_COMPUTE"), 1);
	OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_X"), 8);
	OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_Y"), 8);
}

// ── HBAO+ BLUR COMPUTE SHADER ─────────────────────────────────────────────────────
void FHBAOPlusBlurCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("ENGINE_MAJOR_VERSION"), ENGINE_MAJOR_VERSION);
	OutEnvironment.SetDefine(TEXT("ENGINE_MINOR_VERSION"), ENGINE_MINOR_VERSION);
	OutEnvironment.SetDefine(TEXT("BLUR_GROUP_SIZE"), 64);
	OutEnvironment.SetDefine(TEXT("BLUR_RADIUS"), 4);
	OutEnvironment.SetDefine(TEXT("HBAO_PLUS_BLUR_COMPUTE"), 1);
}

// ── HBAO+ COMPOSITE PIXEL SHADER ────────────────────────────────────────────────
void FHBAOPlusCompositePS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("ENGINE_MAJOR_VERSION"), ENGINE_MAJOR_VERSION);
	OutEnvironment.SetDefine(TEXT("ENGINE_MINOR_VERSION"), ENGINE_MINOR_VERSION);
	FPermutationDomain PermutationVector(Parameters.PermutationId);
	OutEnvironment.SetDefine(TEXT("SUBSTRATE_ENABLED"), PermutationVector.Get<FSubstrateEnabled>() ? 1 : 0);
	OutEnvironment.SetDefine(TEXT("HBAO_PLUS_COMPOSITE"), 1);
}

// ── HBAO+ TEMPORAL ACCUMULATION PIXEL SHADER ───────────────────────────────────────────────────
void FHBAOPlusTemporalPS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("ENGINE_MAJOR_VERSION"), ENGINE_MAJOR_VERSION);
	OutEnvironment.SetDefine(TEXT("ENGINE_MINOR_VERSION"), ENGINE_MINOR_VERSION);
	FPermutationDomain PermutationVector(Parameters.PermutationId);
	OutEnvironment.SetDefine(TEXT("SUBSTRATE_ENABLED"), PermutationVector.Get<FSubstrateEnabled>() ? 1 : 0);
	OutEnvironment.SetDefine(TEXT("HBAO_PLUS_TEMPORAL"), 1);
}

IMPLEMENT_GLOBAL_SHADER(FHBAOPlusCS, "/Plugin/HBAOPlus/Private/HBAOPlus.usf", "HBAOPlusMainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FHBAOPlusBlurCS, "/Plugin/HBAOPlus/Private/HBAOPlus.usf", "HBAOBlurCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FHBAOPlusCompositePS, "/Plugin/HBAOPlus/Private/HBAOPlus.usf", "HBAOPlusCompositePS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FHBAOPlusTemporalPS, "/Plugin/HBAOPlus/Private/HBAOPlus.usf", "HBAOPlusTemporalPS", SF_Pixel);
