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
#include "Controller/ControllerBase.h"
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
				UE_LOG(LogTemp, Warning, TEXT("GoToBuild"));
				GoToBuild(UnitBase, DeltaSeconds);
				//if(!UnitBase->IsFriendly)UE_LOG(LogTemp, Warning, TEXT("None"));
			}
		break;
		case UnitData::Build:
			{
				UE_LOG(LogTemp, Warning, TEXT("Build"));
				Build(UnitBase, DeltaSeconds);
				//if(!UnitBase->IsFriendly)UE_LOG(LogTemp, Warning, TEXT("None"));
			}
		break;
		case UnitData::Dead:
		{
			//if(UnitBase->TeamId == 2)UE_LOG(LogTemp, Warning, TEXT("Dead"));
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
					SetUEPathfindingRandomLocation(UnitBase, DeltaSeconds);
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
				
					if(UnitBase->UnitControlTimer > UnitBase->NextWaypoint->RandomTime)
					{
						UnitBase->UnitControlTimer = 0.f;
						UnitBase->SetUEPathfinding = true;
						UnitBase->SetUnitState(UnitData::PatrolRandom);
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
		case UnitData::Evasion:
		{
			//if(UnitBase->TeamId == 3)UE_LOG(LogTemp, Warning, TEXT("Idle"));
			if(	UnitBase->CollisionUnit)
			{
				//UnitBase->EvadeDistance = GetCloseLocation(UnitBase->GetActorLocation(), 100.f);
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
		UnitBase->SetUnitState(UnitBase->UnitStatePlaceholder);
		UnitBase->CollisionUnit = nullptr;
	}
	UnitBase->UnitControlTimer = 0.f;
}

void AWorkerUnitControllerBase::GoToResourceExtraction(AUnitBase* UnitBase, float DeltaSeconds)
{
	if(!UnitBase || !UnitBase->ResourcePlace) return;

	
	if(UnitBase->CollisionUnit && UnitBase->CollisionUnit->TeamId == UnitBase->TeamId && UnitBase->CollisionUnit->GetUnitState() != UnitData::Dead)
	{
		UnitBase->SetUnitState(UnitData::Evasion);
		UnitBase->UnitStatePlaceholder = UnitData::GoToResourceExtraction;
		return;
	}

	if(!UnitBase->SetUEPathfinding)
		return;
	
	UnitBase->SetWalkSpeed(UnitBase->Attributes->GetRunSpeed());
	const FVector ResourceLocation = UnitBase->ResourcePlace->GetActorLocation();

	SetUEPathfinding(UnitBase, DeltaSeconds, ResourceLocation);
}

void AWorkerUnitControllerBase::ResourceExtraction(AUnitBase* UnitBase, float DeltaSeconds)
{
	if(!UnitBase || !UnitBase->ResourcePlace) return;

	UnitBase->UnitControlTimer += DeltaSeconds;
	if(UnitBase->UnitControlTimer >= UnitBase->ResourceExtractionTime)
	{
		SpawnWorkResource(UnitBase->ExtractingWorkResourceType, UnitBase->GetActorLocation(), UnitBase->WorkResourceClass, UnitBase);
		UnitBase->UnitControlTimer = 0;
		UnitBase->SetUEPathfinding = true;
		UnitBase->SetUnitState(UnitData::GoToBase);
	}
}

void AWorkerUnitControllerBase::GoToBase(AUnitBase* UnitBase, float DeltaSeconds)
{
	if(!UnitBase || !UnitBase->Base) return;

	if(UnitBase->CollisionUnit && UnitBase->CollisionUnit->TeamId == UnitBase->TeamId && UnitBase->CollisionUnit->GetUnitState() != UnitData::Dead)
	{
		UnitBase->SetUnitState(UnitData::Evasion);
		UnitBase->UnitStatePlaceholder = UnitData::GoToBase;
		return;
	}

	if(!UnitBase->SetUEPathfinding)
		return;
	
	UnitBase->SetWalkSpeed(UnitBase->Attributes->GetRunSpeed());
	const FVector BaseLocation = UnitBase->Base->GetActorLocation();

	SetUEPathfinding(UnitBase, DeltaSeconds, BaseLocation);
}

void AWorkerUnitControllerBase::GoToBuild(AUnitBase* UnitBase, float DeltaSeconds)
{
	if(!UnitBase || !UnitBase->BuildArea) return;
	
}

void AWorkerUnitControllerBase::Build(AUnitBase* UnitBase, float DeltaSeconds)
{
	if(!UnitBase || !UnitBase->BuildArea) return;
	
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
		//MyWorkResource->TeamId = TeamId;
		//MyWorkResource->Mesh->OnComponentBeginOverlap.AddDynamic(MyWorkResource, &AEffectArea::OnOverlapBegin);

		// Apply scale to the Mesh
		//MyWorkResource->Mesh->SetWorldScale3D(Scale);

		
		if(ActorToLockOn)
		{
			MyWorkResource->AttachToComponent(ActorToLockOn->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, FName("ResourceSocket"));
			MyWorkResource->IsAttached = true;
			MyWorkResource->ResourceType = ResourceType;
		}
		
		UGameplayStatics::FinishSpawningActor(MyWorkResource, Transform);
		ActorToLockOn->WorkResource = MyWorkResource;
	}
	
}

void AWorkerUnitControllerBase::DespawnWorkResource(AWorkResource* WorkResource)
{
	if (WorkResource != nullptr)
	{
		WorkResource->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		WorkResource->Destroy();
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