// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Characters/Unit/BuildingBase.h"

#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "GameModes/ResourceGameMode.h"
#include "Components/CapsuleComponent.h"
#include "Controller/PlayerController/CustomControllerBase.h"


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
	CanMove = false;
}

void ABuildingBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	// In your actor's constructor or initialization method
	static float LogTimer = 0.0f;
	LogTimer += DeltaTime;
	
	if (LogTimer >= 2.0f && GetWorld()->GetNetMode() == NM_Client)
	{
		LogTimer = 0.0f;
		
		// Get controller info
		APlayerController* PC = GetWorld()->GetFirstPlayerController();
		AControllerBase* ControllerBase = PC ? Cast<AControllerBase>(PC) : nullptr;
		ACustomControllerBase* CustomController = PC ? Cast<ACustomControllerBase>(PC) : nullptr;
		
		UE_LOG(LogTemp, Warning, TEXT("=== Building %s Visibility Debug ==="), *GetName());
		UE_LOG(LogTemp, Warning, TEXT("  TeamId: %d"), TeamId);
		UE_LOG(LogTemp, Warning, TEXT("  CanMove: %s"), CanMove ? TEXT("TRUE") : TEXT("FALSE"));
		UE_LOG(LogTemp, Warning, TEXT("  IsMyTeam: %s"), IsMyTeam ? TEXT("TRUE") : TEXT("FALSE"));
		UE_LOG(LogTemp, Warning, TEXT("  IsVisibleEnemy: %s"), IsVisibleEnemy ? TEXT("TRUE") : TEXT("FALSE"));
		UE_LOG(LogTemp, Warning, TEXT("  IsOnViewport: %s"), IsOnViewport ? TEXT("TRUE") : TEXT("FALSE"));
		UE_LOG(LogTemp, Warning, TEXT("  EnableFog: %s"), EnableFog ? TEXT("TRUE") : TEXT("FALSE"));
		UE_LOG(LogTemp, Warning, TEXT("  StopVisibilityTick: %s"), StopVisibilityTick ? TEXT("TRUE") : TEXT("FALSE"));
		UE_LOG(LogTemp, Warning, TEXT("  bClientIsVisible: %s"), bClientIsVisible ? TEXT("TRUE") : TEXT("FALSE"));
		
		// Controller info
		if (PC)
		{
			UE_LOG(LogTemp, Warning, TEXT("  PlayerController: Valid (%s)"), *PC->GetName());
			
			if (ControllerBase)
			{
				UE_LOG(LogTemp, Warning, TEXT("  ControllerBase Cast: SUCCESS"));
				UE_LOG(LogTemp, Warning, TEXT("  SelectableTeamId: %d"), ControllerBase->SelectableTeamId);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("  ControllerBase Cast: FAILED"));
			}
			
			if (CustomController)
			{
				UE_LOG(LogTemp, Warning, TEXT("  CustomControllerBase Cast: SUCCESS"));
				UE_LOG(LogTemp, Warning, TEXT("  SelectableTeamId: %d"), CustomController->SelectableTeamId);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("  CustomControllerBase Cast: FAILED"));
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("  PlayerController: NULL"));
		}
		
		// Mesh visibility
		if (GetMesh())
		{
			UE_LOG(LogTemp, Warning, TEXT("  Mesh Visibility: %s"), GetMesh()->IsVisible() ? TEXT("TRUE") : TEXT("FALSE"));
			UE_LOG(LogTemp, Warning, TEXT("  Mesh HiddenInGame: %s"), GetMesh()->bHiddenInGame ? TEXT("TRUE") : TEXT("FALSE"));
		}
		
		if (ISMComponent)
		{
			UE_LOG(LogTemp, Warning, TEXT("  ISM Visibility: %s"), ISMComponent->IsVisible() ? TEXT("TRUE") : TEXT("FALSE"));
			UE_LOG(LogTemp, Warning, TEXT("  ISM HiddenInGame: %s"), ISMComponent->bHiddenInGame ? TEXT("TRUE") : TEXT("FALSE"));
		}
		
		// Network info
		UE_LOG(LogTemp, Warning, TEXT("  NetMode: %s"), 
			GetWorld()->GetNetMode() == NM_Client ? TEXT("Client") :
			GetWorld()->GetNetMode() == NM_ListenServer ? TEXT("ListenServer") :
			GetWorld()->GetNetMode() == NM_DedicatedServer ? TEXT("DedicatedServer") :
			TEXT("Standalone"));
		
		UE_LOG(LogTemp, Warning, TEXT("  HasAuthority: %s"), HasAuthority() ? TEXT("TRUE") : TEXT("FALSE"));
		UE_LOG(LogTemp, Warning, TEXT("  IsLocallyControlled: %s"), IsLocallyControlled() ? TEXT("TRUE") : TEXT("FALSE"));
		UE_LOG(LogTemp, Warning, TEXT("====================================="));
	}

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

void ABuildingBase::SwitchResourceArea(AUnitBase* UnitBase, AResourceGameMode* ResourceGameMode, int32 RecursionCount)
{
	// Log initial state

	if (!ResourceGameMode) return;

	TArray<AWorkArea*> WorkPlaces = ResourceGameMode->GetClosestResourcePlaces(UnitBase);
	
	AWorkArea* NewResourcePlace = ResourceGameMode->GetSuitableWorkAreaToWorker(UnitBase->TeamId, WorkPlaces);

	if (IsValid(NewResourcePlace))
	{
		if(IsValid(UnitBase->ResourcePlace) && UnitBase->ResourcePlace->Type != NewResourcePlace->Type)
		{
			ResourceGameMode->AddCurrentWorkersForResourceType(UnitBase->TeamId, ConvertToResourceType(UnitBase->ResourcePlace->Type), -1.0f);
			ResourceGameMode->AddCurrentWorkersForResourceType(UnitBase->TeamId, ConvertToResourceType(NewResourcePlace->Type), +1.0f);
		}
		else if(!IsValid(UnitBase->ResourcePlace))
		{
			ResourceGameMode->AddCurrentWorkersForResourceType(UnitBase->TeamId, ConvertToResourceType(NewResourcePlace->Type), +1.0f);
		}
		UnitBase->ResourcePlace = NewResourcePlace;
	}
	else if (!IsValid(UnitBase->ResourcePlace))
	{
		NewResourcePlace = ResourceGameMode->GetRandomClosestWorkArea(WorkPlaces);
		
		if (NewResourcePlace)
		{
			if(IsValid(UnitBase->ResourcePlace) && UnitBase->ResourcePlace->Type != NewResourcePlace->Type)
			{
				ResourceGameMode->AddCurrentWorkersForResourceType(UnitBase->TeamId, ConvertToResourceType(UnitBase->ResourcePlace->Type), -1.0f);
				ResourceGameMode->AddCurrentWorkersForResourceType(UnitBase->TeamId, ConvertToResourceType(NewResourcePlace->Type), +1.0f);
			}
			else if(!IsValid(UnitBase->ResourcePlace))
			{
				ResourceGameMode->AddCurrentWorkersForResourceType(UnitBase->TeamId, ConvertToResourceType(NewResourcePlace->Type), +1.0f);
			}
			UnitBase->ResourcePlace = NewResourcePlace;
		}else
		{
			//UE_LOG(LogTemp, Warning, TEXT("No WorkAreas, set to Idle!"));
			UnitBase->SetUEPathfinding = true;
			UnitBase->SetUnitState(UnitData::Idle);
			return;
		}
	}

	UnitBase->SetUEPathfinding = true;
	UnitBase->SetUnitState(UnitData::GoToResourceExtraction);
	UnitBase->SwitchEntityTagByState(UnitData::GoToResourceExtraction, UnitBase->UnitStatePlaceholder);
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
		UnitBase->SwitchEntityTagByState(UnitData::GoToBuild, UnitBase->UnitStatePlaceholder);
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
	}
}

void ABuildingBase::MulticastSetEnemyVisibility_Implementation(APerformanceUnit* DetectingActor, bool bVisible)
{
	if (!CanMove && bVisible == false) return;
	Super::MulticastSetEnemyVisibility_Implementation(DetectingActor, bVisible);

	/*
	if (!CanMove && bVisible == false) return;
	if (IsMyTeam) return;
	if (IsVisibleEnemy == bVisible) return;
	
	UWorld* World = GetWorld();
	if (!World) return;  // Safety check

	if (!OwningPlayerController)
	{
		APlayerController* PlayerController = World->GetFirstPlayerController();
		if (PlayerController) OwningPlayerController = PlayerController;
	}

	if (OwningPlayerController)
		if (ACustomControllerBase* MyController = Cast<ACustomControllerBase>(OwningPlayerController))
		{
			if (MyController->IsValidLowLevel() && MyController->SelectableTeamId == DetectingActor->TeamId)
			{
				IsVisibleEnemy = bVisible;
			}
		}
	*/
}

