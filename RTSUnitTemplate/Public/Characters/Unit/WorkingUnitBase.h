// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TransportUnit.h"
#include "GameFramework/Character.h"
#include "Components/WidgetComponent.h"
#include "Core/UnitData.h"
#include "Actors/WorkArea.h"
#include "Actors/WorkResource.h"
#include "PathSeekerBase.h"
#include "WorkingUnitBase.generated.h"


UCLASS()
class RTSUNITTEMPLATE_API AWorkingUnitBase : public ATransportUnit
{
	GENERATED_BODY()
private:
	
	FTimerHandle ShowWorkAreaTimerHandle;
	
public:
	virtual void BeginPlay() override;

	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const override;
	
	UFUNCTION(NetMulticast, Reliable, BlueprintCallable, Category=Worker)
	void SpawnWorkArea(TSubclassOf<AWorkArea> WorkAreaClass, AWaypoint* Waypoint);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category=Worker)
	void ServerSpawnWorkArea(TSubclassOf<AWorkArea> WorkAreaClass, AWaypoint* Waypoint, FVector HitLocation);

	UFUNCTION(BlueprintCallable, Category=Worker)
	void SpawnWorkAreaReplicated(TSubclassOf<AWorkArea> WorkAreaClass, AWaypoint* Waypoint, FVector SpawnLocation, const  FBuildingCost ConstructionCost, bool IsPaid = false);
	
	UFUNCTION(Client, Reliable)
	void ClientReceiveWorkArea(AWorkArea* ClientArea);
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = Worker)
	class AWorkArea* ResourcePlace;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = Worker)
	class ABuildingBase* Base;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = Worker)
	class AWorkArea* BuildArea;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Worker)
	AWorkResource* WorkResource;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	EResourceType ExtractingWorkResourceType = EResourceType::Primary;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Worker)
	float ResourceExtractionTime = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Worker)
	bool AutoMining = true;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	AWorkArea* CurrentDraggedWorkArea;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	UNiagaraComponent* Niagara_Build;

	UFUNCTION(BlueprintImplementableEvent, Category="RTSUnitTemplate")
	void StartBuild();


	UFUNCTION(BlueprintImplementableEvent, Category="RTSUnitTemplate")
	void FinishedBuild();
};








