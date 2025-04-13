// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassEntitySubsystem.h"
#include "Components/ActorComponent.h"
#include "MassEntityTypes.h"
#include "Characters/Mass/UnitMassTag.h"
#include "MassActorBindingComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class RTSUNITTEMPLATE_API UMassActorBindingComponent : public USceneComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UMassActorBindingComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	FMassEntityHandle MassEntityHandle;

	// Caches the Entity Subsystem pointer
	UPROPERTY() // Don't save this pointer
	UMassEntitySubsystem* MassEntitySubsystemCache;

	UPROPERTY() 
	UMassEntitySubsystem* MassSubsystem;
public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY() 
	AActor* MyOwner;
	// Set by your spawner when binding the actor to a Mass entity.
	//void SetMassEntityHandle(FMassEntityHandle InHandle) { MassEntityHandle = InHandle; }
	
	FMassEntityHandle CreateAndLinkOwnerToMassEntity();

	// Getter for the handle
	FMassEntityHandle GetMassEntityHandle() const { return MassEntityHandle; }
	
		
};
