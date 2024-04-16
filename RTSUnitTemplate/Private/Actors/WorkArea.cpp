// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Actors/WorkArea.h"

#include "Characters/Unit/BuildingBase.h"
#include "Core/WorkerData.h"
#include "Characters/Unit/UnitBase.h"
#include "Components/CapsuleComponent.h"
#include "GameModes/ResourceGameMode.h"
#include "Net/UnrealNetwork.h"


// Sets default values
AWorkArea::AWorkArea()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh->SetCollisionProfileName(TEXT("Trigger")); // Kollisionsprofil festlegen
	Mesh->SetGenerateOverlapEvents(true);


	TriggerCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Is WorkArea Capsule"));
	TriggerCapsule->InitCapsuleSize(100.f, 100.0f);;
	TriggerCapsule->SetCollisionProfileName(TEXT("Trigger"));
	TriggerCapsule->SetupAttachment(RootComponent);
	//TriggerCapsule->OnComponentBeginOverlap.AddDynamic(this, &AWorkArea::OnOverlapBegin);

	
	if (HasAuthority())
	{
		bReplicates = true;
		SetReplicateMovement(true);
	}
}

// Called when the game starts or when spawned
void AWorkArea::BeginPlay()
{
	Super::BeginPlay();

}

// Called every frame
void AWorkArea::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	if(Building && Building->GetUnitState() == UnitData::Dead)
	{
		PlannedBuilding = false;
		StartedBuilding = false;
		Building = nullptr;
	}

	
	ControlTimer += DeltaTime;
	if(ControlTimer >= ResetStartBuildTime)
	{
		if(!Building)
		{
			PlannedBuilding = false;
			StartedBuilding = false;
		}
		ControlTimer = 0.f;
	}
	
}

void AWorkArea::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWorkArea, AreaEffect);
	DOREPLIFETIME(AWorkArea, Mesh);
}

void AWorkArea::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    // Early return if OtherActor is not a AWorkingUnitBase
    AWorkingUnitBase* Worker = Cast<AWorkingUnitBase>(OtherActor);
    if (!Worker) return;

    // Since Worker is already AWorkingUnitBase, no need to cast again
    AUnitBase* UnitBase = Cast<AUnitBase>(Worker);
    if (!UnitBase) return;

    // Check for work area types that involve resource extraction
    bool isResourceExtractionArea = Type == WorkAreaData::Primary || Type == WorkAreaData::Secondary || 
                                     Type == WorkAreaData::Tertiary || Type == WorkAreaData::Rare ||
                                     Type == WorkAreaData::Epic || Type == WorkAreaData::Legendary;
    bool isValidStateForExtraction = UnitBase->GetUnitState() == UnitData::GoToResourceExtraction || 
                                     UnitBase->GetUnitState() == UnitData::Evasion;

	AResourceGameMode* ResourceGameMode = Cast<AResourceGameMode>(GetWorld()->GetAuthGameMode());

	bool CanAffordConstruction = Worker->BuildArea? ResourceGameMode->CanAffordConstruction(Worker->BuildArea->ConstructionCost, Worker->TeamId) : false;//Worker->BuildArea->CanAffordConstruction(Worker->TeamId, ResourceGameMode->NumberOfTeams,ResourceGameMode->TeamResources) : false;
	
    if (isResourceExtractionArea && isValidStateForExtraction && Worker->GetUnitState() != UnitData::GoToBuild)
    {
        HandleResourceExtractionArea(Worker, UnitBase);
    }
    else if (Type == WorkAreaData::Base && ResourceGameMode && Worker->GetUnitState() != UnitData::GoToBuild)
    {
        HandleBaseArea(Worker, UnitBase, ResourceGameMode, CanAffordConstruction);
    }
    else if (Type == WorkAreaData::BuildArea && ResourceGameMode)
    {
        HandleBuildArea(Worker, UnitBase, ResourceGameMode, CanAffordConstruction);
    }
}

void AWorkArea::HandleResourceExtractionArea(AWorkingUnitBase* Worker, AUnitBase* UnitBase)
{

		UnitBase->UnitControlTimer = 0;
		UnitBase->ExtractingWorkResourceType = ConvertWorkAreaTypeToResourceType(Type);
		UnitBase->SetUnitState(UnitData::ResourceExtraction);
}

void AWorkArea::HandleBaseArea(AWorkingUnitBase* Worker, AUnitBase* UnitBase, AResourceGameMode* ResourceGameMode, bool CanAffordConstruction)
{
			UnitBase->UnitControlTimer = 0;
			UnitBase->SetUEPathfinding = true;
	
			if(Worker->WorkResource)
			{
				//ResourceGameMode->ModifyTeamResourceAttributes(Worker->TeamId, Worker->WorkResource->ResourceType, Worker->WorkResource->Amount);
				//UE_LOG(LogTemp, Warning, TEXT("WorkResource Despawn!"));
				ResourceGameMode->ModifyResource(Worker->WorkResource->ResourceType, Worker->TeamId, Worker->WorkResource->Amount);
				DespawnWorkResource(UnitBase->WorkResource);
			}
	

			if (Worker->BuildArea && !Worker->BuildArea->PlannedBuilding && !CanAffordConstruction)
			{
				SwitchResourceArea(Worker, UnitBase, ResourceGameMode);
			}else if((Worker->BuildArea && Worker->BuildArea->PlannedBuilding) || !Worker->BuildArea)
			{
				SwitchBuildArea(Worker, UnitBase, ResourceGameMode);
			}else if(Worker->BuildArea && !Worker->BuildArea->PlannedBuilding && CanAffordConstruction)
			{
				Worker->BuildArea->PlannedBuilding = true;
				UnitBase->SetUEPathfinding = true;
				Worker->SetUnitState(UnitData::GoToBuild);
			}else
			{
				UnitBase->SetUEPathfinding = true;
				Worker->SetUnitState(UnitData::GoToResourceExtraction);
			}
	
}

void AWorkArea::SwitchResourceArea(AWorkingUnitBase* Worker, AUnitBase* UnitBase, AResourceGameMode* ResourceGameMode)
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
	//ResourceGameMode->SetAllCurrentWorkers(Worker->TeamId);

	
	UnitBase->SetUEPathfinding = true;
	Worker->SetUnitState(UnitData::GoToResourceExtraction);
}

void AWorkArea::SwitchBuildArea(AWorkingUnitBase* Worker, AUnitBase* UnitBase, AResourceGameMode* ResourceGameMode)
{
	TArray<AWorkArea*> BuildAreas = ResourceGameMode->GetClosestBuildPlaces(Worker);
	BuildAreas.SetNum(3);
	
	Worker->BuildArea = ResourceGameMode->GetRandomClosestWorkArea(BuildAreas); // BuildAreas.Num() ? BuildAreas[0] : nullptr;

	bool CanAffordConstruction = Worker->BuildArea? ResourceGameMode->CanAffordConstruction(Worker->BuildArea->ConstructionCost, Worker->TeamId) : false; //Worker->BuildArea->CanAffordConstruction(Worker->TeamId, ResourceGameMode->NumberOfTeams,ResourceGameMode->TeamResources) : false;
	
	if(!CanAffordConstruction || !Worker->BuildArea)
	{
		UnitBase->SetUEPathfinding = true;
		if(Type == WorkAreaData::Base)
			Worker->SetUnitState(UnitData::GoToResourceExtraction);
		else if(Worker->WorkResource)
			Worker->SetUnitState(UnitData::GoToBase);
		else
			Worker->SetUnitState(UnitData::GoToResourceExtraction);
		
	}else if(CanAffordConstruction && Worker->BuildArea && !Worker->BuildArea->PlannedBuilding)
	{
		Worker->BuildArea->PlannedBuilding = true;
		UnitBase->SetUEPathfinding = true;
		Worker->SetUnitState(UnitData::GoToBuild);
	}else
	{
		UnitBase->SetUEPathfinding = true;
		Worker->SetUnitState(UnitData::GoToBase);
	}
}

void AWorkArea::HandleBuildArea(AWorkingUnitBase* Worker, AUnitBase* UnitBase, AResourceGameMode* ResourceGameMode, bool CanAffordConstruction)
{
		
		if(!ResourceGameMode || !Worker) return; // Exit if the cast fails or game mode is not set
	
		if((this == Worker->BuildArea) && CanAffordConstruction && Building == nullptr && !StartedBuilding)
		{
			StartedBuilding = true;
			/*
			ResourceGameMode->ModifyTeamResourceAttributes(Worker->TeamId, EResourceType::Primary, -Worker->BuildArea->ConstructionCost.PrimaryCost);
			ResourceGameMode->ModifyTeamResourceAttributes(Worker->TeamId, EResourceType::Secondary, -Worker->BuildArea->ConstructionCost.SecondaryCost);
			ResourceGameMode->ModifyTeamResourceAttributes(Worker->TeamId, EResourceType::Tertiary, -Worker->BuildArea->ConstructionCost.TertiaryCost);
			ResourceGameMode->ModifyTeamResourceAttributes(Worker->TeamId, EResourceType::Rare, -Worker->BuildArea->ConstructionCost.RareCost);
			ResourceGameMode->ModifyTeamResourceAttributes(Worker->TeamId, EResourceType::Epic, -Worker->BuildArea->ConstructionCost.EpicCost);
			ResourceGameMode->ModifyTeamResourceAttributes(Worker->TeamId, EResourceType::Legendary, -Worker->BuildArea->ConstructionCost.LegendaryCost);
			*/
			
			ResourceGameMode->ModifyResource(EResourceType::Primary, Worker->TeamId, -Worker->BuildArea->ConstructionCost.PrimaryCost);
			ResourceGameMode->ModifyResource(EResourceType::Secondary, Worker->TeamId, -Worker->BuildArea->ConstructionCost.SecondaryCost);
			ResourceGameMode->ModifyResource(EResourceType::Tertiary, Worker->TeamId, -Worker->BuildArea->ConstructionCost.TertiaryCost);
			ResourceGameMode->ModifyResource(EResourceType::Rare, Worker->TeamId, -Worker->BuildArea->ConstructionCost.RareCost);
			ResourceGameMode->ModifyResource(EResourceType::Epic, Worker->TeamId, -Worker->BuildArea->ConstructionCost.EpicCost);
			ResourceGameMode->ModifyResource(EResourceType::Legendary, Worker->TeamId, -Worker->BuildArea->ConstructionCost.LegendaryCost);
			
			UnitBase->UnitControlTimer = 0;
			UnitBase->SetUEPathfinding = true;
			UnitBase->SetUnitState(UnitData::Build);
		}else if (this == Worker->BuildArea && (Building != nullptr || !StartedBuilding) && CanAffordConstruction)
		{
			SwitchBuildArea(Worker, UnitBase, ResourceGameMode);
		}else if(this == Worker->BuildArea &&  Worker->GetUnitState() != UnitData::Build)
		{
			UnitBase->SetUEPathfinding = true;
			if(Worker->WorkResource)
				Worker->SetUnitState(UnitData::GoToBase);
			else
				Worker->SetUnitState(UnitData::GoToResourceExtraction);
		}
}

EResourceType AWorkArea::ConvertWorkAreaTypeToResourceType(WorkAreaData::WorkAreaType WorkAreaType)
{
    switch (WorkAreaType)
    {
    case WorkAreaData::Primary: return EResourceType::Primary;
    case WorkAreaData::Secondary: return EResourceType::Secondary;
    case WorkAreaData::Tertiary: return EResourceType::Tertiary;
    case WorkAreaData::Rare: return EResourceType::Rare;
    case WorkAreaData::Epic: return EResourceType::Epic;
    case WorkAreaData::Legendary: return EResourceType::Legendary;
    default: return EResourceType::Primary; // Assuming EResourceType::None exists for error handling
    }
}


void AWorkArea::DespawnWorkResource(AWorkResource* WorkResource)
{
	if (WorkResource != nullptr)
	{
		WorkResource->Destroy();
		WorkResource = nullptr;
		//WorkResource->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	}
}

/*
bool AWorkArea::CanAffordConstruction(int32 TeamId, int32 NumberOfTeams, TArray<FResourceArray> TeamResources)
{
	if (TeamId < 0 || TeamId >= NumberOfTeams || !this)
		return false;

	// It's crucial to ensure that ConstructionCost is properly initialized.
	// This example assumes ConstructionCost is valid. If there's a chance it might not be,
	// consider adding additional checks or initialization logic before this point.

	// Initialize a map to store the total costs for easy comparison
	TMap<EResourceType, int32> Costs;
	Costs.Emplace(EResourceType::Primary, ConstructionCost.PrimaryCost);
	Costs.Emplace(EResourceType::Secondary,ConstructionCost.SecondaryCost);
	Costs.Emplace(EResourceType::Tertiary, ConstructionCost.TertiaryCost);
	Costs.Emplace(EResourceType::Rare, ConstructionCost.RareCost);
	Costs.Emplace(EResourceType::Epic, ConstructionCost.EpicCost);
	Costs.Emplace(EResourceType::Legendary, ConstructionCost.LegendaryCost);

	// Verify resources for each type
	for (const FResourceArray& ResourceArray : TeamResources)
	{
		// Ensure TeamId is within bounds for the resource array
		if (!ResourceArray.Resources.IsValidIndex(TeamId))
		{
			return false; // This ensures we don't proceed with invalid TeamId
		}

		int32 ResourceAmount = ResourceArray.Resources[TeamId];

		// Check if the team has enough resources of the current type
		if (Costs.Contains(ResourceArray.ResourceType) && ResourceAmount < Costs[ResourceArray.ResourceType])
		{
			return false; // Not enough resources of this type
		}
	}

	// If all costs are affordable
	return true;
}*/