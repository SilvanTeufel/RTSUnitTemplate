// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UnitPreviewStage.generated.h"

class AUnitBase;
class USceneCaptureComponent2D;
class UPointLightComponent;
class UTextureRenderTarget2D;
class UMeshComponent;
class UMaterialInterface;
class UWidget;

/**
 * Turntable for the 3D unit window of UUnitWidgetSelector. Sits far above the map, holds copies of the
 * meshes of the shown unit, turns them and renders them with one small SceneCapture into a render target
 * (the texture parameter of the selector's preview material). Only the copied meshes are rendered
 * (ShowOnly list), only while a unit is shown, and at most CaptureRate times per second.
 */
UCLASS(NotBlueprintable)
class RTSUNITTEMPLATE_API AUnitPreviewStage : public AActor
{
	GENERATED_BODY()

public:
	AUnitPreviewStage();

	/** Show Unit (nullptr = show nothing and stop capturing). Cheap when it already shows that unit. */
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void ShowUnit(AUnitBase* Unit, UTextureRenderTarget2D* Target);

	/**
	 * Drives one 3D window (UUnitWidgetSelector, UUnitStatsTooltipWidget): shows Unit in the render target behind
	 * Material's texture parameter, spawning Stage on demand at StageLocation, and collapses Box without a unit or
	 * render target. Each window needs its own render target and StageLocation - two stages at one spot would
	 * light each other's models.
	 */
	static void UpdateWindow(UObject* WorldContext, TObjectPtr<AUnitPreviewStage>& Stage, UWidget* Box, UMaterialInterface* Material,
		FName TextureParameter, AUnitBase* Unit, float InRotationSpeed, float InCaptureRate, const FVector& StageLocation);

	/** Degrees per second of the turntable. */
	float RotationSpeed = 30.f;

	/** Captures per second. */
	float CaptureRate = 30.f;

	/** Camera pitch onto the model (degrees down). */
	float CameraPitch = 18.f;

	/** How much of the picture the model fills (0..1). */
	float FillFactor = 0.85f;

protected:
	virtual void Tick(float DeltaSeconds) override;

private:
	void ClearModel();
	void BuildModel(AUnitBase* Unit);
	void FrameModel();

	UPROPERTY()
	TObjectPtr<USceneComponent> Root;

	/** The turning part; the copied meshes hang below it. */
	UPROPERTY()
	TObjectPtr<USceneComponent> ModelRoot;

	UPROPERTY()
	TObjectPtr<USceneCaptureComponent2D> Capture;

	UPROPERTY()
	TObjectPtr<UPointLightComponent> KeyLight;

	UPROPERTY()
	TObjectPtr<UPointLightComponent> FillLight;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMeshComponent>> ModelMeshes;

	TWeakObjectPtr<AUnitBase> ShownUnit;
	TWeakObjectPtr<UTextureRenderTarget2D> ShownTarget;
	float CaptureTimer = 0.f;
};
