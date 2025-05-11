// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "UnitActor.generated.h"

UCLASS()
class RTSUNITTEMPLATE_API AUnitActor : public AActor
{
	GENERATED_BODY()

public:	
	AUnitActor();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;
	
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

protected:
	void InitializeUnitMode();

};
