// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Actors/UnitPreviewStage.h"

#include "Characters/Unit/UnitBase.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/TextureRenderTarget2D.h"

namespace
{
	// Helper meshes on units that do not belong into a portrait (fake shadows, selection rings, ...).
	bool IsHelperMesh(const UMeshComponent* Comp)
	{
		const FString Name = Comp->GetName();
		return Name.Contains(TEXT("Shadow")) || Name.Contains(TEXT("Selection")) || Name.Contains(TEXT("Circle"))
			|| Name.Contains(TEXT("Indicator")) || Name.Contains(TEXT("Range")) || Name.Contains(TEXT("Decal"));
	}
}

AUnitPreviewStage::AUnitPreviewStage()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = false;
	SetCanBeDamaged(false);
#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	ModelRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ModelRoot"));
	ModelRoot->SetupAttachment(Root);

	Capture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Capture"));
	Capture->SetupAttachment(Root);
	Capture->bCaptureEveryFrame = false;
	Capture->bCaptureOnMovement = false;
	Capture->bAlwaysPersistRenderingState = true;
	Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	// HDR scene colour carries the coverage in alpha, so the widget material can cut out the background.
	Capture->CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;
	Capture->FOVAngle = 30.f;
	Capture->ShowFlags.SetAtmosphere(false);
	Capture->ShowFlags.SetFog(false);
	Capture->ShowFlags.SetVolumetricFog(false);
	Capture->ShowFlags.SetCloud(false);
	Capture->ShowFlags.SetDynamicShadows(false);
	Capture->ShowFlags.SetMotionBlur(false);
	Capture->ShowFlags.SetParticles(false);
	Capture->ShowFlags.SetDecals(false);

	KeyLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("KeyLight"));
	KeyLight->SetupAttachment(Root);
	KeyLight->SetCastShadows(false);
	KeyLight->SetIntensityUnits(ELightUnits::Unitless);
	KeyLight->SetIntensity(120000.f);

	FillLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FillLight"));
	FillLight->SetupAttachment(Root);
	FillLight->SetCastShadows(false);
	FillLight->SetIntensityUnits(ELightUnits::Unitless);
	FillLight->SetIntensity(50000.f);
	FillLight->SetLightColor(FLinearColor(0.6f, 0.75f, 1.f));
}

void AUnitPreviewStage::ShowUnit(AUnitBase* Unit, UTextureRenderTarget2D* Target)
{
	if (Unit == ShownUnit.Get() && Target == ShownTarget.Get())
	{
		return;
	}

	ClearModel();
	ShownUnit = Unit;
	ShownTarget = Target;
	Capture->TextureTarget = Target;

	if (!Unit || !Target)
	{
		SetActorTickEnabled(false);
		return;
	}

	BuildModel(Unit);
	FrameModel();
	CaptureTimer = 0.f;
	SetActorTickEnabled(true);
	Capture->CaptureScene();
}

void AUnitPreviewStage::ClearModel()
{
	Capture->ShowOnlyComponents.Reset();
	for (UMeshComponent* Mesh : ModelMeshes)
	{
		if (Mesh)
		{
			Mesh->DestroyComponent();
		}
	}
	ModelMeshes.Reset();
	ModelRoot->SetRelativeRotation(FRotator::ZeroRotator);
}

void AUnitPreviewStage::BuildModel(AUnitBase* Unit)
{
	const FTransform UnitTransform = Unit->GetActorTransform();
	const bool bSkeletal = Unit->bUseSkeletalMovement;

	TArray<UMeshComponent*> SourceMeshes;
	Unit->GetComponents<UMeshComponent>(SourceMeshes);
	for (UMeshComponent* Source : SourceMeshes)
	{
		if (!Source || IsHelperMesh(Source))
		{
			continue;
		}

		UMeshComponent* Copy = nullptr;
		if (USkeletalMeshComponent* SourceSkel = Cast<USkeletalMeshComponent>(Source))
		{
			// Mass units draw through pooled ISMs; their skeletal mesh is only a placeholder then.
			if (!bSkeletal || !SourceSkel->GetSkeletalMeshAsset() || !SourceSkel->IsVisible())
			{
				continue;
			}
			USkeletalMeshComponent* Skel = NewObject<USkeletalMeshComponent>(this);
			Skel->SetSkeletalMeshAsset(SourceSkel->GetSkeletalMeshAsset());
			if (SourceSkel->GetAnimClass())
			{
				Skel->SetAnimInstanceClass(SourceSkel->GetAnimClass());
			}
			// Only the capture sees it; without this the pose would freeze off screen.
			Skel->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
			Copy = Skel;
		}
		else if (const UStaticMeshComponent* SourceStatic = Cast<UStaticMeshComponent>(Source))
		{
			const bool bIsISM = SourceStatic->IsA<UInstancedStaticMeshComponent>();
			// The ISM template is the look of a Mass unit; a skeletal unit's ISM is unused.
			if (!SourceStatic->GetStaticMesh() || (bIsISM && bSkeletal) || (!bIsISM && !SourceStatic->IsVisible()))
			{
				continue;
			}
			UStaticMeshComponent* Static = NewObject<UStaticMeshComponent>(this);
			Static->SetStaticMesh(SourceStatic->GetStaticMesh());
			Copy = Static;
		}
		if (!Copy)
		{
			continue;
		}

		for (int32 i = 0; i < Source->GetNumMaterials(); ++i)
		{
			Copy->SetMaterial(i, Source->GetMaterial(i));
		}
		Copy->SetCastShadow(false);
		Copy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Copy->SetVisibleInSceneCaptureOnly(true);
		Copy->SetupAttachment(ModelRoot);
		Copy->SetRelativeTransform(Source->GetComponentTransform().GetRelativeTransform(UnitTransform));
		Copy->RegisterComponent();
		ModelMeshes.Add(Copy);
		Capture->ShowOnlyComponents.Add(Copy);
	}
}

void AUnitPreviewStage::FrameModel()
{
	FBoxSphereBounds Bounds(ForceInit);
	bool bAny = false;
	for (UMeshComponent* Mesh : ModelMeshes)
	{
		Bounds = bAny ? Bounds + Mesh->Bounds : Mesh->Bounds;
		bAny = true;
	}
	if (!bAny)
	{
		return;
	}

	// Turn around the model's centre: shift the meshes so that it sits on ModelRoot.
	FVector Shift = ModelRoot->GetComponentLocation() - Bounds.Origin;
	if (const AUnitBase* Unit = ShownUnit.Get())
	{
		Shift += Unit->PreviewModelOffset * FMath::Max(Bounds.SphereRadius, 10.f);
	}
	for (UMeshComponent* Mesh : ModelMeshes)
	{
		Mesh->AddWorldOffset(Shift);
	}

	const float Radius = FMath::Max(Bounds.SphereRadius, 10.f);
	const float HalfFov = FMath::DegreesToRadians(Capture->FOVAngle * 0.5f);
	const float Distance = Radius / FMath::Sin(HalfFov) / FMath::Clamp(FillFactor, 0.1f, 1.f);
	// Look at the unit's front (units face +X).
	const FRotator View(-CameraPitch, 180.f, 0.f);
	const FVector Centre = ModelRoot->GetComponentLocation();
	Capture->SetWorldLocationAndRotation(Centre - View.Vector() * Distance, View);

	KeyLight->SetWorldLocation(Centre + FRotator(35.f, -40.f, 0.f).Vector() * -Distance);
	FillLight->SetWorldLocation(Centre + FRotator(10.f, 140.f, 0.f).Vector() * -Distance);
	KeyLight->SetAttenuationRadius(Distance * 3.f);
	FillLight->SetAttenuationRadius(Distance * 3.f);
}

void AUnitPreviewStage::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!ShownUnit.IsValid() || !ShownTarget.IsValid() || ModelMeshes.Num() == 0)
	{
		return;
	}

	ModelRoot->AddRelativeRotation(FRotator(0.f, RotationSpeed * DeltaSeconds, 0.f));

	CaptureTimer += DeltaSeconds;
	if (CaptureTimer >= 1.f / FMath::Max(CaptureRate, 1.f))
	{
		CaptureTimer = 0.f;
		Capture->CaptureScene();
	}
}
