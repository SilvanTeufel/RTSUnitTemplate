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

	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Fog")
	int32 TeamId = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fog")
	UStaticMeshComponent* FogMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog")
	UMaterialInterface* FogMaterial;

	UFUNCTION(BlueprintCallable)
	void InitFogMaskTexture();

	UFUNCTION(BlueprintCallable)
	void ApplyFogMaskToMesh(UStaticMeshComponent* MeshComponent, UMaterialInterface* BaseMaterial, int32 MaterialIndex);

	UFUNCTION(BlueprintCallable)
	void SetFogBounds(const FVector2D& Min, const FVector2D& Max);
	
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_UpdateFogMaskWithCircles(
		const TArray<FVector_NetQuantize>& Positions,
		const TArray<float>&              WorldRadii,
		const TArray<uint8>&              UnitTeamIds);
	//void Multicast_UpdateFogMaskWithCircles(const TArray<FMassEntityHandle>& Entities);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 FogSize = 200.f;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	FVector2D FogMinBounds = FVector2D(-FogSize*200*50, -FogSize*200*50);

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	FVector2D FogMaxBounds = FVector2D(FogSize*200*50, FogSize*200*50);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 FogTexSize = 0.25*1024;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 CircleRadius = 32;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float FogUpdateRate = 0.1f;
private:
	FTimerHandle FogUpdateTimerHandle;
	
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_ApplyFogMask(); // Helper to get controller and call ApplyFogMaskToMesh

	UPROPERTY()
	UTexture2D* FogMaskTexture;

	UPROPERTY()
	TArray<FColor> FogPixels;
	
};