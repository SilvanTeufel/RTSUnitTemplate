/*
// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
//
// This component is designed to create a replicated Area of Effect (AoE) decal
// that is visually defined by a material and can represent a gameplay effect.
*/

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "Components/DecalComponent.h"
#include "Net/UnrealNetwork.h"
#include "AreaDecalComponent.generated.h"

class UMaterialInterface;
class UMaterialInstanceDynamic;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTSUNITTEMPLATE_API UAreaDecalComponent : public UDecalComponent
{
	GENERATED_BODY()

public:
	UAreaDecalComponent();

	// Required function to register replicated properties.
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// The gameplay effect this decal represents. Set this in the Blueprint.

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TSubclassOf<UGameplayEffect> FriendlyEffect;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
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

	UPROPERTY(Transient, ReplicatedUsing = OnRep_DecalRadius)
	float TickInterval = 0.5f;
	/*
	UPROPERTY(Transient, ReplicatedUsing = OnRep_DecalRadius)
	bool AddsFriendlyGameplayEffect = true;

	UPROPERTY(Transient, ReplicatedUsing = OnRep_DecalRadius)
	bool AddsEnemyGameplayEffect = true;

	*/
	// Called on clients when CurrentMaterial is updated by the server.
	UFUNCTION()
	void OnRep_CurrentMaterial();
	
	// Called on clients when CurrentDecalColor is updated.
	UFUNCTION()
	void OnRep_DecalColor();

	// Called on clients when CurrentDecalRadius is updated.
	UFUNCTION()
	void OnRep_DecalRadius();

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
};