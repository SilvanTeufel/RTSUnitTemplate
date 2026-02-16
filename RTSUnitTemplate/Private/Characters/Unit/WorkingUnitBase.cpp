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
#include "Actors/Projectile.h"
#include "Controller/AIController/WorkerUnitControllerBase.h"
#include "Controller/PlayerController/ExtendedControllerBase.h"
#include "Characters/Unit/BuildingBase.h"
#include "Characters/Unit/ConstructionUnit.h"
#include "Components/StaticMeshComponent.h"
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

void AWorkingUnitBase::Destroyed()
{
	if (WorkResource)
	{
		WorkResource->Destroy();
		WorkResource = nullptr;
	}
	Super::Destroyed();
}

void AWorkingUnitBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWorkingUnitBase, CurrentDraggedWorkArea);
	DOREPLIFETIME(AWorkingUnitBase, ResourcePlace);
	DOREPLIFETIME(AWorkingUnitBase, Base);
	DOREPLIFETIME(AWorkingUnitBase, BuildArea);
}

void AWorkingUnitBase::OnRep_CurrentDraggedWorkArea()
{
	if (CurrentDraggedWorkArea)
	{
		ShowWorkAreaIfNoFog_Implementation(CurrentDraggedWorkArea);
	}
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
			FVector End = Start + MouseDirection * 100000.f; // Extend to a maximum reasonable distance

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

			SpawnedWorkArea->ForceNetUpdate();
			ClientReceiveWorkArea(SpawnedWorkArea);
		}
	}
}

AWorkArea* AWorkingUnitBase::SpawnWorkAreaReplicated(TSubclassOf<AWorkArea> WorkAreaClass,
								   AWaypoint* Waypoint,
								   FVector SpawnLocation,
								   const FBuildingCost ConstructionCost,
								   bool IsPaid,
								   TSubclassOf<AUnitBase> ConstructionUnitClass,
								   bool IsExtensionArea) 
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
		FVector TargetLocation = SpawnLocation;
		FRotator TargetRotation = FRotator::ZeroRotator;

		if (IsExtensionArea)
		{
			ABuildingBase* Unit = Cast<ABuildingBase>(this);
			if (!Unit)
			{
				Unit = Base;
			}

			if (Unit)
			{
				const FVector UnitLoc = Unit->GetMassActorLocation();
				FVector UnitExtentBounds(100.f, 100.f, 100.f);

				if (UCapsuleComponent* Capsule = Unit->FindComponentByClass<UCapsuleComponent>())
				{
					const float R = Capsule->GetScaledCapsuleRadius();
					UnitExtentBounds.X = R;
					UnitExtentBounds.Y = R;
					UnitExtentBounds.Z = Capsule->GetScaledCapsuleHalfHeight();
				}

				if (Unit->ExtensionDominantSideSelection)
				{
					const FVector2D Delta2D(SpawnLocation.X - UnitLoc.X, SpawnLocation.Y - UnitLoc.Y);
					const float AbsX = Unit->ExtensionOffset.X + UnitExtentBounds.X;
					const float AbsY = Unit->ExtensionOffset.Y + UnitExtentBounds.Y;

					TargetLocation = UnitLoc;
					float DesiredYaw = 0.f;
					if (FMath::Abs(Delta2D.X) >= FMath::Abs(Delta2D.Y))
					{
						const float SignX = (Delta2D.X >= 0.f) ? 1.f : -1.f;
						TargetLocation.X += SignX * AbsX;
						DesiredYaw = (SignX > 0.f) ? 0.f : 180.f;
					}
					else
					{
						const float SignY = (Delta2D.Y >= 0.f) ? 1.f : -1.f;
						TargetLocation.Y += SignY * AbsY;
						DesiredYaw = (SignY > 0.f) ? 90.f : 270.f;
					}
					TargetRotation = FRotator(0.f, DesiredYaw, 0.f);
				}
				else
				{
					TargetLocation = UnitLoc;
					TargetLocation.X += Unit->ExtensionOffset.X;
					TargetLocation.Y += Unit->ExtensionOffset.Y;
				}

				if (Unit->ExtensionGroundTrace)
				{
					const FVector TraceStart = TargetLocation + FVector(0, 0, 2000.f);
					const FVector TraceEnd = TargetLocation - FVector(0, 0, 2000.f);
					FCollisionQueryParams Params(SCENE_QUERY_STAT(SpawnExtensionAreaGround), true);
					Params.AddIgnoredActor(Unit);

					FHitResult GroundHit;
					bool bFoundValidGround = false;
					const int32 MaxTries = 8;
					for (int32 Try = 0; Try < MaxTries; ++Try)
					{
						if (!GetWorld()->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility, Params))
						{
							break;
						}
						AActor* HitActor = GroundHit.GetActor();
						if (HitActor && (HitActor->IsA(AWorkArea::StaticClass()) || HitActor->IsA(ABuildingBase::StaticClass())))
						{
							Params.AddIgnoredActor(HitActor);
							continue;
						}
						bFoundValidGround = true;
						break;
					}
					if (bFoundValidGround)
					{
						TargetLocation.Z = GroundHit.Location.Z;
					}
				}
				else
				{
					TargetLocation.Z = UnitLoc.Z + Unit->ExtensionOffset.Z + UnitExtentBounds.Z;
				}

				// Adjust Z for mesh bottom
				if (AWorkArea* DefaultWorkArea = WorkAreaClass->GetDefaultObject<AWorkArea>())
				{
					if (UStaticMeshComponent* MeshComp = DefaultWorkArea->FindComponentByClass<UStaticMeshComponent>())
					{
						const FBoxSphereBounds Bounds = MeshComp->CalcBounds(MeshComp->GetRelativeTransform());
						const float RelativeBottomZ = Bounds.Origin.Z - Bounds.BoxExtent.Z;
						const float Clearance = 2.f;
						TargetLocation.Z += (Clearance - RelativeBottomZ);
					}
				}
			}
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;


		// Spawn the replicated WorkArea on the server
		AWorkArea* SpawnedWorkArea = GetWorld()->SpawnActor<AWorkArea>(
			WorkAreaClass,
			TargetLocation,
			TargetRotation,
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
			SpawnedWorkArea->IsExtensionArea = IsExtensionArea;

			if (SpawnedWorkArea->IsExtensionArea)
			{
				SpawnedWorkArea->AllowAddingWorkers = false;
				SpawnedWorkArea->ServerMeshRotationBuilding = TargetRotation;
			}
			
			CurrentDraggedWorkArea = SpawnedWorkArea;
			//CurrentDraggedWorkArea->SetReplicateMovement(true);

			SpawnedWorkArea->ForceNetUpdate();
			ClientReceiveWorkArea(SpawnedWorkArea);

			// Store optional construction site class
			if (CurrentDraggedWorkArea && ConstructionUnitClass)
			{
				CurrentDraggedWorkArea->ConstructionUnitClass = ConstructionUnitClass;
				CurrentDraggedWorkArea->Origin = this;

				if (AConstructionUnit* ConstructionCDO = Cast<AConstructionUnit>(ConstructionUnitClass->GetDefaultObject()))
				{
					if (ConstructionCDO->SetOffsetsDueToWorkAreaBounds && CurrentDraggedWorkArea->Mesh)
					{
						const FBoxSphereBounds MeshBounds = CurrentDraggedWorkArea->Mesh->CalcBounds(CurrentDraggedWorkArea->Mesh->GetRelativeTransform());
						ConstructionCDO->DefaultOscOffsetA.Z = MeshBounds.Origin.Z - MeshBounds.BoxExtent.Z;
						ConstructionCDO->DefaultOscOffsetB.Z = MeshBounds.Origin.Z + MeshBounds.BoxExtent.Z;
					}
				}
			}
			
			return CurrentDraggedWorkArea;
		}
	}

	return nullptr;
}


void AWorkingUnitBase::ClientReceiveWorkArea_Implementation(AWorkArea* ClientArea)
{
	if (!ClientArea)
	{
		UE_LOG(LogTemp, Warning, TEXT("ClientReceiveWorkArea: ClientArea is NULL!"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("ClientReceiveWorkArea: Successfully received replicated WorkArea: %s"), *ClientArea->GetName());
	
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
		FVector End = Start + MouseDirection * 1000000.f; // Extend to a maximum reasonable distance

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

void AWorkingUnitBase::SetCharacterVisibility(bool desiredVisibility)
{
	Super::SetCharacterVisibility(desiredVisibility);
	
	if (WorkResource)
	{
		// Only show the resource if the unit is visible AND the resource is supposed to be active (attached)
		bool bShouldShowResource = desiredVisibility && WorkResource->IsAttached;
		WorkResource->SetActorHiddenInGame(!bShouldShowResource);
	}
}
