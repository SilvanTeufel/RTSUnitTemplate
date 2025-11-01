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
	/*
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	*/
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot); // Set it as the root component
	
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(SceneRoot);
	//SetRootComponent(Mesh);
	
	// Set collision enabled 
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly); // Query Only (No Physics Collision)
	Mesh->SetCollisionObjectType(ECC_WorldStatic); // Object Type: WorldStatic
	Mesh->SetGenerateOverlapEvents(true);

	// Set collision responses
	Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block); // Visibility: Block
	Mesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Overlap); // Camera: Overlap
	Mesh->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Overlap); // WorldStatic: Overlap
	Mesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap); // WorldDynamic: Overlap
	Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap); // Pawn: Overlap
	Mesh->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap); // PhysicsBody: Overlap
	Mesh->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Overlap); // Vehicle: Overlap
	Mesh->SetCollisionResponseToChannel(ECC_Destructible, ECR_Overlap); // Destructible: Overlap



	
	TriggerCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Is WorkArea Capsule"));
	TriggerCapsule->InitCapsuleSize(100.f, 100.0f);;
	TriggerCapsule->SetCollisionProfileName(TEXT("Trigger"));
	TriggerCapsule->SetupAttachment(RootComponent);
	//TriggerCapsule->OnComponentBeginOverlap.AddDynamic(this, &AWorkArea::OnOverlapBegin);

	//SceneRoot->SetVisibility(false, true);
	
	//if (HasAuthority())
	//{
		bReplicates = true;
		bAlwaysRelevant = true;
		//SetReplicateMovement(true);
	//}

	Mesh->SetIsReplicated(true); // was false
	MaxAvailableResourceAmount = AvailableResourceAmount;
}

// Called when the game starts or when spawned
void AWorkArea::BeginPlay()
{
	Super::BeginPlay();
	MaxAvailableResourceAmount = AvailableResourceAmount;
	SetReplicateMovement(false);
}


void AWorkArea::AddAreaToGroup_Implementation()
{
	AResourceGameMode* ResourceGameMode = Cast<AResourceGameMode>(GetWorld()->GetAuthGameMode());

	if(ResourceGameMode)
	{
		switch (Type)
		{
		case WorkAreaData::Primary:
			ResourceGameMode->WorkAreaGroups.PrimaryAreas.Add(this);
			break;
		case WorkAreaData::Secondary:
			ResourceGameMode->WorkAreaGroups.SecondaryAreas.Add(this);
			break;
		case WorkAreaData::Tertiary:
			ResourceGameMode->WorkAreaGroups.TertiaryAreas.Add(this);
			break;
		case WorkAreaData::Rare:
			ResourceGameMode->WorkAreaGroups.RareAreas.Add(this);
			break;
		case WorkAreaData::Epic:
			ResourceGameMode->WorkAreaGroups.EpicAreas.Add(this);
			break;
		case WorkAreaData::Legendary:
			ResourceGameMode->WorkAreaGroups.LegendaryAreas.Add(this);
			break;
		/*case WorkAreaData::Base:
			ResourceGameMode->WorkAreaGroups.BaseAreas.Add(this);
			break;*/
		case WorkAreaData::BuildArea:
			ResourceGameMode->WorkAreaGroups.BuildAreas.Add(this);
			break;
		default:
			// Handle any cases not explicitly covered
				break;
		}
	}
}

void AWorkArea::RemoveAreaFromGroup_Implementation()
{
	AResourceGameMode* ResourceGameMode = Cast<AResourceGameMode>(GetWorld()->GetAuthGameMode());

	if(ResourceGameMode)
	{
		switch (Type)
		{
		case WorkAreaData::Primary:
			ResourceGameMode->WorkAreaGroups.PrimaryAreas.Remove(this);
			break;
		case WorkAreaData::Secondary:
			ResourceGameMode->WorkAreaGroups.SecondaryAreas.Remove(this);
			break;
		case WorkAreaData::Tertiary:
			ResourceGameMode->WorkAreaGroups.TertiaryAreas.Remove(this);
			break;
		case WorkAreaData::Rare:
			ResourceGameMode->WorkAreaGroups.RareAreas.Remove(this);
			break;
		case WorkAreaData::Epic:
			ResourceGameMode->WorkAreaGroups.EpicAreas.Remove(this);
			break;
		case WorkAreaData::Legendary:
			ResourceGameMode->WorkAreaGroups.LegendaryAreas.Remove(this);
			break;
		/* case WorkAreaData::Base:
			ResourceGameMode->WorkAreaGroups.BaseAreas.Remove(this);
			break; */
		case WorkAreaData::BuildArea:
			ResourceGameMode->WorkAreaGroups.BuildAreas.Remove(this);
			break;
		default:
			// Handle any cases not explicitly covered
				break;
		}
	}
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
		if(!Building || PlannedBuilding && !StartedBuilding)
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
	DOREPLIFETIME(AWorkArea, TeamId);
	DOREPLIFETIME(AWorkArea, IsNoBuildZone);
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

	if(!ResourceGameMode) return;

	bool CanAffordConstruction;
	
	if(IsPaid)
		CanAffordConstruction = true;
	else	
		CanAffordConstruction = Worker->BuildArea? ResourceGameMode->CanAffordConstruction(Worker->BuildArea->ConstructionCost, Worker->TeamId) : false;//Worker->BuildArea->CanAffordConstruction(Worker->TeamId, ResourceGameMode->NumberOfTeams,ResourceGameMode->TeamResources) : false;
	
    if (isResourceExtractionArea && isValidStateForExtraction && Worker->GetUnitState() != UnitData::GoToBuild)
    {
        HandleResourceExtractionArea(UnitBase);
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

void AWorkArea::HandleResourceExtractionArea(AUnitBase* UnitBase)
{

		if (this != UnitBase->ResourcePlace) return;
		
		UnitBase->UnitControlTimer = 0;
		UnitBase->ExtractingWorkResourceType = ConvertWorkAreaTypeToResourceType(Type);
		UnitBase->SetUnitState(UnitData::ResourceExtraction);
		StartedResourceExtraction();
}

void AWorkArea::HandleBaseArea(AWorkingUnitBase* Worker, AUnitBase* UnitBase, AResourceGameMode* ResourceGameMode, bool CanAffordConstruction)
{
	
			UnitBase->UnitControlTimer = 0;
			UnitBase->SetUEPathfinding = true;
	
			if(Worker->WorkResource)
			{
				//ResourceGameMode->ModifyTeamResourceAttributes(Worker->TeamId, Worker->WorkResource->ResourceType, Worker->WorkResource->Amount);
				ResourceGameMode->ModifyResource(Worker->WorkResource->ResourceType, Worker->TeamId, Worker->WorkResource->Amount);
				DespawnWorkResource(UnitBase->WorkResource);
			}
	
	
			if (!SwitchBuildArea(Worker, UnitBase, ResourceGameMode))
			{
				SwitchResourceArea(Worker, UnitBase, ResourceGameMode);
			}
	
	
}

void AWorkArea::SwitchResourceArea(AWorkingUnitBase* Worker, AUnitBase* UnitBase, AResourceGameMode* ResourceGameMode)
{
	TArray<AWorkArea*> WorkPlaces = ResourceGameMode->GetClosestResourcePlaces(Worker);
	//ResourceGameMode->SetAllCurrentWorkers(Worker->TeamId);
	AWorkArea* NewResourcePlace = ResourceGameMode->GetSuitableWorkAreaToWorker(Worker->TeamId, WorkPlaces);

	if (NewResourcePlace)
	{
		if(UnitBase->ResourcePlace && UnitBase->ResourcePlace->Type != NewResourcePlace->Type)
		{
			ResourceGameMode->AddCurrentWorkersForResourceType(UnitBase->TeamId, ConvertToResourceType(UnitBase->ResourcePlace->Type), -1.0f);
			ResourceGameMode->AddCurrentWorkersForResourceType(UnitBase->TeamId, ConvertToResourceType(NewResourcePlace->Type), +1.0f);
		}
		else if(!UnitBase->ResourcePlace)
		{
			ResourceGameMode->AddCurrentWorkersForResourceType(UnitBase->TeamId, ConvertToResourceType(NewResourcePlace->Type), +1.0f);
		}
		UnitBase->ResourcePlace = NewResourcePlace;
	}
	else if (!UnitBase->ResourcePlace)
	{
		NewResourcePlace = ResourceGameMode->GetRandomClosestWorkArea(WorkPlaces);
		
		if (NewResourcePlace)
		{
			if(UnitBase->ResourcePlace && UnitBase->ResourcePlace->Type != NewResourcePlace->Type)
			{
				ResourceGameMode->AddCurrentWorkersForResourceType(UnitBase->TeamId, ConvertToResourceType(UnitBase->ResourcePlace->Type), -1.0f);
				ResourceGameMode->AddCurrentWorkersForResourceType(UnitBase->TeamId, ConvertToResourceType(NewResourcePlace->Type), +1.0f);
			}
			else if(!UnitBase->ResourcePlace)
			{
				ResourceGameMode->AddCurrentWorkersForResourceType(UnitBase->TeamId, ConvertToResourceType(NewResourcePlace->Type), +1.0f);
			}
			UnitBase->ResourcePlace = NewResourcePlace;
		}
	}

	/*
	if(Worker->ResourcePlace && NewResourcePlace && Worker->ResourcePlace->Type != NewResourcePlace->Type)
	{
		ResourceGameMode->AddCurrentWorkersForResourceType(Worker->TeamId, ConvertToResourceType(Worker->ResourcePlace->Type), -1.0f);
		ResourceGameMode->AddCurrentWorkersForResourceType(Worker->TeamId, ConvertToResourceType(NewResourcePlace->Type), +1.0f);
		Worker->ResourcePlace = NewResourcePlace;
	}else if(!Worker->ResourcePlace && NewResourcePlace)
	{
		ResourceGameMode->AddCurrentWorkersForResourceType(Worker->TeamId, ConvertToResourceType(NewResourcePlace->Type), +1.0f);
		Worker->ResourcePlace = NewResourcePlace;
	}*/
	
	UnitBase->SetUEPathfinding = true;
	Worker->SetUnitState(UnitData::GoToResourceExtraction);
	Worker->SwitchEntityTagByState(UnitData::GoToResourceExtraction, Worker->UnitStatePlaceholder);
}

bool AWorkArea::SwitchBuildArea(AWorkingUnitBase* Worker, AUnitBase* UnitBase, AResourceGameMode* ResourceGameMode)
{
	TArray<AWorkArea*> BuildAreas = ResourceGameMode->GetClosestBuildPlaces(Worker);
	BuildAreas.SetNum(3);
	
	Worker->BuildArea = ResourceGameMode->GetRandomClosestWorkArea(BuildAreas); // BuildAreas.Num() ? BuildAreas[0] : nullptr;

	bool CanAffordConstruction;
	
	if(IsPaid)
		CanAffordConstruction = true;
	else	
		CanAffordConstruction = Worker->BuildArea? ResourceGameMode->CanAffordConstruction(Worker->BuildArea->ConstructionCost, Worker->TeamId) : false; //Worker->BuildArea->CanAffordConstruction(Worker->TeamId, ResourceGameMode->NumberOfTeams,ResourceGameMode->TeamResources) : false;

	
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
		Worker->BuildArea->ControlTimer = 0.f;
		UnitBase->SetUEPathfinding = true;
		Worker->SetUnitState(UnitData::GoToBuild);
		Worker->SwitchEntityTagByState(UnitData::GoToBuild, Worker->UnitStatePlaceholder);
	} else
	{
		return false;
	}
	
	return true;
}

void AWorkArea::HandleBuildArea(AWorkingUnitBase* Worker, AUnitBase* UnitBase, AResourceGameMode* ResourceGameMode, bool CanAffordConstruction)
{
		
		if(!ResourceGameMode || !Worker) return; // Exit if the cast fails or game mode is not set

		bool AreaIsForTeam = false;
		if (Worker->BuildArea)
		{
			AreaIsForTeam = 
				(Worker->BuildArea->TeamId == 0) || 
				(Worker->TeamId == Worker->BuildArea->TeamId);
		}

		if(this == Worker->BuildArea && CanAffordConstruction && Building == nullptr && !StartedBuilding && AreaIsForTeam)
		{

			StartedBuilding = true;
			StartedBuild();


			if(!IsPaid)
			{
				ResourceGameMode->ModifyResource(EResourceType::Primary, Worker->TeamId, -Worker->BuildArea->ConstructionCost.PrimaryCost);
				ResourceGameMode->ModifyResource(EResourceType::Secondary, Worker->TeamId, -Worker->BuildArea->ConstructionCost.SecondaryCost);
				ResourceGameMode->ModifyResource(EResourceType::Tertiary, Worker->TeamId, -Worker->BuildArea->ConstructionCost.TertiaryCost);
				ResourceGameMode->ModifyResource(EResourceType::Rare, Worker->TeamId, -Worker->BuildArea->ConstructionCost.RareCost);
				ResourceGameMode->ModifyResource(EResourceType::Epic, Worker->TeamId, -Worker->BuildArea->ConstructionCost.EpicCost);
				ResourceGameMode->ModifyResource(EResourceType::Legendary, Worker->TeamId, -Worker->BuildArea->ConstructionCost.LegendaryCost);
			}
			
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
			{
				Worker->SetUnitState(UnitData::GoToBase);
				Worker->SwitchEntityTagByState(UnitData::GoToBase, Worker->UnitStatePlaceholder);
			}
			else
			{
				Worker->SetUnitState(UnitData::GoToResourceExtraction);
				Worker->SwitchEntityTagByState(UnitData::GoToResourceExtraction, Worker->UnitStatePlaceholder);
			}
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

void AWorkArea::Multicast_SetScale_Implementation(FVector NewScale)
{
	SetActorScale3D(NewScale);
}

void AWorkArea::TemporarilyChangeMaterial()
{
    // Ensure the Mesh component and the temporary material are valid
    if (!Mesh || !TemporaryHighlightMaterial)
    {
        UE_LOG(LogTemp, Warning, TEXT("AWorkArea::TemporarilyChangeMaterial: Mesh or TemporaryHighlightMaterial is not set."));
        return;
    }

    // If a revert timer is already active, clear it. This handles rapidly calling the function.
    GetWorld()->GetTimerManager().ClearTimer(ChangeMaterialTimerHandle);

    // If we haven't stored the original material yet, store it now.
    // This prevents overwriting the true original material if the function is called multiple times.
    if (!OriginalMaterial)
    {
        // We assume the material is on element index 0.
        OriginalMaterial = Mesh->GetMaterial(0); 
    }
    
    // Apply the temporary material
    Mesh->SetMaterial(0, TemporaryHighlightMaterial);

    // Set a timer to call the RevertMaterial function after 3.0 seconds
    const float RevertDelay = 0.25f;
    GetWorld()->GetTimerManager().SetTimer(
        ChangeMaterialTimerHandle,      // The handle to manage this timer
        this,                           // The object to call the function on
        &AWorkArea::RevertMaterial,     // The function to call
        RevertDelay,                    // The delay in seconds
        false                           // Do not loop
    );
}

/**
 * Reverts the material back to the one stored in OriginalMaterial.
 */
void AWorkArea::RevertMaterial()
{
    // Ensure the Mesh and the stored OriginalMaterial are still valid
    if (Mesh && OriginalMaterial)
    {
        // Apply the original material back to the mesh
        Mesh->SetMaterial(0, OriginalMaterial);
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("AWorkArea::RevertMaterial: Could not revert material as Mesh or OriginalMaterial is no longer valid."));
    }
    
    // Clear the stored material pointer now that we are done with it.
    // This makes the logic in TemporarilyChangeMaterial correct for the next time it's called.
    OriginalMaterial = nullptr;

    // It's good practice to clear the handle after the timer has finished its job.
    GetWorld()->GetTimerManager().ClearTimer(ChangeMaterialTimerHandle);
}
