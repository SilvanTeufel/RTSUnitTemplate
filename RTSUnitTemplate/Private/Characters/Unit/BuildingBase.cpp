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

	if(ResourceGameMode)
		ResourceGameMode->AddBaseToGroup(this);
}

void ABuildingBase::Destroyed()
{
	Super::Destroyed();
	
	AResourceGameMode* ResourceGameMode = Cast<AResourceGameMode>(GetWorld()->GetAuthGameMode());
	
	if(ResourceGameMode)
		ResourceGameMode->RemoveBaseFromGroup(this);
}

void ABuildingBase::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
                                   UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{


	// Since Worker is already AWorkingUnitBase, no need to cast again
	AUnitBase* UnitBase = Cast<AUnitBase>(OtherActor);
	if (!UnitBase || !UnitBase->IsWorker) return;
	
	UnitBase->Base = this;
	
	AResourceGameMode* ResourceGameMode = Cast<AResourceGameMode>(GetWorld()->GetAuthGameMode());
	if (!ResourceGameMode) return;
	
	bool CanAffordConstruction = false;

	if(UnitBase->BuildArea)
	{
		CanAffordConstruction = ResourceGameMode->CanAffordConstruction(UnitBase->BuildArea->ConstructionCost, UnitBase->TeamId);
	}
	
	if (UnitBase->IsWorker && IsBase && ResourceGameMode && UnitBase->GetUnitState() != UnitData::GoToBuild)
	{
		HandleBaseArea(UnitBase, ResourceGameMode, CanAffordConstruction);
	}

}

void ABuildingBase::HandleBaseArea(AUnitBase* UnitBase, AResourceGameMode* ResourceGameMode, bool CanAffordConstruction)
{
	UnitBase->UnitControlTimer = 0;
	UnitBase->SetUEPathfinding = true;

	bool AreaIsForTeam = false;
	if (UnitBase->BuildArea)
	{
		AreaIsForTeam = 
			(UnitBase->BuildArea->TeamId == 0) || 
			(UnitBase->TeamId == UnitBase->BuildArea->TeamId);
	}
	
	if(UnitBase->WorkResource)
	{
		//ResourceGameMode->ModifyTeamResourceAttributes(Worker->TeamId, Worker->WorkResource->ResourceType, Worker->WorkResource->Amount);
		//UE_LOG(LogTemp, Warning, TEXT("WorkResource Despawn!"));
		ResourceGameMode->ModifyResource(UnitBase->WorkResource->ResourceType, UnitBase->TeamId, UnitBase->WorkResource->Amount);
		DespawnWorkResource(UnitBase->WorkResource);
	}
	
	
	if (!SwitchBuildArea( UnitBase, ResourceGameMode))
	{
		SwitchResourceArea(UnitBase, ResourceGameMode);
	}
	
	
}

void ABuildingBase::SwitchResourceArea(AUnitBase* UnitBase, AResourceGameMode* ResourceGameMode)
{
	TArray<AWorkArea*> WorkPlaces = ResourceGameMode->GetClosestResourcePlaces(UnitBase);
	//ResourceGameMode->SetAllCurrentWorkers(Worker->TeamId);
	AWorkArea* NewResourcePlace = ResourceGameMode->GetSuitableWorkAreaToWorker(UnitBase->TeamId, WorkPlaces);

	if(UnitBase->ResourcePlace && NewResourcePlace && UnitBase->ResourcePlace->Type != NewResourcePlace->Type)
	{
		ResourceGameMode->AddCurrentWorkersForResourceType(UnitBase->TeamId, ConvertToResourceType(UnitBase->ResourcePlace->Type), -1.0f);
		ResourceGameMode->AddCurrentWorkersForResourceType(UnitBase->TeamId, ConvertToResourceType(NewResourcePlace->Type), +1.0f);
		UnitBase->ResourcePlace = NewResourcePlace;
	}else if(!UnitBase->ResourcePlace && NewResourcePlace)
	{
		ResourceGameMode->AddCurrentWorkersForResourceType(UnitBase->TeamId, ConvertToResourceType(NewResourcePlace->Type), +1.0f);
		UnitBase->ResourcePlace = NewResourcePlace;
	}

	UnitBase->SetUEPathfinding = true;
	UnitBase->SetUnitState(UnitData::GoToResourceExtraction);
}

bool ABuildingBase::SwitchBuildArea(AUnitBase* UnitBase, AResourceGameMode* ResourceGameMode)
{
	TArray<AWorkArea*> BuildAreas = ResourceGameMode->GetClosestBuildPlaces(UnitBase);
	BuildAreas.SetNum(3);
	
	UnitBase->BuildArea = ResourceGameMode->GetRandomClosestWorkArea(BuildAreas); // BuildAreas.Num() ? BuildAreas[0] : nullptr;

	bool CanAffordConstruction = UnitBase->BuildArea? ResourceGameMode->CanAffordConstruction(UnitBase->BuildArea->ConstructionCost, UnitBase->TeamId) : false; //Worker->BuildArea->CanAffordConstruction(Worker->TeamId, ResourceGameMode->NumberOfTeams,ResourceGameMode->TeamResources) : false;

	bool AreaIsForTeam = false;
	if (UnitBase->BuildArea)
	{
		AreaIsForTeam = 
			(UnitBase->BuildArea->TeamId == 0) || 
			(UnitBase->TeamId == UnitBase->BuildArea->TeamId);
	}


	if(CanAffordConstruction && UnitBase->BuildArea && !UnitBase->BuildArea->PlannedBuilding && AreaIsForTeam) // && AreaIsForTeam
	{
		UnitBase->BuildArea->PlannedBuilding = true;
		UnitBase->SetUEPathfinding = true;
		UnitBase->SetUnitState(UnitData::GoToBuild);
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

