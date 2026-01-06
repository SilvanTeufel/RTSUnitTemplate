// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "Components/PostProcessComponent.h"
#include "GameFramework/Actor.h"
#include "FogActor.generated.h"

UCLASS()
class RTSUNITTEMPLATE_API AFogActor : public AActor
{
	GENERATED_BODY()
	
public:	
	AFogActor();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;

	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const override;

	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int32 TeamId = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FogOfWar")
	UPostProcessComponent* PostProcessComponent;
	
	//UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = RTSUnitTemplate)
	//UStaticMeshComponent* FogMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	UMaterialInterface* FogMaterial;

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void InitFogMaskTexture();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void InitializeFogPostProcess();

	UFUNCTION()
	void RetryInitializeFogPostProcess();
	//UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	//void ApplyFogMaskToMesh(UStaticMeshComponent* MeshComponent, UMaterialInterface* BaseMaterial, int32 MaterialIndex);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetFogBounds(const FVector2D& Min, const FVector2D& Max);
	
	UFUNCTION(NetMulticast, Unreliable, Category = RTSUnitTemplate)
	void Multicast_UpdateFogMaskWithCircles(
		const TArray<FVector_NetQuantize>& Positions,
		const TArray<float>&              WorldRadii,
		const TArray<uint8>&              UnitTeamIds);

	// Local (non-replicated) fog update for client-side execution
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void UpdateFogMaskWithCircles_Local(
		const TArray<FVector_NetQuantize>& Positions,
		const TArray<float>&              WorldRadii,
		const TArray<uint8>&              UnitTeamIds);
	//void Multicast_UpdateFogMaskWithCircles(const TArray<FMassEntityHandle>& Entities);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int32 FogSize = 200.f;
	
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FVector2D FogMinBounds = FVector2D(-FogSize*50, -FogSize*50);

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FVector2D FogMaxBounds = FVector2D(FogSize*50, FogSize*50);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int32 FogTexSize = 0.25*1024;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int32 CircleRadius = 32;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float FogUpdateRate = 0.1f;
private:
	FTimerHandle FogUpdateTimerHandle;
	
	//UFUNCTION(NetMulticast, Unreliable, Category = RTSUnitTemplate)
	//void Multicast_ApplyFogMask(); // Helper to get controller and call ApplyFogMaskToMesh

	UPROPERTY(VisibleAnywhere, Category = RTSUnitTemplate)
	UTexture2D* FogMaskTexture;

	UPROPERTY(VisibleAnywhere, Category = RTSUnitTemplate)
	TArray<FColor> FogPixels;

	int32 InitPPAttempts = 0; // retry counter for PP init on clients
	
};