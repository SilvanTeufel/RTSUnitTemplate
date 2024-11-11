// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Characters/Unit/WorkingUnitBase.h"
#include "GAS/AttributeSetBase.h"
#include "AbilitySystemComponent.h"
#include "NavCollision.h"
#include "Widgets/UnitBaseHealthBar.h"
#include "Components/CapsuleComponent.h"
#include "Actors/SelectedIcon.h"
#include "Actors/Projectile.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SkeletalMeshComponent.h"
#include "Controller/AIController/WorkerUnitControllerBase.h"
#include "Controller/PlayerController/ExtendedControllerBase.h"
#include "GameFramework/CharacterMovementComponent.h"
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


void AWorkingUnitBase::SpawnWorkAreaReplicated(TSubclassOf<AWorkArea> WorkAreaClass, AWaypoint* Waypoint, bool IsPaid)
{
		if (WorkAreaClass && !CurrentDraggedWorkArea) // ExtendedControllerBase->CurrentDraggedGround == nullptr &&
		{

			FVector SpawnLocation = GetActorLocation()+FVector(0.f, 0.f, 500.f); // Assuming you want to use HitResult location as spawn point
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
				CurrentDraggedWorkArea = SpawnedWorkArea;
				//BuildArea = SpawnedWorkArea;
				//ClientReceiveWorkArea_Implementation(CurrentDraggedWorkArea);
			}
			
		}
}

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
