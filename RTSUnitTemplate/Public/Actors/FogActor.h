// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = RTSUnitTemplate)
	UStaticMeshComponent* FogMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	UMaterialInterface* FogMaterial;

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void InitFogMaskTexture();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void ApplyFogMaskToMesh(UStaticMeshComponent* MeshComponent, UMaterialInterface* BaseMaterial, int32 MaterialIndex);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetFogBounds(const FVector2D& Min, const FVector2D& Max);
	
	UFUNCTION(NetMulticast, Unreliable, Category = RTSUnitTemplate)
	void Multicast_UpdateFogMaskWithCircles(
		const TArray<FVector_NetQuantize>& Positions,
		const TArray<float>&              WorldRadii,
		const TArray<uint8>&              UnitTeamIds);
	//void Multicast_UpdateFogMaskWithCircles(const TArray<FMassEntityHandle>& Entities);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int32 FogSize = 200.f;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FVector2D FogMinBounds = FVector2D(-FogSize*200*50, -FogSize*200*50);

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FVector2D FogMaxBounds = FVector2D(FogSize*200*50, FogSize*200*50);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int32 FogTexSize = 0.25*1024;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int32 CircleRadius = 32;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float FogUpdateRate = 0.1f;
private:
	FTimerHandle FogUpdateTimerHandle;
	
	UFUNCTION(NetMulticast, Unreliable, Category = RTSUnitTemplate)
	void Multicast_ApplyFogMask(); // Helper to get controller and call ApplyFogMaskToMesh

	UPROPERTY(VisibleAnywhere, Category = RTSUnitTemplate)
	UTexture2D* FogMaskTexture;

	UPROPERTY(VisibleAnywhere, Category = RTSUnitTemplate)
	TArray<FColor> FogPixels;
	
};