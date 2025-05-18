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

	UFUNCTION(Server, Reliable)
	void Server_RequestFogUpdate(const TArray<FMassEntityHandle>& Entities);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_UpdateFogMaskWithCircles(const TArray<FMassEntityHandle>& Entities);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector2D FogMinBounds = FVector2D(-10000.f, -10000.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector2D FogMaxBounds = FVector2D(10000.f, 10000.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 FogTexSize = 1024;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 CircleRadius = 32;
	
private:
	FTimerHandle FogUpdateTimerHandle;
	
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_ApplyFogMask(); // Helper to get controller and call ApplyFogMaskToMesh

	UPROPERTY()
	UTexture2D* FogMaskTexture;

	UPROPERTY()
	TArray<FColor> FogPixels;
	
};