// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Controller/WorkerUnitControllerBase.h"
#include "Landscape.h"
#include "Characters/Unit/UnitBase.h"
#include "Actors/Waypoint.h"
#include "Actors/WorkResource.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameSession.h"
#include "Kismet/KismetMathLibrary.h"
#include "Runtime/Engine/Classes/Kismet/GameplayStatics.h"
#include "Runtime/Engine/Classes/GameFramework/Controller.h"
#include "Perception/AiPerceptionComponent.h"
#include "Perception/AiSenseConfig_Sight.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "NavigationSystem.h"
#include "Characters/Unit/BuildingBase.h"
#include "Controller/ControllerBase.h"
#include "GameModes/ResourceGameMode.h"
#include "Net/UnrealNetwork.h"

void AWorkerUnitControllerBase::Tick(float DeltaSeconds)
{
	//Super::Tick(DeltaSeconds);
	WorkingUnitControlStateMachine(DeltaSeconds);
}

void AWorkerUnitControllerBase::WorkingUnitControlStateMachine(float DeltaSeconds)
{
		AUnitBase* UnitBase = Cast<AUnitBase>(GetPawn());
		//UE_LOG(LogTemp, Warning, TEXT("Controller UnitBase->Attributes! %f"), UnitBase->Attributes->GetAttackDamage());
		if(!UnitBase) return;
	
		switch (UnitBase->UnitState)
		{
		case UnitData::None:
		{
			//if(!UnitBase->IsFriendly)UE_LOG(LogTemp, Warning, TEXT("None"));
		}
		break;
		case UnitData::GoToResourceExtraction:
			{
				//UE_LOG(LogTemp, Warning, TEXT("GoToResourceExtraction"));
				GoToResourceExtraction(UnitBase, DeltaSeconds);
				//if(!UnitBase->IsFriendly)UE_LOG(LogTemp, Warning, TEXT("None"));
			}
		break;
		case UnitData::ResourceExtraction:
			{
				//UE_LOG(LogTemp, Warning, TEXT("ResourceExtraction"));
				ResourceExtraction(UnitBase, DeltaSeconds);
				//if(!UnitBase->IsFriendly)UE_LOG(LogTemp, Warning, TEXT("None"));
			}
		break;
		case UnitData::GoToBase:
			{
				//UE_LOG(LogTemp, Warning, TEXT("GoToBase"));
				GoToBase(UnitBase, DeltaSeconds);
				//if(!UnitBase->IsFriendly)UE_LOG(LogTemp, Warning, TEXT("None"));
			}
		break;
		case UnitData::GoToBuild:
			{
				//UE_LOG(LogTemp, Warning, TEXT("GoToBuild"));
				GoToBuild(UnitBase, DeltaSeconds);
				//if(!UnitBase->IsFriendly)UE_LOG(LogTemp, Warning, TEXT("None"));
			}
		break;
		case UnitData::Build:
			{
				//UE_LOG(LogTemp, Warning, TEXT("Build"));
				Build(UnitBase, DeltaSeconds);
				//if(!UnitBase->IsFriendly)UE_LOG(LogTemp, Warning, TEXT("None"));
			}
		break;
		case UnitData::Dead:
		{
			//if(UnitBase->TeamId == 2)UE_LOG(LogTemp, Warning, TEXT("Dead"));
				
			if(UnitBase && UnitBase->BuildArea && !UnitBase->BuildArea->Building)
			{
				UnitBase->BuildArea->PlannedBuilding = false;
				UnitBase->BuildArea->StartedBuilding = false;
			}

				Dead(UnitBase, DeltaSeconds);
		}
		break;
		case UnitData::Patrol:
		{
			//if(UnitBase->TeamId == 3)UE_LOG(LogTemp, Warning, TEXT("Patrol"));
				
			if(UnitBase->UsingUEPathfindingPatrol)
				PatrolUEPathfinding(UnitBase, DeltaSeconds);
			else
				Patrol(UnitBase, DeltaSeconds);
		}
		break;
		case UnitData::PatrolRandom:
			{
				//if(UnitBase->TeamId == 3)UE_LOG(LogTemp, Warning, TEXT("PatrolRandom"));
				if(UnitBase->SetNextUnitToChase())
				{
					UnitBase->SetUnitState(UnitData::Chase);
				}else
				{
					UnitBase->SetWalkSpeed(UnitBase->Attributes->GetRunSpeed());
					UnitBase->UnitControlTimer = 0.f;
					UnitBase->SetUEPathfinding = true;
					UnitBase->SetUnitState(UnitData::GoToBase);
					//SetUEPathfindingRandomLocation(UnitBase, DeltaSeconds);
				}

			}
			break;
		case UnitData::PatrolIdle:
			{
				//if(UnitBase->TeamId == 3)UE_LOG(LogTemp, Warning, TEXT("PatrolIdle"));
				if(UnitBase->SetNextUnitToChase())
				{
					UnitBase->SetUnitState(UnitData::Chase);
				}else
				{
					UnitBase->UnitControlTimer = (UnitBase->UnitControlTimer + DeltaSeconds);

					UnitBase->SetWalkSpeed(0);
					if(UnitBase->UnitControlTimer > UnitBase->NextWaypoint->RandomTime)
					{
						UnitBase->UnitControlTimer = 0.f;
						UnitBase->SetUEPathfinding = true;
						UnitBase->SetUnitState(UnitData::GoToBase);
					}
				}
			}
			break;
		case UnitData::Run:
		{
			//if(UnitBase->TeamId == 3)UE_LOG(LogTemp, Warning, TEXT("Run"));
				
			if(UnitBase->UEPathfindingUsed)
				RunUEPathfinding(UnitBase, DeltaSeconds);
			else
				Run(UnitBase, DeltaSeconds);
		}
		break;
		case UnitData::Chase:
		{
			//if(UnitBase->TeamId == 3)UE_LOG(LogTemp, Warning, TEXT("Chase"));
				
			Chase(UnitBase, DeltaSeconds);
		}
		break;
		case UnitData::Attack:
		{
			//if(UnitBase->TeamId == 3)UE_LOG(LogTemp, Warning, TEXT("Attack"));
			Attack(UnitBase, DeltaSeconds);
		}
		break;
		case UnitData::Pause:
		{
			//if(UnitBase->TeamId == 3)UE_LOG(LogTemp, Warning, TEXT("Pause"));
			Pause(UnitBase, DeltaSeconds);
		}
		break;

		case UnitData::IsAttacked:
		{
			//if(UnitBase->TeamId == 3)UE_LOG(LogTemp, Warning, TEXT("Pause"));
			//if(UnitBase->TeamId == 1)UE_LOG(LogTemp, Warning, TEXT("IsAttacked"));
			IsAttacked(UnitBase, DeltaSeconds);
		}
		break;
		case UnitData::EvasionChase:
		case UnitData::Evasion:
		{
			//if(UnitBase->TeamId == 3)UE_LOG(LogTemp, Warning, TEXT("Evasion"));

				
			
			if(UnitBase->UnitControlTimer >= 7.f)
			{
		
				//UnitBase->StartAcceleratingTowardsDestination(UnitBase->GetActorLocation()+UnitBase->GetActorForwardVector()*350.f, FVector(5000.f, 5000.f, 0.f), 25.f, 350.f);
				UnitBase->CollisionUnit = nullptr;
				FVector NewLocation = UnitBase->GetActorLocation()+UnitBase->GetActorForwardVector()*300.f;
				NewLocation.Z = UnitBase->GetActorLocation().Z;
				UnitBase->UnitControlTimer = 0.f;
				UnitBase->TeleportToValidLocation(NewLocation);
				UnitBase->SetUEPathfinding = true;
				UnitBase->SetUnitState(UnitBase->UnitStatePlaceholder);
	
			}else if(UnitBase->CollisionUnit)
			{
				EvasionWorker(UnitBase, UnitBase->CollisionUnit->GetActorLocation());
				UnitBase->UnitControlTimer += DeltaSeconds;
			}
				
		}
		break;
		case UnitData::Idle:
		{
			//if(UnitBase->TeamId == 3)UE_LOG(LogTemp, Warning, TEXT("Idle"));
			Idle(UnitBase, DeltaSeconds);
		}
		break;
		default:
		{
			//UE_LOG(LogTemp, Warning, TEXT("default Idle"));
			UnitBase->SetUnitState(UnitData::Idle);
		}
		break;
		}

	if (UnitBase->Attributes->GetHealth() <= 0.f && UnitBase->GetUnitState() != UnitData::Dead) {
		DetachWorkResource(UnitBase->WorkResource);
		KillUnitBase(UnitBase);
		
		if(UnitBase && UnitBase->BuildArea && !UnitBase->BuildArea->Building)
		{
			UnitBase->BuildArea->PlannedBuilding = false;
			UnitBase->BuildArea->StartedBuilding = false;
		}
		UnitBase->UnitControlTimer = 0.f;
	}
}

void AWorkerUnitControllerBase::EvasionWorker(AUnitBase* UnitBase, FVector CollisionLocation)
{
	
	UnitBase->SetWalkSpeed(UnitBase->Attributes->GetRunSpeed());
				
	const FVector UnitLocation = UnitBase->GetActorLocation();
				
	if(UnitBase->IsFlying)
	{
		CollisionLocation = FVector(CollisionLocation.X, CollisionLocation.Y, UnitBase->FlyHeight);
	}
	
	const FVector ADirection = UKismetMathLibrary::GetDirectionUnitVector(UnitLocation, CollisionLocation);
	const FVector RotatedDirection = FRotator(0.f,60.f,0.f).RotateVector(-1*ADirection);
	UnitBase->AddMovementInput(RotatedDirection, UnitBase->Attributes->GetRunSpeedScale());

	const float Distance = sqrt((UnitLocation.X-CollisionLocation.X)*(UnitLocation.X-CollisionLocation.X)+(UnitLocation.Y-CollisionLocation.Y)*(UnitLocation.Y-CollisionLocation.Y));

	if (Distance >= UnitBase->EvadeDistance) {
		UnitBase->SetUEPathfinding = true;
		UnitBase->SetUnitState(UnitBase->UnitStatePlaceholder);
		UnitBase->CollisionUnit = nullptr;
		UnitBase->UnitControlTimer = 0.f;
	}
}

void AWorkerUnitControllerBase::GoToResourceExtraction(AUnitBase* UnitBase, float DeltaSeconds)
{
	if(!UnitBase || !UnitBase->ResourcePlace) return;

	UnitBase->SetWalkSpeed(UnitBase->Attributes->GetRunSpeed());
	if(UnitBase->CollisionUnit && UnitBase->CollisionUnit->TeamId == UnitBase->TeamId && UnitBase->CollisionUnit->GetUnitState() != UnitData::Dead)
	{
		UnitBase->SetUnitState(UnitData::Evasion);
		UnitBase->UnitStatePlaceholder = UnitData::GoToResourceExtraction;
		return;
	}
	
	UnitBase->UnitControlTimer+= DeltaSeconds;
	if(UnitBase->UnitControlTimer > ResetPathfindingTime)
	{
		UnitBase->UnitControlTimer = 0.f;
		UnitBase->SetUEPathfinding = true;
	}

	// Check if Base is allready in Range /////////////////////////////
	AWorkingUnitBase* Worker = Cast<AWorkingUnitBase>(UnitBase);
	const float DistanceToBase = FVector::Dist(Worker->GetActorLocation(),  Worker->ResourcePlace->GetActorLocation()) - Worker->ResourcePlace->GetSimpleCollisionRadius();
	
	// Check Distance between Worker and Base
	if (EnableDistanceCheck && DistanceToBase <= ResourceAreaArrivalDistance)
	{
		Worker->ResourcePlace->HandleResourceExtractionArea(Worker, UnitBase);
		return;
	}
	// Check if Base is allready in Range /////////////////////////////

	
	if(!UnitBase->SetUEPathfinding)
		return;
	
	UnitBase->SetWalkSpeed(UnitBase->Attributes->GetRunSpeed());
	const FVector ResourceLocation = UnitBase->ResourcePlace->GetActorLocation();

	SetUEPathfinding(UnitBase, DeltaSeconds, ResourceLocation);
}

void AWorkerUnitControllerBase::ResourceExtraction(AUnitBase* UnitBase, float DeltaSeconds)
{
	if(!UnitBase || !UnitBase->ResourcePlace) return;

	UnitBase->SetWalkSpeed(0);
	UnitBase->UnitControlTimer += DeltaSeconds;
	if(UnitBase->UnitControlTimer >= UnitBase->ResourceExtractionTime)
	{
		UE_LOG(LogTemp, Warning, TEXT("Spawn WorkResource!"));
		SpawnWorkResource(UnitBase->ExtractingWorkResourceType, UnitBase->GetActorLocation(), UnitBase->ResourcePlace->WorkResourceClass, UnitBase);
		UnitBase->UnitControlTimer = 0;
		UnitBase->SetUEPathfinding = true;
		UnitBase->SetUnitState(UnitData::GoToBase);
	}
}

void AWorkerUnitControllerBase::GoToBase(AUnitBase* UnitBase, float DeltaSeconds)
{
	if(!UnitBase || !UnitBase->Base) return;

	UnitBase->SetWalkSpeed(UnitBase->Attributes->GetRunSpeed());
	
	if(UnitBase->CollisionUnit && UnitBase->CollisionUnit->TeamId == UnitBase->TeamId && UnitBase->CollisionUnit->GetUnitState() != UnitData::Dead)
	{
		UnitBase->SetUnitState(UnitData::Evasion);
		UnitBase->UnitStatePlaceholder = UnitData::GoToBase;
		return;
	}

	// Check if Base is allready in Range /////////////////////////////
	AWorkingUnitBase* Worker = Cast<AWorkingUnitBase>(UnitBase);
	AResourceGameMode* ResourceGameMode = Cast<AResourceGameMode>(GetWorld()->GetAuthGameMode());
	const bool CanAffordConstruction = Worker->BuildArea? ResourceGameMode->CanAffordConstruction(Worker->BuildArea->ConstructionCost, Worker->TeamId) : false; //Worker->BuildArea->CanAffordConstruction(Worker->TeamId, ResourceGameMode->NumberOfTeams,ResourceGameMode->TeamResources) : false;

	const float DistanceToBase = FVector::Dist(Worker->GetActorLocation(),  Worker->Base->GetActorLocation()) - Worker->Base->GetSimpleCollisionRadius();


	// Check Distance between Worker and Base
	if (EnableDistanceCheck && DistanceToBase <= BaseArrivalDistance)
	{
		Worker->Base->HandleBaseArea(Worker, UnitBase, ResourceGameMode, CanAffordConstruction);
		return;
	}
	// Check if Base is allready in Range /////////////////////////////
	
	UnitBase->UnitControlTimer+= DeltaSeconds;
	if(UnitBase->UnitControlTimer > ResetPathfindingTime)
	{
		UnitBase->UnitControlTimer = 0.f;
		UnitBase->SetUEPathfinding = true;
	}

	if(!UnitBase->SetUEPathfinding)
		return;
	
	UnitBase->SetWalkSpeed(UnitBase->Attributes->GetRunSpeed());
	const FVector BaseLocation = UnitBase->Base->GetActorLocation();

	SetUEPathfinding(UnitBase, DeltaSeconds, BaseLocation);
}

void AWorkerUnitControllerBase::GoToBuild(AUnitBase* UnitBase, float DeltaSeconds)
{
	if (!UnitBase || !UnitBase->BuildArea || !UnitBase->BuildArea->BuildingClass) // || !UnitBase->BuildArea->BuildingClass
	{
		UnitBase->SetUEPathfinding = true;
		UnitBase->SetUnitState(UnitData::GoToResourceExtraction);
		return;
	}

	UnitBase->SetWalkSpeed(UnitBase->Attributes->GetRunSpeed());
	
	if(UnitBase->CollisionUnit && UnitBase->CollisionUnit->TeamId == UnitBase->TeamId && UnitBase->CollisionUnit->GetUnitState() != UnitData::Dead)
	{
		UnitBase->SetUEPathfinding = true;
		UnitBase->SetUnitState(UnitData::Evasion);
		UnitBase->UnitStatePlaceholder = UnitData::GoToBuild;
		return;
	}

	UnitBase->UnitControlTimer+= DeltaSeconds;
	
	if(UnitBase->UnitControlTimer > ResetPathfindingTime)
	{
		UnitBase->UnitControlTimer = 0.f;
		UnitBase->SetUEPathfinding = true;
	}


	// Check if Base is allready in Range /////////////////////////////
	AWorkingUnitBase* Worker = Cast<AWorkingUnitBase>(UnitBase);
	AResourceGameMode* ResourceGameMode = Cast<AResourceGameMode>(GetWorld()->GetAuthGameMode());
	const bool CanAffordConstruction = Worker->BuildArea? ResourceGameMode->CanAffordConstruction(Worker->BuildArea->ConstructionCost, Worker->TeamId) : false; //Worker->BuildArea->CanAffordConstruction(Worker->TeamId, ResourceGameMode->NumberOfTeams,ResourceGameMode->TeamResources) : false;

	const float DistanceToBase = FVector::Dist(Worker->GetActorLocation(),  Worker->BuildArea->GetActorLocation()) - Worker->BuildArea->GetSimpleCollisionRadius();
	
	// Check Distance between Worker and Base
	if (EnableDistanceCheck && DistanceToBase <= BuildAreaArrivalDistance)
	{
		Worker->BuildArea->HandleBuildArea(Worker, UnitBase, ResourceGameMode, CanAffordConstruction);
		return;
	}
	// Check if Base is allready in Range /////////////////////////////

	if(!UnitBase->SetUEPathfinding)
		return;
	
	UnitBase->SetWalkSpeed(UnitBase->Attributes->GetRunSpeed());
	const FVector BaseLocation = UnitBase->BuildArea->GetActorLocation();

	//BaseLocation = GetGroundLocation(BaseLocation, UnitBase);
	
	SetUEPathfinding(UnitBase, DeltaSeconds, BaseLocation);
}



FVector AWorkerUnitControllerBase::GetGroundLocation(FVector ALocation, AUnitBase* UnitBase)
{
	while(true)
	{

		// Now adjust the Z-coordinate of PatrolCloseLocation to ensure it's above terrain
		const FVector Start = FVector(ALocation.X, ALocation.Y, ALocation.Z + UnitBase->LineTraceZDistance);  // Start from a point high above the PatrolCloseLocation
		const FVector End = FVector(ALocation.X, ALocation.Y, ALocation.Z - UnitBase->LineTraceZDistance);  // End at a point below the PatrolCloseLocation

		FHitResult HitResult;
		FCollisionQueryParams CollisionParams;
		CollisionParams.AddIgnoredActor(this);  // Ignore this actor during the trace

		if (GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, CollisionParams))
		{
			AActor* HitActor = HitResult.GetActor();

			// Check if we hit the landscape
			if (HitActor && HitActor->IsA(ALandscape::StaticClass()) )
			{
				// Hit landscape
				// Set the Z-coordinate accordingly
				//UnitBase->RandomPatrolLocation.Z = HitResult.ImpactPoint.Z;
				return HitResult.ImpactPoint;
			}
		}
	}
}
void AWorkerUnitControllerBase:: Build(AUnitBase* UnitBase, float DeltaSeconds)
{
	if(!UnitBase || !UnitBase->BuildArea || !UnitBase->BuildArea->BuildingClass)
	{
		UnitBase->SetUEPathfinding = true;
		UnitBase->SetUnitState(UnitData::GoToResourceExtraction);
		return;
	}

	UnitBase->SetWalkSpeed(0);
	//UE_LOG(LogTemp, Warning, TEXT("BuildTime: %f"), UnitBase->UnitControlTimer);
	UnitBase->UnitControlTimer += DeltaSeconds;
	if(UnitBase->BuildArea->BuildTime < UnitBase->UnitControlTimer)
	{

		if(!UnitBase->BuildArea->Building)
		{
			FUnitSpawnParameter SpawnParameter;
			SpawnParameter.UnitBaseClass = UnitBase->BuildArea->BuildingClass;
			SpawnParameter.UnitControllerBaseClass = UnitBase->BuildArea->BuildingController;
			SpawnParameter.UnitOffset = FVector(0.f, 0.f, UnitBase->BuildArea->BuildZOffset);
			SpawnParameter.UnitMinRange = FVector(0.f);
			SpawnParameter.UnitMaxRange = FVector(0.f);
			SpawnParameter.ServerMeshRotation = FRotator (0.f, -90.f, 0.f);
			SpawnParameter.State = UnitData::Idle;
			SpawnParameter.StatePlaceholder = UnitData::Idle;
			SpawnParameter.Material = nullptr;
			//UE_LOG(LogTemp, Warning, TEXT("Spawn Building!"));
			UnitBase->BuildArea->Building = Cast<ABuildingBase>(SpawnSingleUnit(SpawnParameter, UnitBase->BuildArea->GetActorLocation(), nullptr, UnitBase->TeamId, nullptr));
			UnitBase->BuildArea->Building->NextWaypoint = UnitBase->BuildArea->NextWaypoint;
		}
			UnitBase->SetUEPathfinding = true;
			UnitBase->SetUnitState(UnitData::GoToResourceExtraction);
		
	}else if (UnitBase->GetUnitState() == UnitData::Dead)
	{
			UnitBase->BuildArea->PlannedBuilding = false;
			UnitBase->BuildArea->StartedBuilding = false;
	}
}




void AWorkerUnitControllerBase::SpawnWorkResource(EResourceType ResourceType, FVector Location, TSubclassOf<class AWorkResource> WRClass, AUnitBase* ActorToLockOn)
{

	FTransform Transform;

	Transform.SetLocation(Location);
	Transform.SetRotation(FQuat(FRotator::ZeroRotator)); // FRotator::ZeroRotator

		
	const auto MyWorkResource = Cast<AWorkResource>
						(UGameplayStatics::BeginDeferredActorSpawnFromClass
						(this, WRClass, Transform,  ESpawnActorCollisionHandlingMethod::AlwaysSpawn));
	
	if (MyWorkResource != nullptr)
	{
		if(ActorToLockOn)
		{
			//MyWorkResource->AttachToComponent(ActorToLockOn->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, FName("ResourceSocket"));
			MyWorkResource->IsAttached = true;
			MyWorkResource->ResourceType = ResourceType;
			
			UGameplayStatics::FinishSpawningActor(MyWorkResource, Transform);
			
			ActorToLockOn->WorkResource = MyWorkResource;


			
			/// Attack Socket with Delay //////////////
			FAttachmentTransformRules AttachmentRules(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::KeepWorld, false);
			FName SocketName = FName("ResourceSocket");
			
			auto AttachWorkResource = [MyWorkResource, ActorToLockOn, AttachmentRules, SocketName]()
			{
				if (ActorToLockOn->GetMesh()->DoesSocketExist(SocketName))
				{
					MyWorkResource->AttachToComponent(ActorToLockOn->GetMesh(), AttachmentRules, SocketName);
					MyWorkResource->IsAttached = true;
					// Now attempt to set the actor's relative location after attachment
					MyWorkResource->SetActorRelativeLocation(MyWorkResource->SocketOffset, false, nullptr, ETeleportType::TeleportPhysics);
				}
			};

			AttachWorkResource();
		}
	}
	
}

void AWorkerUnitControllerBase::DespawnWorkResource(AWorkResource* WorkResource)
{
	if (WorkResource != nullptr)
	{
		WorkResource->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		WorkResource->Destroy();
		WorkResource = nullptr;
	}
}

void AWorkerUnitControllerBase::DetachWorkResource(AWorkResource* WorkResource)
{
	if (WorkResource != nullptr)
	{
		WorkResource->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		WorkResource->Destroy();
		WorkResource->IsAttached = false;
	}
}



AUnitBase* AWorkerUnitControllerBase::SpawnSingleUnit(FUnitSpawnParameter SpawnParameter, FVector Location,
	AUnitBase* UnitToChase, int TeamId, AWaypoint* Waypoint)
{
	// Waypointspawn

	FTransform EnemyTransform;
	
	EnemyTransform.SetLocation(FVector(Location.X+SpawnParameter.UnitOffset.X, Location.Y+SpawnParameter.UnitOffset.Y, Location.Z+SpawnParameter.UnitOffset.Z));
		
		
	const auto UnitBase = Cast<AUnitBase>
		(UGameplayStatics::BeginDeferredActorSpawnFromClass
		(this, SpawnParameter.UnitBaseClass, EnemyTransform, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn));

		

	if(SpawnParameter.UnitControllerBaseClass)
	{
		AAIController* ControllerBase = GetWorld()->SpawnActor<AAIController>(SpawnParameter.UnitControllerBaseClass, FTransform());
		if(!ControllerBase) return nullptr;
		ControllerBase->Possess(UnitBase);
	}
	
	if (UnitBase != nullptr)
	{
		if(UnitBase->UnitToChase)
		{
			UnitBase->UnitToChase = UnitToChase;
			UnitBase->SetUnitState(UnitData::Chase);
		}
		
		if(TeamId)
		{
			UnitBase->TeamId = TeamId;
		}

		UnitBase->ServerMeshRotation = SpawnParameter.ServerMeshRotation;
			
		UnitBase->OnRep_MeshAssetPath();
		UnitBase->OnRep_MeshMaterialPath();

		UnitBase->SetReplicateMovement(true);
		SetReplicates(true);
		UnitBase->GetMesh()->SetIsReplicated(true);

		// Does this have to be replicated?
		UnitBase->SetMeshRotationServer();
		
		UnitBase->UnitState = SpawnParameter.State;
		UnitBase->UnitStatePlaceholder = SpawnParameter.StatePlaceholder;

		
		
		UGameplayStatics::FinishSpawningActor(UnitBase, EnemyTransform);


		UnitBase->InitializeAttributes();
		AResourceGameMode* GameMode = Cast<AResourceGameMode>(GetWorld()->GetAuthGameMode());
		if (!GameMode){
			UnitBase->SetUEPathfinding = true;
			UnitBase->SetUnitState(UnitData::GoToResourceExtraction);
			return nullptr;
		}
		
		GameMode->AddUnitIndexAndAssignToAllUnitsArray(UnitBase);
		
		
		return UnitBase;
	}

	return nullptr;
}


	