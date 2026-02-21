// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "Components/DecalComponent.h"
#include "Net/UnrealNetwork.h"
#include "AreaDecalComponent.generated.h"

class UMaterialInterface;
class UMaterialInstanceDynamic;
class URuntimeVirtualTexture;
class UStaticMeshComponent;
class UStaticMesh;

UCLASS(Blueprintable, meta=(BlueprintSpawnableComponent))
class RTSUNITTEMPLATE_API UAreaDecalComponent : public UDecalComponent
{
	GENERATED_BODY()

public:
	UAreaDecalComponent();

	// Required function to register replicated properties.
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	TSubclassOf<UGameplayEffect> FriendlyEffect;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	TSubclassOf<UGameplayEffect> EnemyEffect;
	
	// The current material being used for the decal. Replicated.
	UPROPERTY(Transient, ReplicatedUsing = OnRep_CurrentMaterial)
	UMaterialInterface* CurrentMaterial;

	// The current color of the decal. Replicated.
	UPROPERTY(Transient, ReplicatedUsing = OnRep_DecalColor)
	FLinearColor CurrentDecalColor;

	// The current radius of the decal. Replicated.
	UPROPERTY(Transient, ReplicatedUsing = OnRep_DecalRadius)
	float CurrentDecalRadius;

	float LastNotifiedRadius = -1.f;
	FLinearColor LastNotifiedColor = FLinearColor::Transparent;

	UPROPERTY(Transient)
	float VisibilityCheckInterval = 10.f;

	float TimeSinceLastVisibilityCheck = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	float StandardDecalRadiusDivider = 3.f;

	UPROPERTY(EditAnywhere, Transient, ReplicatedUsing = OnRep_DecalIsVisible, Category = "RTSUnitTemplate")
	bool bDecalIsVisible = false;

	// --- RVT Support ---

	// Schaltet zwischen normalem Decal und RVT-Schreibweise um
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_UseRuntimeVirtualTexture, Category = "RVT")
	bool bUseRuntimeVirtualTexture = false;

	// Einstellbare Größe des RVT-Schreib-Meshs (Kantenlänge in Unreal Units). Standard 2000
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RVT")
	float RVTWriterMeshSize;

	// Einstellbarer Material-Parameterbereich für den Radius (0..0.5 entspricht Mesh-Halbkante)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RVT")
	float RVTMaterialRadiusMin = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RVT")
	float RVTMaterialRadiusMax = 0.5f;

	// Number of decimal places for the normalized radius/alpha (Default: 2)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RVT")
	int32 RVTMaterialRadiusPrecision = 4;

	// Die Ziel-RVT, in die geschrieben werden soll
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RVT")
	URuntimeVirtualTexture* TargetVirtualTexture;

	// Material für den RVT-Schreibvorgang (sollte ein RVT-Output Material sein)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RVT")
	UMaterialInterface* RVTWriterMaterial;

	// Custom mesh for RVT writing. If null, a default plane is used.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RVT")
	UStaticMesh* RVTWriterCustomMesh;

	// Hilfskomponente zum Schreiben in die RVT
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RVT")
	UStaticMeshComponent* RVTWriterComponent;

	UPROPERTY(Transient)
	UMaterialInstanceDynamic* RVTWriterDynamicMaterial;
	
	// Called on clients when CurrentMaterial is updated by the server.
	UFUNCTION()
	void OnRep_CurrentMaterial();
	
	// Called on clients when CurrentDecalColor is updated.
	UFUNCTION()
	void OnRep_DecalColor();

	// Called on clients when CurrentDecalRadius is updated.
	UFUNCTION()
	void OnRep_DecalRadius();

	// Called on clients when bUseRuntimeVirtualTexture is updated.
	UFUNCTION()
	void OnRep_UseRuntimeVirtualTexture();

	// Called on clients when bDecalIsVisible is updated.
	UFUNCTION()
	void OnRep_DecalIsVisible();

	// Helper function to apply all visual updates based on replicated properties.
	void UpdateDecalVisuals();

private:
	// Dynamic material instance to change parameters at runtime.
	UPROPERTY()
	UMaterialInstanceDynamic* DynamicDecalMaterial;

public:
	/**
	 * Activates the AoE decal. Must be called on the server.
	 * @param NewMaterial The material to use for the decal's appearance.
	 * @param NewColor The color to apply to the material.
	 * @param NewRadius The radius of the decal effect.
	 */
	UFUNCTION(BlueprintCallable, Category = "Area Decal", Server, Reliable)
	void Server_ActivateDecal(UMaterialInterface* NewMaterial, FLinearColor NewColor, float NewRadius);

	/**
	 * Deactivates the AoE decal. Must be called on the server.
	 */
	UFUNCTION(BlueprintCallable, Category = "Area Decal", Server, Reliable)
	void Server_DeactivateDecal();

	/**
	 * Smoothly scales the decal radius to the target value over the given time (linear interpolation).
	 * Must be called on the server.
	 * @param EndRadius Target radius to reach.
	 * @param TimeSeconds Duration for the transition. If <= 0, the radius is set immediately.
	 */
	UFUNCTION(BlueprintCallable, Category = "Area Decal", Server, Reliable)
	void Server_ScaleDecalToRadius(float EndRadius, float TimeSeconds, bool OwnerIsBeacon = false);

	void SetCurrentDecalRadiusFromMass(float NewRadius);

	/**
	 * Multicast version of ScaleDecalToRadius to allow smooth client-side interpolation.
	 */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_ScaleDecalToRadius(float EndRadius, float TimeSeconds, bool OwnerIsBeacon);

	// Mass-based scaling helpers
	UFUNCTION(BlueprintCallable, Category = "Area Decal")
	void StartMassScaling(float InStartRadius, float InTargetRadius, float InDurationSeconds, bool bInOwnerIsBeacon);

	bool AdvanceMassScaling(float DeltaSeconds, float& OutNewRadius, bool& bOutCompleted);
	bool IsBeaconScaling() const { return bScaleOwnerIsBeacon; }

protected:
	float ScaleStartRadius = 0.f;
	float ScaleTargetRadius = 0.f;
	float ScaleDuration = 0.f;
	bool bIsScaling = false;
	bool bScaleOwnerIsBeacon = false; // if true, push current radius to owning Building's beacon range each step

	// Mass-based scaling elapsed accumulator
	float MassElapsedTime = 0.f;

	// Helper to propagate radius to Mass fragment when available (server-only)
	void UpdateMassEffectRadius(float NewRadius);
};
