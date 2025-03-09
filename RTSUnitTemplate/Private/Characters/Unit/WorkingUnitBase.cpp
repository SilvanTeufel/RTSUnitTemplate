// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
// WorkingUnitBase.h (Corresponding Header)
#include "Characters/Unit/WorkingUnitBase.h"

// Engine Headers
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NavCollision.h"

// Project-Specific Headers
#include "GAS/AttributeSetBase.h"
#include "Widgets/UnitBaseHealthBar.h"
#include "Actors/SelectedIcon.h"
#include "Actors/Projectile.h"
#include "Controller/AIController/WorkerUnitControllerBase.h"
#include "Controller/PlayerController/ExtendedControllerBase.h"
#include "GameModes/ResourceGameMode.h"
#include "Net/UnrealNetwork.h"

void AWorkingUnitBase::BeginPlay()
{
	Super::BeginPlay();

	AResourceGameMode* GameMode = Cast<AResourceGameMode>(GetWorld()->GetAuthGameMode());

	if (GameMode)
	{
		AWorkerUnitControllerBase* WorkerController = Cast<AWorkerUnitControllerBase>(GetController());
		if (WorkerController)
		{
			GameMode->AssignWorkAreasToWorker(this);
		}
	}
	
}

void AWorkingUnitBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWorkingUnitBase, CurrentDraggedWorkArea);
	DOREPLIFETIME(AWorkingUnitBase, ResourcePlace);
	DOREPLIFETIME(AWorkingUnitBase, Base);
	DOREPLIFETIME(AWorkingUnitBase, BuildArea);
}


void AWorkingUnitBase::SpawnWorkArea_Implementation(TSubclassOf<AWorkArea> WorkAreaClass, AWaypoint* Waypoint)
{
		if (!OwningPlayerController)
		{
			UE_LOG(LogTemp, Error, TEXT("No OwningPlayerController"));
			return;
		}
	
		AExtendedControllerBase* ExtendedControllerBase = Cast<AExtendedControllerBase>(OwningPlayerController);
	
		if (!ExtendedControllerBase)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to get owning player controller."));
			return;
		}


		if (WorkAreaClass && !CurrentDraggedWorkArea && ExtendedControllerBase->SelectableTeamId == TeamId) // ExtendedControllerBase->CurrentDraggedGround == nullptr &&
		{
			FVector MousePosition, MouseDirection;
			ExtendedControllerBase->DeprojectMousePositionToWorld(MousePosition, MouseDirection);

			// Raycast from the mouse position into the scene to find the ground
			FVector Start = MousePosition;
			FVector End = Start + MouseDirection * 5000.f; // Extend to a maximum reasonable distance

			FHitResult HitResult;
			FCollisionQueryParams CollisionParams;
			CollisionParams.bTraceComplex = true; // Use complex collision for precise tracing

			// Perform the raycast
			bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, CollisionParams);
	
			FVector SpawnLocation = HitResult.Location; // Assuming you want to use HitResult location as spawn point
			FRotator SpawnRotation = FRotator::ZeroRotator;
			FActorSpawnParameters SpawnParams;
			//SpawnParams.Owner = this;
			SpawnParams.Instigator = ExtendedControllerBase->GetPawn(); // Assuming we want to set the pawn that is responsible for spawning
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	    
			AWorkArea* SpawnedWorkArea = GetWorld()->SpawnActor<AWorkArea>(WorkAreaClass, SpawnLocation, SpawnRotation, SpawnParams);
			if (SpawnedWorkArea)
			{
			
				if(Waypoint) SpawnedWorkArea->NextWaypoint = Waypoint;
				SpawnedWorkArea->TeamId = TeamId;
				CurrentDraggedWorkArea = SpawnedWorkArea;
				//BuildArea = SpawnedWorkArea;
			
			}
			
		}
}

void AWorkingUnitBase::ServerSpawnWorkArea_Implementation(TSubclassOf<AWorkArea> WorkAreaClass, AWaypoint* Waypoint, FVector HitLocation)
{

	if (HasAuthority()) // Make sure the server is executing this code
	{
		FRotator SpawnRotation = FRotator::ZeroRotator;
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		// Spawn the WorkArea on the server
		AWorkArea* SpawnedWorkArea = GetWorld()->SpawnActor<AWorkArea>(WorkAreaClass, HitLocation, SpawnRotation, SpawnParams);
		if (SpawnedWorkArea)
		{
			if (Waypoint)
			{
				SpawnedWorkArea->NextWaypoint = Waypoint;
			}
			SpawnedWorkArea->TeamId = TeamId;
			CurrentDraggedWorkArea = SpawnedWorkArea;
		}
	}
}

void AWorkingUnitBase::SpawnWorkAreaReplicated(TSubclassOf<AWorkArea> WorkAreaClass,
											   AWaypoint* Waypoint,
											   FVector SpawnLocation,
											   const FBuildingCost ConstructionCost,
											   bool IsPaid)
{
	
 // && !CurrentDraggedWorkArea
	if (CurrentDraggedWorkArea){
		CurrentDraggedWorkArea->PlannedBuilding = true;
		CurrentDraggedWorkArea->ControlTimer = 0.f;
		CurrentDraggedWorkArea->RemoveAreaFromGroup();
		CurrentDraggedWorkArea->Destroy();
		CurrentDraggedWorkArea = nullptr;
	}
	
	if (WorkAreaClass)
	{


		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		FRotator SpawnRotation = FRotator::ZeroRotator;

		// Spawn the replicated WorkArea on the server
		AWorkArea* SpawnedWorkArea = GetWorld()->SpawnActor<AWorkArea>(
			WorkAreaClass,
			SpawnLocation,
			SpawnRotation,
			SpawnParams
		);

		if (SpawnedWorkArea)
		{
			// Initialize any properties on the spawned WorkArea
			if (Waypoint)
			{
				SpawnedWorkArea->NextWaypoint = Waypoint;
			}
			SpawnedWorkArea->TeamId          = TeamId;
			SpawnedWorkArea->IsPaid          = IsPaid;
			SpawnedWorkArea->ConstructionCost = ConstructionCost;
			//SpawnedWorkArea->SceneRoot->SetVisibility(false, true);
			// Keep track of this WorkArea if needed
			CurrentDraggedWorkArea = SpawnedWorkArea;
			CurrentDraggedWorkArea->SetReplicateMovement(true);
			// Call a Multicast to hide the mesh on all clients
			// (this call only makes sense if we are on the server, which we are).

			// Create a delegate to call ShowWorkArea with our SpawnedWorkArea as a parameter
			/*
			FTimerDelegate TimerDelegate;
			TimerDelegate.BindUFunction(this, FName("ShowWorkAreaIfNoFog"), CurrentDraggedWorkArea);
			
		
			// Schedule the timer (on the server)
			GetWorldTimerManager().SetTimer(
				ShowWorkAreaTimerHandle,  // Our stored FTimerHandle
				TimerDelegate,
				0.01,
				false                     // Don't loop, just once
			);
			*/
		}
	}
}

/*
void AWorkingUnitBase::SpawnWorkAreaReplicated(TSubclassOf<AWorkArea> WorkAreaClass, AWaypoint* Waypoint, FVector SpawnLocation, const  FBuildingCost ConstructionCost, bool IsPaid)
{
		if (WorkAreaClass && !CurrentDraggedWorkArea) // ExtendedControllerBase->CurrentDraggedGround == nullptr &&
		{

			//FVector SpawnLocation = GetActorLocation()+FVector(0.f, 0.f, 500.f); // Assuming you want to use HitResult location as spawn point
			FRotator SpawnRotation = FRotator::ZeroRotator;
			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = this;
			// Assuming we want to set the pawn that is responsible for spawning
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	    
			AWorkArea* SpawnedWorkArea = GetWorld()->SpawnActor<AWorkArea>(WorkAreaClass, SpawnLocation, SpawnRotation, SpawnParams);

			if (SpawnedWorkArea)
			{
			
				if(Waypoint) SpawnedWorkArea->NextWaypoint = Waypoint;
				SpawnedWorkArea->TeamId = TeamId;
				SpawnedWorkArea->IsPaid = IsPaid;
				SpawnedWorkArea->ConstructionCost = ConstructionCost;
				CurrentDraggedWorkArea = SpawnedWorkArea;
				//BuildArea = SpawnedWorkArea;
				//ClientReceiveWorkArea_Implementation(CurrentDraggedWorkArea);
			}
			
		}
}
*/
void AWorkingUnitBase::ClientReceiveWorkArea_Implementation(AWorkArea* ClientArea)
{
	if (!OwningPlayerController)
	{
		UE_LOG(LogTemp, Error, TEXT("No OwningPlayerController"));
		return;
	}
	
	AExtendedControllerBase* ExtendedControllerBase = Cast<AExtendedControllerBase>(OwningPlayerController);
	
	if (!ExtendedControllerBase)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to get owning player controller."));
		return;
	}

	if( ExtendedControllerBase->SelectableTeamId == TeamId)
	{
		FVector MousePosition, MouseDirection;
		ExtendedControllerBase->DeprojectMousePositionToWorld(MousePosition, MouseDirection);

		// Raycast from the mouse position into the scene to find the ground
		FVector Start = MousePosition;
		FVector End = Start + MouseDirection * 5000.f; // Extend to a maximum reasonable distance

		FHitResult HitResult;
		FCollisionQueryParams CollisionParams;
		CollisionParams.bTraceComplex = true; // Use complex collision for precise tracing

		// Perform the raycast
		bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, CollisionParams);

		if(bHit)
		{
			ClientArea->SetActorLocation(HitResult.Location);
		}
	}
}
