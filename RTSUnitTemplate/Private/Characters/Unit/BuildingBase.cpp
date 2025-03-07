// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Characters/Unit/BuildingBase.h"

#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "GameModes/ResourceGameMode.h"
#include "Components/CapsuleComponent.h"


ABuildingBase::ABuildingBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SnapMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SnapMesh"));
	SnapMesh->SetupAttachment(RootComponent);  // Ensure SceneRoot is valid

	// Set collision enabled 
	SnapMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly); // Query Only (No Physics Collision)
	SnapMesh->SetCollisionObjectType(ECC_WorldStatic);           // Object Type: WorldStatic
	SnapMesh->SetGenerateOverlapEvents(true);

	// Set collision responses
	SnapMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);   // Visibility: Block
	SnapMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Overlap);       // Camera: Overlap
	SnapMesh->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Overlap);  // WorldStatic: Overlap
	SnapMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap); // WorldDynamic: Overlap
	SnapMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);         // Pawn: Overlap
	SnapMesh->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);  // PhysicsBody: Overlap
	SnapMesh->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Overlap);      // Vehicle: Overlap
	SnapMesh->SetCollisionResponseToChannel(ECC_Destructible, ECR_Overlap);   // Destructible: Overlap
}

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
	
		if(UnitBase->BuildArea->IsPaid)
			CanAffordConstruction = true;
		else	
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
	
	if(UnitBase->WorkResource)
	{
		//ResourceGameMode->ModifyTeamResourceAttributes(Worker->TeamId, Worker->WorkResource->ResourceType, Worker->WorkResource->Amount);
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
	// Log initial state
	/*
	UE_LOG(LogTemp, Log, TEXT("1111 ABuildingBase::SwitchResourceArea - Unit %s (Team %d) current resource place: %s"), 
		*UnitBase->GetName(), 
		UnitBase->TeamId,
		UnitBase->ResourcePlace ? *UnitBase->ResourcePlace->GetName() : TEXT("None"));
	*/

	TArray<AWorkArea*> WorkPlaces = ResourceGameMode->GetClosestResourcePlaces(UnitBase);
	AWorkArea* NewResourcePlace = ResourceGameMode->GetSuitableWorkAreaToWorker(UnitBase->TeamId, WorkPlaces);

	//UE_LOG(LogTemp, Log, TEXT("NewResourcePlace: %s"), NewResourcePlace ? *NewResourcePlace->GetName() : TEXT("None"));
	if(UnitBase->ResourcePlace && NewResourcePlace && UnitBase->ResourcePlace->Type != NewResourcePlace->Type)
	{
		ResourceGameMode->AddCurrentWorkersForResourceType(UnitBase->TeamId, ConvertToResourceType(UnitBase->ResourcePlace->Type), -1.0f);
		ResourceGameMode->AddCurrentWorkersForResourceType(UnitBase->TeamId, ConvertToResourceType(NewResourcePlace->Type), +1.0f);
		UnitBase->ResourcePlace = NewResourcePlace;
	}else if(NewResourcePlace) //!UnitBase->ResourcePlace && 
	{
		ResourceGameMode->AddCurrentWorkersForResourceType(UnitBase->TeamId, ConvertToResourceType(NewResourcePlace->Type), +1.0f);
		UnitBase->ResourcePlace = NewResourcePlace;
	}

	UnitBase->SetUEPathfinding = true;
	UnitBase->SetUnitState(UnitData::GoToResourceExtraction);

	/*
	UE_LOG(LogTemp, Log, TEXT("2222 ABuildingBase::SwitchResourceArea - Unit %s (Team %d) current resource place: %s"), 
	*UnitBase->GetName(), 
	UnitBase->TeamId,
	UnitBase->ResourcePlace ? *UnitBase->ResourcePlace->GetName() : TEXT("None"));
	*/
}

bool ABuildingBase::SwitchBuildArea(AUnitBase* UnitBase, AResourceGameMode* ResourceGameMode)
{
	TArray<AWorkArea*> BuildAreas = ResourceGameMode->GetClosestBuildPlaces(UnitBase);

	if (BuildAreas.Num() > 3) BuildAreas.SetNum(3);
	
	UnitBase->BuildArea = ResourceGameMode->GetRandomClosestWorkArea(BuildAreas); // BuildAreas.Num() ? BuildAreas[0] : nullptr;

	bool CanAffordConstruction = false;

	if(UnitBase->BuildArea && UnitBase->BuildArea->IsPaid)
		CanAffordConstruction = true;
	else	
		CanAffordConstruction = UnitBase->BuildArea? ResourceGameMode->CanAffordConstruction(UnitBase->BuildArea->ConstructionCost, UnitBase->TeamId) : false; //Worker->BuildArea->CanAffordConstruction(Worker->TeamId, ResourceGameMode->NumberOfTeams,ResourceGameMode->TeamResources) : false;

	
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
		UnitBase->BuildArea->ControlTimer = 0.f;
		UnitBase->SetUEPathfinding = true;
		UnitBase->SetUnitState(UnitData::GoToBuild);
		return true;
	}
	
	return false;

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

