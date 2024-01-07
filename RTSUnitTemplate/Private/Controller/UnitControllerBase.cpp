// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Controller/UnitControllerBase.h"
#include <string>

#include "Landscape.h"
#include "Characters/Unit/UnitBase.h"
#include "Actors/Waypoint.h"
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

AUnitControllerBase::AUnitControllerBase()
{
	
	PrimaryActorTick.bCanEverTick = true;
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("Sight Config"));
	SetPerceptionComponent(*CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("Perception Component")));

	SightConfig->SightRadius = SightRadius;
	SightConfig->LoseSightRadius = LoseSightRadius;
	SightConfig->PeripheralVisionAngleDegrees = FieldOfView;
	SightConfig->SetMaxAge(SightAge);

	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;

	GetPerceptionComponent()->SetDominantSense(*SightConfig->GetSenseImplementation());
	GetPerceptionComponent()->OnPerceptionUpdated.AddDynamic(this, &AUnitControllerBase::OnUnitDetected);
	GetPerceptionComponent()->ConfigureSense(*SightConfig);
	
}

void AUnitControllerBase::BeginPlay()
{
	Super::BeginPlay();
	
		if (GetPerceptionComponent() != nullptr) {
			UE_LOG(LogTemp, Warning, TEXT("All Systems Set"));
		}
		else {
			UE_LOG(LogTemp, Warning, TEXT("Some Problem Occured"));
		}
	
}

void AUnitControllerBase::OnPossess(APawn* PawN)
{
	Super::OnPossess(PawN);
}

void AUnitControllerBase::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}


void AUnitControllerBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	
	UnitControlStateMachine(DeltaSeconds);
}

FRotator AUnitControllerBase::GetControlRotation() const
{
	if (GetPawn() == nullptr) {
		return FRotator(0.0f, 0.0f, 0.0f);
	}

	return FRotator(0.0f, GetPawn()->GetActorRotation().Yaw, 0.0f);
}

void AUnitControllerBase::KillUnitBase(AUnitBase* UnitBase)
{
	UnitBase->SetWalkSpeed(0);
	UnitBase->GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	UnitBase->SetActorEnableCollision(false);
	UnitBase->SetDeselected();
	UnitBase->SetUnitState(UnitData::Dead);
}

void AUnitControllerBase::OnUnitDetected(const TArray<AActor*>& DetectedUnits)
{
	AUnitBase* CurrentUnit = Cast<AUnitBase>(GetPawn());
	AUnitBase* DetectedUnit = Cast<AUnitBase>(DetectedUnits[0]);
	
	if(!DetectFriendlyUnits && DetectedUnit && CurrentUnit && (DetectedUnit->TeamId != CurrentUnit->TeamId)) 
	{
		if(DetectedUnit->GetUnitState() != UnitData::Dead && CurrentUnit->GetUnitState() != UnitData::Dead)
		{
			DistanceToUnitToChase = GetPawn()->GetDistanceTo(DetectedUnit);
			
				CurrentUnit->UnitsToChase.Emplace(DetectedUnit);
				CurrentUnit->SetNextUnitToChase();
				
				if (CurrentUnit->UnitToChase) {
					if(CurrentUnit->GetUnitState() != UnitData::Run)
					{
						CurrentUnit->AddMovementInput(FVector(0.f), CurrentUnit->Attributes->GetRunSpeedScale());
						CurrentUnit->SetUnitState(UnitData::Chase);
					}
				}
			
		}
	}else if (DetectFriendlyUnits && DetectedUnit && CurrentUnit && (DetectedUnit->TeamId == CurrentUnit->TeamId)) {
		if(DetectedUnit->GetUnitState() != UnitData::Dead && CurrentUnit->GetUnitState() != UnitData::Dead)
		{
			DistanceToUnitToChase = GetPawn()->GetDistanceTo(DetectedUnit);
			
			CurrentUnit->UnitsToChase.Emplace(DetectedUnit);
			CurrentUnit->SetNextUnitToChase();
				
			if (CurrentUnit->UnitToChase) {
				if(CurrentUnit->GetUnitState() != UnitData::Attack && CurrentUnit->GetUnitState() != UnitData::Run && CurrentUnit->UnitToChase->Attributes->GetHealth() < CurrentUnit->UnitToChase->Attributes->GetMaxHealth())
				{
					CurrentUnit->AddMovementInput(FVector(0.f), CurrentUnit->Attributes->GetRunSpeedScale());
					CurrentUnit->SetUnitState(UnitData::Chase);
				}
			}
			
		}
	}
	
}

void AUnitControllerBase::RotateToAttackUnit(AUnitBase* AttackingUnit, AUnitBase* UnitToAttack)
{
	if(AttackingUnit && UnitToAttack)
	{
		float AngleDiff = CalcAngle(AttackingUnit->GetActorForwardVector(), UnitToAttack->GetActorLocation() - AttackingUnit->GetActorLocation());

		FQuat QuadRotation;
		
			if ((AngleDiff > 2.f+AttackAngleTolerance && AngleDiff < 179.f-AttackAngleTolerance) || (AngleDiff <= -179.f+AttackAngleTolerance && AngleDiff > -359.f+AttackAngleTolerance)) {
				AngleDiff > 50.f || AngleDiff < -50.f ? QuadRotation = FQuat(FRotator(0.f, -20.0f, 0.f)) : QuadRotation = FQuat(FRotator(0.f, -4.0f, 0.f));
				AttackingUnit->AddActorLocalRotation(QuadRotation, false, 0, ETeleportType::None);
			}
			else if ((AngleDiff < -2.f-AttackAngleTolerance && AngleDiff > -179.f+AttackAngleTolerance) || (AngleDiff >= 180.f-AttackAngleTolerance && AngleDiff < 359.f-AttackAngleTolerance)) {
				AngleDiff > 50.f || AngleDiff < -50.f ? QuadRotation = FQuat(FRotator(0.f, 20.0f, 0.f)) : QuadRotation = FQuat(FRotator(0.f, 4.0f, 0.f));
				AttackingUnit->AddActorLocalRotation(QuadRotation, false, 0, ETeleportType::None);
			}
	}
}



void AUnitControllerBase::UnitControlStateMachine(float DeltaSeconds)
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
		KillUnitBase(UnitBase);
		UnitBase->UnitControlTimer = 0.f;
	}
}

bool AUnitControllerBase::IsUnitToChaseInRange(AUnitBase* UnitBase)
{
	if(!UnitBase->UnitToChase) return false;
	if(UnitBase->UnitToChase && UnitBase->UnitToChase->GetUnitState() == UnitData::Dead) return false;
	
	FVector UnitToChaseOrigin;
	FVector UnitToChaseBoxExtent;
	DistanceToUnitToChase = GetPawn()->GetDistanceTo(UnitBase->UnitToChase);
	
	UnitBase->UnitToChase->GetActorBounds(false, UnitToChaseOrigin, UnitToChaseBoxExtent);
	const float RangeOffset = UnitToChaseBoxExtent.X > UnitToChaseBoxExtent.Y? UnitToChaseBoxExtent.X/1.75f : UnitToChaseBoxExtent.Y/1.75f;
	
	if (DistanceToUnitToChase < (UnitBase->Attributes->GetRange() + RangeOffset))
		return true;
	
	return false;
}

void AUnitControllerBase::Dead(AUnitBase* UnitBase, float DeltaSeconds)
{
	//UnitBase->SetWalkSpeed(0);
				
	UnitBase->UnitControlTimer = (UnitBase->UnitControlTimer + DeltaSeconds);

	FVector ActorLocation = UnitBase->GetActorLocation();
				
	//UnitBase->SetActorLocation(FVector(ActorLocation.X + 0.f,ActorLocation.Y + 0.f,ActorLocation.Z -1.f));

	if (UnitBase->UnitControlTimer >= DespawnTime) {
		if(UnitBase->DestroyAfterDeath) UnitBase->Destroy(true, false);
	}
}

void AUnitControllerBase::Patrol(AUnitBase* UnitBase, float DeltaSeconds)
{
	UnitBase->SetWalkSpeed(UnitBase->Attributes->GetRunSpeed());
				
	if(UnitBase->UnitToChase && UnitBase->UnitToChase->GetUnitState() != UnitData::Dead)
	{
		if(UnitBase->SetNextUnitToChase())
			UnitBase->SetUnitState(UnitData::Chase);
		
	} else if (UnitBase->NextWaypoint != nullptr)
	{

		FVector WaypointLocation = UnitBase->NextWaypoint->GetActorLocation();

		if(UnitBase->IsFlying)
		{
			WaypointLocation =  FVector(WaypointLocation.X, WaypointLocation.Y, UnitBase->FlyHeight);
		}

		if(UnitBase->FollowPath)
		{
			const FVector ADirection = UKismetMathLibrary::GetDirectionUnitVector(UnitBase->GetActorLocation(), UnitBase->RunLocation);
			UnitBase->AddMovementInput(ADirection, UnitBase->Attributes->GetRunSpeedScale());
		}else
		{
			const FVector ADirection = UKismetMathLibrary::GetDirectionUnitVector(UnitBase->GetActorLocation(), WaypointLocation);
			UnitBase->AddMovementInput(ADirection, UnitBase->Attributes->GetRunSpeedScale());
		}
	}
	else
	{
		UnitBase->SetUnitState(UnitData::Idle);
	}
}

void AUnitControllerBase::Run(AUnitBase* UnitBase, float DeltaSeconds)
{
	
	if(UnitBase->GetToggleUnitDetection() && UnitBase->UnitToChase)
	{
		if(UnitBase->SetNextUnitToChase())
		{
			UnitBase->SetUnitState(UnitData::Chase);
		}
	}
				
	UnitBase->SetWalkSpeed(UnitBase->Attributes->GetRunSpeed());
				
	const FVector UnitLocation = UnitBase->GetActorLocation();
				
	if(UnitBase->IsFlying)
	{
		UnitBase->RunLocation = FVector(UnitBase->RunLocation.X, UnitBase->RunLocation.Y, UnitBase->FlyHeight);
	}
	
	const FVector ADirection = UKismetMathLibrary::GetDirectionUnitVector(UnitLocation, UnitBase->RunLocation);
	UnitBase->AddMovementInput(ADirection, UnitBase->Attributes->GetRunSpeedScale());

	const float Distance = sqrt((UnitLocation.X-UnitBase->RunLocation.X)*(UnitLocation.X-UnitBase->RunLocation.X)+(UnitLocation.Y-UnitBase->RunLocation.Y)*(UnitLocation.Y-UnitBase->RunLocation.Y));

	if (Distance <= UnitBase->StopRunTolerance) {
		UnitBase->SetUnitState(UnitData::Idle);
	}
	UnitBase->UnitControlTimer = 0.f;
}

void AUnitControllerBase::Chase(AUnitBase* UnitBase, float DeltaSeconds)
{
					if(UnitBase->CollisionUnit && UnitBase->CollisionUnit->TeamId != UnitBase->TeamId && UnitBase->CollisionUnit->GetUnitState() != UnitData::Dead)
					{
						UnitBase->UnitToChase = UnitBase->CollisionUnit;
						UnitBase->UnitsToChase.Emplace(UnitBase->CollisionUnit);
						UnitBase->CollisionUnit = nullptr;
					}else if(!UnitBase->SetNextUnitToChase())
					{
						UnitBase->SetUEPathfinding = true;
						UnitBase->SetUnitState(UnitBase->UnitStatePlaceholder);
					}else if (UnitBase->UnitToChase) {
    					UnitBase->SetWalkSpeed(UnitBase->Attributes->GetRunSpeed());

		
    				
    				RotateToAttackUnit(UnitBase, UnitBase->UnitToChase);
    				DistanceToUnitToChase = GetPawn()->GetDistanceTo(UnitBase->UnitToChase);
    
    					AUnitBase* UnitToChase = UnitBase->UnitToChase;
 
    					if (IsUnitToChaseInRange(UnitBase)) {
    						if(UnitBase->UnitControlTimer >= PauseDuration)
    						{
    							UnitBase->ServerStartAttackEvent_Implementation();
    							UnitBase->SetUnitState(UnitData::Attack);
    							CreateProjectile(UnitBase);
    						}else
    						{
    							UnitBase->SetUnitState(UnitData::Pause);
    						}	
    					}
    					else {
    						FVector UnitToChaseLocation = UnitToChase->GetActorLocation();
    
    						if(UnitToChase->IsFlying && !UnitBase->IsFlying)
    						{
    							if (DistanceToUnitToChase > UnitBase->StopRunToleranceForFlying)
    							{
    								UnitBase->SetUnitState(UnitData::Idle);
    							}
    						}
    						
    						if(UnitBase->IsFlying)
    						{
    							UnitToChaseLocation =  FVector(UnitToChaseLocation.X, UnitToChaseLocation.Y, UnitBase->FlyHeight);
    						}

    						UnitBase->UnitToChase->ActivateAbilityByInputID(UnitBase->UnitToChase->OffensiveAbilityID);
    						
    						UnitBase->SetUEPathfinding = true;
    						SetUEPathfinding(UnitBase, DeltaSeconds, UnitToChaseLocation);
    	
    					}
    
    					if (DistanceToUnitToChase > LoseSightRadius) {
    
    						UnitBase->UnitsToChase.Remove(UnitBase->UnitToChase );
    						UnitBase->UnitToChase = nullptr;
    						
    						if(!UnitBase->TeamId && UnitBase->FollowPath)
    						{
    							UnitBase->RunLocationArray.Empty();
    							UnitBase->RunLocationArrayIterator = 0;
    							UnitBase->DijkstraStartPoint = UnitBase->GetActorLocation();
    							UnitBase->DijkstraEndPoint = UnitBase->NextWaypoint->GetActorLocation();
    							UnitBase->DijkstraSetPath = true;
    							UnitBase->FollowPath = false;
    							UnitBase->SetUEPathfinding = true;
    					
    							UnitBase->SetUnitState(UnitBase->UnitStatePlaceholder);
    						}else
    						{
    							UnitBase->UnitsToChase.Remove(UnitBase->UnitToChase );
    							UnitBase->UnitToChase = nullptr;
    							UnitBase->SetUEPathfinding = true;
    							
    							UnitBase->SetUnitState(UnitBase->UnitStatePlaceholder);
    						}
    						
    					}
    				}else
    				{
    					UnitBase->SetUEPathfinding = true;
    					UnitBase->SetUnitState(UnitBase->UnitStatePlaceholder);
    				}
}


void AUnitControllerBase::Attack(AUnitBase* UnitBase, float DeltaSeconds)
{
	UnitBase->SetWalkSpeed(0);	
	RotateToAttackUnit(UnitBase, UnitBase->UnitToChase);
	UnitBase->UnitControlTimer = (UnitBase->UnitControlTimer + DeltaSeconds);

	if(UnitBase->UnitToChase && UnitBase->UnitToChase->GetUnitState() == UnitData::Dead)
	{
		if(UnitBase->SetNextUnitToChase())
		{
			if(!IsUnitToChaseInRange(UnitBase))
			{
				UnitBase->SetUnitState( UnitData::Chase );
				UnitBase->UnitControlTimer = 0.f;
				return;
			}
		}else
		{
			UnitBase->SetUEPathfinding = true;
			UnitBase->SetUnitState( UnitBase->UnitStatePlaceholder ); //UnitBase->UnitStatePlaceholder
			UnitBase->UnitControlTimer = 0.f;
			return;
		}
	}else if(!UnitBase->UnitToChase)
	{
		UnitBase->SetUEPathfinding = true;
		UnitBase->SetUnitState( UnitBase->UnitStatePlaceholder); //UnitBase->UnitStatePlaceholder
		UnitBase->UnitControlTimer = 0.f;
		return;
	}
				
	if (UnitBase->UnitControlTimer > AttackDuration + PauseDuration) {
		if(!UnitBase->UseProjectile )
		{
			// Attack without Projectile
			if(IsUnitToChaseInRange(UnitBase))
			{

				float NewDamage = UnitBase->Attributes->GetAttackDamage() - UnitBase->UnitToChase->Attributes->GetArmor();
			
				if(UnitBase->IsDoingMagicDamage)
					NewDamage = UnitBase->Attributes->GetAttackDamage() - UnitBase->UnitToChase->Attributes->GetMagicResistance();
				
				if(UnitBase->UnitToChase->Attributes->GetShield() <= 0)
					UnitBase->UnitToChase->SetHealth(UnitBase->UnitToChase->Attributes->GetHealth()-NewDamage);
				else
					UnitBase->UnitToChase->Attributes->SetAttributeShield(UnitBase->UnitToChase->Attributes->GetShield()-UnitBase->Attributes->GetAttackDamage());

				UnitBase->LevelData.Experience++;

				UnitBase->ServerMeeleImpactEvent();
				UnitBase->UnitToChase->ActivateAbilityByInputID(UnitBase->UnitToChase->DefensiveAbilityID);
				
				if (!UnitBase->UnitToChase->UnitsToChase.Contains(UnitBase))
				{
					// If not, add UnitBase to the array
					UnitBase->UnitToChase->UnitsToChase.Emplace(UnitBase);
					UnitBase->UnitToChase->SetNextUnitToChase();
				}
				
				if(UnitBase->UnitToChase->GetUnitState() != UnitData::Run &&
					UnitBase->UnitToChase->GetUnitState() != UnitData::Attack &&
					UnitBase->UnitToChase->GetUnitState() != UnitData::Pause)
				{
					UnitBase->UnitToChase->UnitControlTimer = 0.f;
					UnitBase->UnitToChase->SetUnitState( UnitData::IsAttacked );
				}
			}
			else if(UnitBase->UnitToChase->GetUnitState() != UnitData::Run)
			{
				UnitBase->UnitToChase->SetUnitState( UnitData::Chase );
				return;
			}// Attack without Projectile	
		}

		ProjectileSpawned = false;
		UnitBase->UnitControlTimer = 0.f;
		UnitBase->SetUnitState( UnitData::Pause);
	}
}

void AUnitControllerBase::Pause(AUnitBase* UnitBase, float DeltaSeconds)
{
	UnitBase->SetWalkSpeed(0);
	RotateToAttackUnit(UnitBase, UnitBase->UnitToChase);
				
	UnitBase->UnitControlTimer = (UnitBase->UnitControlTimer + DeltaSeconds);
	
	if(UnitBase->UnitToChase && UnitBase->UnitToChase->GetUnitState() == UnitData::Dead) {

		if(UnitBase->SetNextUnitToChase()) return;

		UnitBase->SetUEPathfinding = true;
		UnitBase->SetUnitState( UnitBase->UnitStatePlaceholder );
				
	} else if (UnitBase->UnitControlTimer > PauseDuration) {
		
		ProjectileSpawned = false;
		UnitBase->SetUnitState(UnitData::Chase);
		UnitBase->SetWalkSpeed(UnitBase->Attributes->GetRunSpeed());
		
		if (IsUnitToChaseInRange(UnitBase)) {
				UnitBase->ServerStartAttackEvent_Implementation();
				UnitBase->SetUnitState(UnitData::Attack);
				CreateProjectile(UnitBase);
		}else
		{
			UnitBase->SetUnitState(UnitData::Chase);
		}
		
	}
}

void AUnitControllerBase::IsAttacked(AUnitBase* UnitBase, float DeltaSeconds)
{
	UnitBase->UnitControlTimer += DeltaSeconds;
	if (UnitBase->UnitControlTimer > IsAttackedDuration) {
		UnitBase->SetWalkSpeed(UnitBase->Attributes->GetRunSpeed());
		UnitBase->SetUnitState(UnitData::Pause);
	}
}

void AUnitControllerBase::Idle(AUnitBase* UnitBase, float DeltaSeconds)
{
	UnitBase->SetWalkSpeed(0);



	if(UnitBase->UnitsToChase.Num())
	{
		UnitBase->SetUnitState(UnitData::Chase);
	}else
		SetUnitBackToPatrol(UnitBase, DeltaSeconds);
}

void AUnitControllerBase::SetUnitBackToPatrol(AUnitBase* UnitBase, float DeltaSeconds)
{
	UnitBase->UnitControlTimer += DeltaSeconds;
	
	if(UnitBase->NextWaypoint && SetUnitsBackToPatrol && UnitBase->UnitControlTimer > SetUnitsBackToPatrolTime)
	{
		if(UnitBase->NextWaypoint->PatrolCloseToWaypoint)
		{
			UnitBase->UnitControlTimer = 0.f;
			UnitBase->NextWaypoint->RandomTime = FMath::FRandRange(UnitBase->NextWaypoint->PatrolCloseMinInterval, UnitBase->NextWaypoint->PatrolCloseMaxInterval);
			UnitBase->SetUEPathfinding = true;
			UnitBase->SetUnitState(UnitData::PatrolRandom);
		}else
		{
			UnitBase->UnitControlTimer = 0.f;
			UnitBase->SetUEPathfinding = true;
			UnitBase->SetUnitState(UnitData::Patrol);
		}
	}
}

void AUnitControllerBase::RunUEPathfinding(AUnitBase* UnitBase, float DeltaSeconds)
{
	if(UnitBase->GetToggleUnitDetection() && UnitBase->UnitToChase)
	{
		if(UnitBase->SetNextUnitToChase())
		{
			UnitBase->SetUnitState(UnitData::Chase);
		}
	}
	
	UnitBase->SetWalkSpeed(UnitBase->Attributes->GetRunSpeed());

	const FVector UnitLocation = UnitBase->GetActorLocation();
	const float Distance = sqrt((UnitLocation.X-UnitBase->RunLocation.X)*(UnitLocation.X-UnitBase->RunLocation.X)+(UnitLocation.Y-UnitBase->RunLocation.Y)*(UnitLocation.Y-UnitBase->RunLocation.Y));

	if (Distance <= UnitBase->StopRunTolerance) {
		UnitBase->SetUnitState(UnitData::Idle);
	}
}

void AUnitControllerBase::PatrolUEPathfinding(AUnitBase* UnitBase, float DeltaSeconds)
{
	UnitBase->SetWalkSpeed(UnitBase->Attributes->GetRunSpeed());
	
	if(UnitBase->CollisionUnit && UnitBase->CollisionUnit->TeamId !=  UnitBase->TeamId && UnitBase->CollisionUnit->GetUnitState() != UnitData::Dead)
	{
		UnitBase->UnitToChase = UnitBase->CollisionUnit;
		UnitBase->UnitsToChase.Emplace(UnitBase->CollisionUnit);
		UnitBase->CollisionUnit = nullptr;
		UnitBase->SetUEPathfinding = true;
		UnitBase->SetUnitState(UnitData::Chase);
	}if(UnitBase->UnitToChase && UnitBase->UnitToChase->GetUnitState() != UnitData::Dead)
	{
		if(UnitBase->SetNextUnitToChase())
		{
			UnitBase->SetUEPathfinding = true;
			UnitBase->SetUnitState(UnitData::Chase);
		}
		
	} else if (UnitBase->NextWaypoint != nullptr)
	{
		if(UnitBase->IsFlying)
		{
			FVector WaypointLocation = UnitBase->NextWaypoint->GetActorLocation();
			WaypointLocation =  FVector(WaypointLocation.X, WaypointLocation.Y, UnitBase->FlyHeight);
			const FVector ADirection = UKismetMathLibrary::GetDirectionUnitVector(UnitBase->GetActorLocation(), WaypointLocation);
			UnitBase->AddMovementInput(ADirection, UnitBase->Attributes->GetRunSpeedScale());
		}else 
			SetUEPathfinding(UnitBase, DeltaSeconds, UnitBase->NextWaypoint->GetActorLocation());
		
	}
	else
	{
		UnitBase->SetUnitState(UnitData::Idle);
	}
}

void AUnitControllerBase::SetPatrolCloseLocation(AUnitBase* UnitBase)
{
	while(true)
	{
		UnitBase->RandomPatrolLocation = UnitBase->NextWaypoint->GetActorLocation()
		+ FMath::RandPointInBox(FBox(FVector(-UnitBase->NextWaypoint->PatrolCloseOffset.X, -UnitBase->NextWaypoint->PatrolCloseOffset.Y, 0),
		FVector(UnitBase->NextWaypoint->PatrolCloseOffset.X, UnitBase->NextWaypoint->PatrolCloseOffset.Y, 0)));

		// Now adjust the Z-coordinate of PatrolCloseLocation to ensure it's above terrain
		const FVector Start = FVector(UnitBase->RandomPatrolLocation.X, UnitBase->RandomPatrolLocation.Y, UnitBase->RandomPatrolLocation.Z + 1000.f);  // Start from a point high above the PatrolCloseLocation
		const FVector End = FVector(UnitBase->RandomPatrolLocation.X, UnitBase->RandomPatrolLocation.Y, UnitBase->RandomPatrolLocation.Z - 1000.f);  // End at a point below the PatrolCloseLocation

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
				UnitBase->RandomPatrolLocation.Z = HitResult.ImpactPoint.Z;
				return;
			}
		}
	}
}

void AUnitControllerBase::SetUEPathfindingRandomLocation(AUnitBase* UnitBase, float DeltaSeconds)
{
	if(!UnitBase ||  !UnitBase->NextWaypoint) return;

	FVector ActorLocation = UnitBase->GetActorLocation();
	FVector WaypointLocation = UnitBase->NextWaypoint->GetActorLocation();
	
	if (ActorLocation.Equals(UnitBase->RandomPatrolLocation, 200.f) ||
		UnitBase->UnitControlTimer > UnitBase->NextWaypoint->RandomTime) 
	{
		if (FMath::FRand() * 100.0f < UnitBase->NextWaypoint->PatrolCloseIdlePercentage && !ActorLocation.Equals(WaypointLocation, 200.f)) // && DistanceToWaypoint > 500.f
		{
			UnitBase->UnitControlTimer = 0.f;
			UnitBase->SetUnitState(UnitData::PatrolIdle);
			return;
		}
		
		UnitBase->UnitControlTimer = 0.f;
		//UnitBase->ReCalcRandomLocation = true;
		UnitBase->SetUEPathfinding = true;
		
	}

	if(UnitBase->CollisionUnit)
	{
		UnitBase->SetUEPathfinding = true;
		UnitBase->CollisionUnit = nullptr;
	}
	
	if(!UnitBase->SetUEPathfinding)
		return;
	
	//if(UnitBase->ReCalcRandomLocation)
	SetPatrolCloseLocation(UnitBase);
	SetUEPathfinding(UnitBase, DeltaSeconds, UnitBase->RandomPatrolLocation);
}

void AUnitControllerBase::SetUEPathfinding(AUnitBase* UnitBase, float DeltaSeconds, FVector Location)
{
	if(!UnitBase->SetUEPathfinding)
		return;
		
	APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	if(PlayerController)
	{
		AControllerBase* ControllerBase = Cast<AControllerBase>(PlayerController);
		if (ControllerBase != nullptr)
		{
			UnitBase->SetWalkSpeed(UnitBase->Attributes->GetRunSpeed());
			// You can use the controller here

			
			// For example, you can use the MoveToLocationUEPathFinding function if it's defined in your controller class.
			UnitBase->SetUEPathfinding = false;
			ControllerBase->MoveToLocationUEPathFinding(UnitBase, Location);
		}
	}
}

void AUnitControllerBase::SetUEPathfindingTo(AUnitBase* UnitBase, float DeltaSeconds, FVector Location)
{
	if(!UnitBase->SetUEPathfinding)
		return;
		
	APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	if(PlayerController)
	{
		AControllerBase* ControllerBase = Cast<AControllerBase>(PlayerController);
		if (ControllerBase != nullptr)
		{
			UnitBase->SetWalkSpeed(UnitBase->Attributes->GetRunSpeed());
			// You can use the controller here
			// For example, you can use the MoveToLocationUEPathFinding function if it's defined in your controller class.
			UnitBase->SetUEPathfinding = false;
			ControllerBase->MoveToLocationUEPathFinding(UnitBase, Location);
	
		}
	}
}

void AUnitControllerBase::CreateProjectile_Implementation(AUnitBase* UnitBase)
{
	if(UnitBase->UseProjectile && !ProjectileSpawned)
	{
		UnitBase->SpawnProjectile(UnitBase->UnitToChase, UnitBase);
		ProjectileSpawned = true;
	}
}

