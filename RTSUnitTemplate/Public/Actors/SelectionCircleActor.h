// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DecalActor.h"
#include "GameFramework/Actor.h"
#include "SelectionCircleActor.generated.h"

UCLASS()
class RTSUNITTEMPLATE_API ASelectionCircleActor : public AActor
{
    GENERATED_BODY()

public:
    ASelectionCircleActor();

    virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const override;

protected:
    virtual void BeginPlay() override;

public:
    virtual void Tick(float DeltaTime) override;

    UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
    int32 TeamId = 0;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = RTSUnitTemplate)
    UStaticMeshComponent* CircleMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
    UMaterialInterface* CircleMaterial;

    UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
    void InitCircleMaskTexture();

    UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
    void ApplyCircleMaskToMesh(UStaticMeshComponent* MeshComponent, UMaterialInterface* BaseMaterial, int32 MaterialIndex);

    UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
    void SetCircleBounds(const FVector2D& Min, const FVector2D& Max);

    UFUNCTION(NetMulticast, Unreliable, Category = RTSUnitTemplate)
    void Multicast_UpdateSelectionCircles(const TArray<FVector_NetQuantize>& Positions, const TArray<float>& WorldRadii);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
    int32 CircleMapSize = 200.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
    FVector2D CircleMapMinBounds;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
    FVector2D CircleMapMaxBounds;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
    int32 CircleTexSize = 4096;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
    float SelectionCircleUpdateRate = 0.05f;

private:
    FTimerHandle SelectionCircleUpdateTimerHandle;

    UFUNCTION(NetMulticast, Unreliable, Category = RTSUnitTemplate)
    void Multicast_ApplyCircleMask();

    UPROPERTY()
    UTexture2D* CircleMaskTexture;

    UPROPERTY()
    TArray<FColor> CirclePixels;
};