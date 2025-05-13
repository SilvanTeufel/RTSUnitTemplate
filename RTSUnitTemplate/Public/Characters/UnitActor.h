// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GAS/AttributeSetBase.h"
#include "UnitActor.generated.h"

UCLASS()
class RTSUNITTEMPLATE_API AUnitActor : public AActor
{
	GENERATED_BODY()

public:	
	AUnitActor(const FObjectInitializer& ObjectInitializer);;

	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const override;

	virtual void PostInitializeComponents() override;
protected:
	virtual void BeginPlay() override;

	virtual void OnConstruction(const FTransform& Transform) override;
public:	
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Unit")
	USkeletalMeshComponent* GetMesh() const { return SkeletalMesh; }

	UFUNCTION(BlueprintCallable, Category = "Unit")
	UCapsuleComponent* GetCapsuleComponent() const { return CapsuleComponent; }
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UFloatingPawnMovement* MovementComponent;
	
	UFUNCTION(BlueprintCallable, Category = "Unit")
	UPawnMovementComponent* GetMovementComponent() const { return MovementComponent; }

	/** Whether this unit should use SkeletalMesh behavior (like a character) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unit Mode")
	bool bUseSkeletalMovement = true;

	/** Capsule for collision (used in both modes) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UCapsuleComponent* CapsuleComponent;

	/** SkeletalMesh used in character-like mode */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USkeletalMeshComponent* SkeletalMesh;

	/** Instanced mesh used in lightweight mode */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UInstancedStaticMeshComponent* ISMComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ISM")
	int32 InstanceIndex = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, Category = RTSUnitTemplate)
	APlayerController* OwningPlayerController;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int TeamId = 1;

	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool CanOnlyAttackGround = false;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool CanOnlyAttackFlying = false;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool CanDetectInvisible = false;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool CanAttack = true;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool IsInvisible = false;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool IsFlying = false;
	
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool IsMyTeam = false;

	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	//bool EnemyStartVisibility = true;
	
	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	//float FogSight = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool IsVisibleEnemy = false;

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastSetEnemyVisibility(AUnitActor* DetectingActor, bool bVisible);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float FogDeadVisibilityTime = 10.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool EnableFog = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int FogManagerOverlaps = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int DetectorOverlaps = 0;
	


	UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category=RTSUnitTemplate, meta=(AllowPrivateAccess=true))
	class UAttributeSetBase* Attributes;


	
	/** Sphere component used for fog overlap testing */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fog")
	USphereComponent* SightSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool bFogSphereSpawned = false;
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleBeginOverlapDetection(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleEndOverlapDetection(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex
);
	
protected:
	void InitializeUnitMode();

	UFUNCTION(BlueprintCallable, Category = "Unit")
	void CreateISMInstance();

};
