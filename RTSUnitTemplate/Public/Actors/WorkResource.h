// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
//#include "Characters/Unit/UnitBase.h"
#include "TimerManager.h"
#include "Core/WorkerData.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "WorkResource.generated.h"

UCLASS()
class RTSUNITTEMPLATE_API AWorkResource : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AWorkResource();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	class UCapsuleComponent* TriggerCapsule;
	
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintImplementableEvent, Category = RTSUnitTemplate)
	void ImpactEvent();
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	UStaticMeshComponent* Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TMap<EResourceType, UStaticMesh*> ResourceMeshes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TMap<EResourceType, UMaterialInterface*> ResourceMaterials;

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetResourceActive(bool bActive, EResourceType Type = EResourceType::Primary, float InAmount = 0.f, FVector Offset = FVector::ZeroVector);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
	float PickUpDistance = 25.f;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool FollowTarget = false;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
	AActor* Target;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float MovementSpeed = 13.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	USoundBase* Sound;

	UPROPERTY(ReplicatedUsing=OnRep_IsAttached, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool IsAttached = false;

	UPROPERTY(ReplicatedUsing=OnRep_ResourceType, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	EResourceType ResourceType = EResourceType::Primary;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float Amount = 10.f;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FVector SocketOffset = FVector(10.0f, 10.0f, 0.0f);

	UFUNCTION()
	void OnRep_IsAttached();

	UFUNCTION()
	void OnRep_ResourceType();
};
