// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/EngineTypes.h"
#include "FogOfWarManager.generated.h"


UCLASS()
class RTSUNITTEMPLATE_API AFogOfWarManager : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AFogOfWarManager();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void CheckForCollisions();
	
	// Declare the replicated property
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	UStaticMeshComponent* Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int PlayerTeamId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int TeamId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	APlayerController* PlayerController;
	
	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	AActor* OwningUnit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool DetectFriendlyUnits = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool ManagerSetsVisibility = true;
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void AddUnitToChase(AActor* OtherActor);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void RemoveUnitToChase(AActor* OtherActor);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleBeginOverlapDetection(AActor* OtherActor);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleEndOverlapDetection(AActor* OtherActor);
	// Handle collision events to reveal units
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void OnMeshBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);


	
	// Handle collision end events to hide units
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void OnMeshEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

};