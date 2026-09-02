// Copyright 2026 matiasgql. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "SceneViewExtension.h"
#include "ScreenPass.h"
#include "Runtime/Launch/Resources/Version.h"

struct FPostProcessMaterialInputs;

class FHBAOPlusViewExtension final : public FSceneViewExtensionBase
{
public:
	explicit FHBAOPlusViewExtension(const FAutoRegister& AutoRegister);
	virtual ~FHBAOPlusViewExtension() override;

	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override {}
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {}
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual void PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override {}
	virtual void PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView) override {}

#if ENGINE_MINOR_VERSION >= 1
	virtual void PostRenderBasePassDeferred_RenderThread(
		FRDGBuilder& GraphBuilder,
		FSceneView& InView,
		const FRenderTargetBindingSlots& RenderTargets,
		TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTextures) override;
#endif

#if ENGINE_MINOR_VERSION >= 5
    virtual void SubscribeToPostProcessingPass(
        ISceneViewExtension::EPostProcessingPass Pass,
        const FSceneView& InView,
        FAfterPassCallbackDelegateArray& InOutPassCallbacks,
        bool bIsPassEnabled
    ) override;
#else
    virtual void SubscribeToPostProcessingPass(
        EPostProcessingPass Pass,
        FAfterPassCallbackDelegateArray& InOutPassCallbacks,
        bool bIsPassEnabled
    ) override;
#endif

	virtual int32 GetPriority() const override { return 0; }

	static void Register();
	static void Unregister();

private:
	FRDGTextureRef RenderHBAO_Internal_RenderThread(
		FRDGBuilder& GraphBuilder,
		const class FViewInfo& ViewInfo,
		const FSceneTextureShaderParameters& SceneTextureParams);

	FScreenPassTexture RenderHBAOPlus_RenderThread(
		FRDGBuilder& GraphBuilder,
		const FSceneView& View,
		const FPostProcessMaterialInputs& Inputs);

	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override;

	TMap<const FSceneViewStateInterface*, TRefCountPtr<IPooledRenderTarget>> HistoryRTs;
	int32 PreviousAOMethod = -1;
	float PreviousAOMipLevelFactor = -1.0f;
};