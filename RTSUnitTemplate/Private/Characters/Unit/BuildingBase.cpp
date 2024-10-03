// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Characters/Unit/BuildingBase.h"

#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "GameModes/ResourceGameMode.h"
#include "Components/CapsuleComponent.h"

void ABuildingBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// In your actor's constructor or initialization method

}

void ABuildingBase::BeginPlay()
{
	Super::BeginPlay();

	AResourceGameMode* ResourceGameMode = Cast<AResourceGameMode>(GetWorld()->GetAuthGameMode());
	ResourceGameMode->AddBaseToGroup(this);
}

void ABuildingBase::Destroyed()
{
	Super::Destroyed();
	
	AResourceGameMode* ResourceGameMode = Cast<AResourceGameMode>(GetWorld()->GetAuthGameMode());
	ResourceGameMode->RemoveBaseFromGroup(this);
}

void ABuildingBase::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
                                   UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// Early return if OtherActor is not a AWorkingUnitBase
	AWorkingUnitBase* Worker = Cast<AWorkingUnitBase>(OtherActor);
	if (!Worker) return;

	// Since Worker is already AWorkingUnitBase, no need to cast again
	AUnitBase* UnitBase = Cast<AUnitBase>(Worker);
	if (!UnitBase) return;
	

	AResourceGameMode* ResourceGameMode = Cast<AResourceGameMode>(GetWorld()->GetAuthGameMode());

	bool CanAffordConstruction = Worker->BuildArea? ResourceGameMode->CanAffordConstruction(Worker->BuildArea->ConstructionCost, Worker->TeamId) : false;

	if (IsBase && ResourceGameMode && Worker->GetUnitState() != UnitData::GoToBuild)
	{
		HandleBaseArea(Worker, UnitBase, ResourceGameMode, CanAffordConstruction);
	}

}

void ABuildingBase::HandleBaseArea(AWorkingUnitBase* Worker, AUnitBase* UnitBase, AResourceGameMode* ResourceGameMode, bool CanAffordConstruction)
{
	UnitBase->UnitControlTimer = 0;
	UnitBase->SetUEPathfinding = true;

	bool AreaIsForTeam = false;
	if (Worker->BuildArea)
	{
		AreaIsForTeam = 
			(Worker->BuildArea->TeamId == 0) || 
			(Worker->TeamId == Worker->BuildArea->TeamId);
	}
	
	if(Worker->WorkResource)
	{
		//ResourceGameMode->ModifyTeamResourceAttributes(Worker->TeamId, Worker->WorkResource->ResourceType, Worker->WorkResource->Amount);
		//UE_LOG(LogTemp, Warning, TEXT("WorkResource Despawn!"));
		ResourceGameMode->ModifyResource(Worker->WorkResource->ResourceType, Worker->TeamId, Worker->WorkResource->Amount);
		DespawnWorkResource(UnitBase->WorkResource);
	}
	
	
	if (!SwitchBuildArea(Worker, UnitBase, ResourceGameMode))
	{
		SwitchResourceArea(Worker, UnitBase, ResourceGameMode);
	}
	
	
}

void ABuildingBase::SwitchResourceArea(AWorkingUnitBase* Worker, AUnitBase* UnitBase, AResourceGameMode* ResourceGameMode)
{
	TArray<AWorkArea*> WorkPlaces = ResourceGameMode->GetClosestResourcePlaces(Worker);
	//ResourceGameMode->SetAllCurrentWorkers(Worker->TeamId);
	AWorkArea* NewResourcePlace = ResourceGameMode->GetSuitableWorkAreaToWorker(Worker->TeamId, WorkPlaces);

	if(Worker->ResourcePlace && NewResourcePlace && Worker->ResourcePlace->Type != NewResourcePlace->Type)
	{
		ResourceGameMode->AddCurrentWorkersForResourceType(Worker->TeamId, ConvertToResourceType(Worker->ResourcePlace->Type), -1.0f);
		ResourceGameMode->AddCurrentWorkersForResourceType(Worker->TeamId, ConvertToResourceType(NewResourcePlace->Type), +1.0f);
		Worker->ResourcePlace = NewResourcePlace;
	}else if(!Worker->ResourcePlace && NewResourcePlace)
	{
		ResourceGameMode->AddCurrentWorkersForResourceType(Worker->TeamId, ConvertToResourceType(NewResourcePlace->Type), +1.0f);
		Worker->ResourcePlace = NewResourcePlace;
	}

	UnitBase->SetUEPathfinding = true;
	Worker->SetUnitState(UnitData::GoToResourceExtraction);
}

bool ABuildingBase::SwitchBuildArea(AWorkingUnitBase* Worker, AUnitBase* UnitBase, AResourceGameMode* ResourceGameMode)
{
	TArray<AWorkArea*> BuildAreas = ResourceGameMode->GetClosestBuildPlaces(Worker);
	BuildAreas.SetNum(3);
	
	Worker->BuildArea = ResourceGameMode->GetRandomClosestWorkArea(BuildAreas); // BuildAreas.Num() ? BuildAreas[0] : nullptr;

	bool CanAffordConstruction = Worker->BuildArea? ResourceGameMode->CanAffordConstruction(Worker->BuildArea->ConstructionCost, Worker->TeamId) : false; //Worker->BuildArea->CanAffordConstruction(Worker->TeamId, ResourceGameMode->NumberOfTeams,ResourceGameMode->TeamResources) : false;

	bool AreaIsForTeam = false;
	if (Worker->BuildArea)
	{
		AreaIsForTeam = 
			(Worker->BuildArea->TeamId == 0) || 
			(Worker->TeamId == Worker->BuildArea->TeamId);
	}


	if(CanAffordConstruction && Worker->BuildArea && !Worker->BuildArea->PlannedBuilding && AreaIsForTeam) // && AreaIsForTeam
	{
		Worker->BuildArea->PlannedBuilding = true;
		UnitBase->SetUEPathfinding = true;
		Worker->SetUnitState(UnitData::GoToBuild);
	} else
	{
		return false;
	}
	
	return true;
}

void ABuildingBase::DespawnWorkResource(AWorkResource* ResourceToDespawn)
{
	if (ResourceToDespawn != nullptr)
	{
		ResourceToDespawn->Destroy();
		ResourceToDespawn = nullptr;
		//WorkResource->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	}
}

