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
#include "Subsystems/ResourceVisualManager.h"
#include "MassEntityTypes.h"
#include "MassActorSubsystem.h"

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
	if (BuildArea)
	{
		BuildArea->RemoveWorkerFromArray(this);
		if (BuildArea->Workers.Num() == 0 && !BuildArea->StartedBuilding)
		{
			BuildArea->PlannedBuilding = false;
		}
	}
	
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
	DOREPLIFETIME(AWorkingUnitBase, CarryingResourceType);
}

void AWorkingUnitBase::OnRep_CarryingResourceType()
{
	if (GetNetMode() == NM_DedicatedServer) return;

	UResourceVisualManager* VisualManager = GetWorld()->GetSubsystem<UResourceVisualManager>();
	if (!VisualManager) return;

	UMassActorSubsystem* MassActorSubsystem = GetWorld()->GetSubsystem<UMassActorSubsystem>();
	if (!MassActorSubsystem) return;

	FMassEntityHandle EntityHandle = MassActorSubsystem->GetEntityHandleFromActor(this);
	if (!EntityHandle.IsValid()) return;

	if (CarryingResourceType != EResourceType::MAX)
	{
		TSubclassOf<AWorkResource> ResourceClass = nullptr;
		if (ResourcePlace)
		{
			ResourceClass = ResourcePlace->WorkResourceClass;
		}
		VisualManager->AssignResource(EntityHandle, CarryingResourceType, ResourceClass);
	}
	else
	{
		VisualManager->RemoveResource(EntityHandle);
	}
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
				const FVector UnitLoc = Unit->GetActorLocation();
				FVector UnitExtentBounds(100.f, 100.f, 100.f);

				if (UCapsuleComponent* Capsule = Unit->FindComponentByClass<UCapsuleComponent>())
				{
					const float R = Capsule->GetScaledCapsuleRadius();
					UnitExtentBounds.X = R;
					UnitExtentBounds.Y = R;
					UnitExtentBounds.Z = Capsule->GetScaledCapsuleHalfHeight();
				}

				const float AbsX = Unit->ExtensionOffset.X + UnitExtentBounds.X;
				const float AbsY = Unit->ExtensionOffset.Y + UnitExtentBounds.Y;
				const FVector2D Delta2D(SpawnLocation.X - UnitLoc.X, SpawnLocation.Y - UnitLoc.Y);
				const float MouseDist = Delta2D.Size();

				TargetLocation = UnitLoc;
				float DesiredYaw = 0.f;
				FVector Offset(0.f, 0.f, 0.f);

				switch (Unit->ExtensionSnapMethod)
				{
					case EExtensionSnapMethod::Snap8Way:
						{
							float AngleDeg = FMath::RadiansToDegrees(FMath::Atan2(Delta2D.Y, Delta2D.X));
							if (AngleDeg < 0) AngleDeg += 360.f;
							float SnappedAngle = FMath::RoundToFloat(AngleDeg / 45.f) * 45.f;
							DesiredYaw = (SnappedAngle >= 360.f) ? 0.f : SnappedAngle;
							
							float SnappedRad = FMath::DegreesToRadians(DesiredYaw);
							float CosA = FMath::Cos(SnappedRad), SinA = FMath::Sin(SnappedRad);
							float TargetX = (FMath::Abs(CosA) > 0.1f) ? (FMath::Sign(CosA) * AbsX) : 0.f;
							float TargetY = (FMath::Abs(SinA) > 0.1f) ? (FMath::Sign(SinA) * AbsY) : 0.f;

							float Dist = Unit->ExtensionMovementAllowed ? FMath::Min(MouseDist, FVector2D(TargetX, TargetY).Size()) : FVector2D(TargetX, TargetY).Size();
							Offset = FVector(CosA * Dist, SinA * Dist, 0.f);
						}
						break;

					case EExtensionSnapMethod::Snap4Way:
						if (FMath::Abs(Delta2D.X) >= FMath::Abs(Delta2D.Y)) {
							float SignX = (Delta2D.X >= 0.f) ? 1.f : -1.f;
							Offset.X = SignX * (Unit->ExtensionMovementAllowed ? FMath::Min(FMath::Abs(Delta2D.X), AbsX) : AbsX);
							DesiredYaw = (SignX > 0.f) ? 0.f : 180.f;
						} else {
							float SignY = (Delta2D.Y >= 0.f) ? 1.f : -1.f;
							Offset.Y = SignY * (Unit->ExtensionMovementAllowed ? FMath::Min(FMath::Abs(Delta2D.Y), AbsY) : AbsY);
							DesiredYaw = (SignY > 0.f) ? 90.f : 270.f;
						}
						break;

					case EExtensionSnapMethod::Snap2Way:
					case EExtensionSnapMethod::Snap1Way:
						{
							bool bIsXAxis = FMath::Abs(Unit->ExtensionOffset.X) >= FMath::Abs(Unit->ExtensionOffset.Y);
							if (bIsXAxis) {
								float SignX = (Unit->ExtensionSnapMethod == EExtensionSnapMethod::Snap1Way) 
									? FMath::Sign(Unit->ExtensionOffset.X) 
									: ((Delta2D.X >= 0.f) ? 1.f : -1.f);
								if (SignX == 0.f) SignX = 1.f;
								Offset.X = SignX * (Unit->ExtensionMovementAllowed ? FMath::Min(FMath::Abs(Delta2D.X), AbsX) : AbsX);
								DesiredYaw = (SignX > 0.f) ? 0.f : 180.f;
							} else {
								float SignY = (Unit->ExtensionSnapMethod == EExtensionSnapMethod::Snap1Way) 
									? FMath::Sign(Unit->ExtensionOffset.Y) 
									: ((Delta2D.Y >= 0.f) ? 1.f : -1.f);
								if (SignY == 0.f) SignY = 1.f;
								Offset.Y = SignY * (Unit->ExtensionMovementAllowed ? FMath::Min(FMath::Abs(Delta2D.Y), AbsY) : AbsY);
								DesiredYaw = (SignY > 0.f) ? 90.f : 270.f;
							}
						}
						break;

					case EExtensionSnapMethod::None:
					default:
						Offset = Unit->ExtensionOffset;
						DesiredYaw = 0.f;
						break;
				}
				TargetLocation += Offset;
				TargetRotation = FRotator(0.f, DesiredYaw, 0.f);

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
		// Special handling for Extension Areas with EExtensionSnapMethod::None: skip mouse jump
		bool bSkipMouseJump = false;
		if (ClientArea->IsExtensionArea)
		{
			ABuildingBase* Unit = Base;
			if (Unit && Unit->ExtensionSnapMethod == EExtensionSnapMethod::None)
			{
				bSkipMouseJump = true;
			}
		}

		if (!bSkipMouseJump)
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

			if (bHit)
			{
				ClientArea->SetActorLocation(HitResult.Location);
			}
		}
	}
}

void AWorkingUnitBase::SetCharacterVisibility(bool desiredVisibility)
{
	Super::SetCharacterVisibility(desiredVisibility);
	// WorkResource mesh visibility is handled in SyncAttachedAssetsVisibility called by UUnitVisibilityProcessor.
}

void AWorkingUnitBase::SyncAttachedAssetsVisibility()
{
	if (WorkResource && WorkResource->Mesh)
	{
		const bool bCarry = WorkResource->IsAttached;
		const bool bShow = bCarry && ComputeLocalVisibility();
		WorkResource->Mesh->SetVisibility(bShow, true);
		WorkResource->Mesh->SetHiddenInGame(!bShow);
	}
}
