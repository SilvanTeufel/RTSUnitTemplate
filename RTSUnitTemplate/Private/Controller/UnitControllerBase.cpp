// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Controller/UnitControllerBase.h"
#include <string>
#include "Characters/UnitBase.h"
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
						CurrentUnit->AddMovementInput(FVector(0.f), CurrentUnit->RunSpeedScale);
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
				if(CurrentUnit->GetUnitState() != UnitData::Attack && CurrentUnit->GetUnitState() != UnitData::Run && CurrentUnit->UnitToChase->GetHealth() < CurrentUnit->UnitToChase->GetMaxHealth())
				{
					CurrentUnit->AddMovementInput(FVector(0.f), CurrentUnit->RunSpeedScale);
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
			//if(UnitBase->TeamId == 2)UE_LOG(LogTemp, Warning, TEXT("Patrol"));
				
			if(UnitBase->UsingUEPathfindingPatrol)
				PatrolUEPathfinding(UnitBase, DeltaSeconds);
			else
				Patrol(UnitBase, DeltaSeconds);
		}
		break;
		case UnitData::Run:
		{
			//if(UnitBase->TeamId == 2)UE_LOG(LogTemp, Warning, TEXT("Run"));
				
			if(UnitBase->UEPathfindingUsed)
				RunUEPathfinding(UnitBase, DeltaSeconds);
			else
				Run(UnitBase, DeltaSeconds);
		}
		break;
		case UnitData::Chase:
		{
			//if(UnitBase->TeamId == 2)UE_LOG(LogTemp, Warning, TEXT("Chase"));
				
			Chase(UnitBase, DeltaSeconds);
		}
		break;
		case UnitData::Attack:
		{
			//if(!UnitBase->IsFriendly)UE_LOG(LogTemp, Warning, TEXT("Attack"));
			Attack(UnitBase, DeltaSeconds);
		}
		break;
		case UnitData::Pause:
		{
			//if(UnitBase->TeamId == 1)UE_LOG(LogTemp, Warning, TEXT("Pause"));
			Pause(UnitBase, DeltaSeconds);
		}
		break;

		case UnitData::IsAttacked:
		{
			//if(UnitBase->TeamId == 1)UE_LOG(LogTemp, Warning, TEXT("IsAttacked"));
			IsAttacked(UnitBase, DeltaSeconds);
		}
		break;

		case UnitData::Idle:
		{
			//if(UnitBase->TeamId == 1)UE_LOG(LogTemp, Warning, TEXT("Idle"));
			Idle(UnitBase, DeltaSeconds);
		}
		break;
		default:
			{
				UnitBase->SetUnitState(UnitData::Idle);
			}
		break;
		}

	if (UnitBase->GetHealth() <= 0.f && UnitBase->GetUnitState() != UnitData::Dead) {
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
	
	if (DistanceToUnitToChase < (UnitBase->Range + RangeOffset))
		return true;
	
	return false;
}

void AUnitControllerBase::Dead(AUnitBase* UnitBase, float DeltaSeconds)
{
	UnitBase->SetWalkSpeed(0);
				
	UnitBase->UnitControlTimer = (UnitBase->UnitControlTimer + DeltaSeconds);

	FVector ActorLocation = UnitBase->GetActorLocation();
				
	UnitBase->SetActorLocation(FVector(ActorLocation.X + 0.f,ActorLocation.Y + 0.f,ActorLocation.Z -1.f));

	if (UnitBase->UnitControlTimer >= DespawnTime) {
		if(UnitBase->DestroyAfterDeath) UnitBase->Destroy(true, false);
	}
}

void AUnitControllerBase::Patrol(AUnitBase* UnitBase, float DeltaSeconds)
{
	UnitBase->SetWalkSpeed(UnitBase->MaxRunSpeed);
				
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
			UnitBase->AddMovementInput(ADirection, UnitBase->RunSpeedScale);
		}else
		{
			const FVector ADirection = UKismetMathLibrary::GetDirectionUnitVector(UnitBase->GetActorLocation(), WaypointLocation);
			UnitBase->AddMovementInput(ADirection, UnitBase->RunSpeedScale);
		}
	}
	else
	{
		UnitBase->SetUnitState(UnitData::Idle);
	}
}

void AUnitControllerBase::Run(AUnitBase* UnitBase, float DeltaSeconds)
{
	
	if(UnitBase->ToggleUnitDetection && UnitBase->UnitToChase)
	{
		if(UnitBase->SetNextUnitToChase())
		{
			UnitBase->SetUnitState(UnitData::Chase);
		}
	}
				
	UnitBase->SetWalkSpeed(UnitBase->MaxRunSpeed);
				
	const FVector UnitLocation = UnitBase->GetActorLocation();
				
	if(UnitBase->IsFlying)
	{
		UnitBase->RunLocation = FVector(UnitBase->RunLocation.X, UnitBase->RunLocation.Y, UnitBase->FlyHeight);
	}
	
	const FVector ADirection = UKismetMathLibrary::GetDirectionUnitVector(UnitLocation, UnitBase->RunLocation);
	UnitBase->AddMovementInput(ADirection, UnitBase->RunSpeedScale);

	const float Distance = sqrt((UnitLocation.X-UnitBase->RunLocation.X)*(UnitLocation.X-UnitBase->RunLocation.X)+(UnitLocation.Y-UnitBase->RunLocation.Y)*(UnitLocation.Y-UnitBase->RunLocation.Y));

	if (Distance <= UnitBase->StopRunTolerance) {
		UnitBase->SetUnitState(UnitData::Idle);
	}
	UnitBase->UnitControlTimer = 0.f;
}

void AUnitControllerBase::Chase(AUnitBase* UnitBase, float DeltaSeconds)
{
					if(!UnitBase->SetNextUnitToChase())
					{
						UnitBase->SetUEPathfinding = true;
						UnitBase->SetUnitState(UnitBase->UnitStatePlaceholder);
					}else if (UnitBase->UnitToChase) {
    				UnitBase->SetWalkSpeed(UnitBase->MaxRunSpeed);
    				
    				RotateToAttackUnit(UnitBase, UnitBase->UnitToChase);
    				DistanceToUnitToChase = GetPawn()->GetDistanceTo(UnitBase->UnitToChase);
    
    					AUnitBase* UnitToChase = UnitBase->UnitToChase;
 
    					if (IsUnitToChaseInRange(UnitBase)) {
    						if(UnitBase->UnitControlTimer >= PauseDuration)
    						{
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
    						
    						const FVector ADirection = UKismetMathLibrary::GetDirectionUnitVector(UnitBase->GetActorLocation(), UnitToChaseLocation);
    						
    						UnitBase->AddMovementInput(ADirection, UnitBase->RunSpeedScale);
    						
    						//SetUEPathfinding(UnitBase, DeltaSeconds);
    						//SetUEPathfindingTo(UnitBase, DeltaSeconds, UnitToChaseLocation);
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
				
				if(UnitBase->UnitToChase->GetShield() <= 0)
					UnitBase->UnitToChase->SetHealth(UnitBase->UnitToChase->GetHealth()-UnitBase->AttackDamage);
				else
					UnitBase->UnitToChase->SetShield(UnitBase->UnitToChase->GetShield()-UnitBase->AttackDamage);
				
				if(UnitBase->UnitToChase->GetUnitState() != UnitData::Run)
				UnitBase->UnitToChase->SetUnitState( UnitData::IsAttacked );
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
		UnitBase->SetWalkSpeed(UnitBase->MaxRunSpeed);
		
		if (IsUnitToChaseInRange(UnitBase)) {
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
		UnitBase->SetWalkSpeed(UnitBase->MaxRunSpeed);
		UnitBase->SetUnitState(UnitData::Pause);
	}
}

void AUnitControllerBase::Idle(AUnitBase* UnitBase, float DeltaSeconds)
{
	UnitBase->SetWalkSpeed(0);
	if(UnitBase->UnitsToChase.Num())
	{
		UnitBase->SetUnitState(UnitData::Chase);
	}
}

void AUnitControllerBase::RunUEPathfinding(AUnitBase* UnitBase, float DeltaSeconds)
{

	if(UnitBase->ToggleUnitDetection && UnitBase->UnitToChase)
	{
		if(UnitBase->SetNextUnitToChase())
		{
			UnitBase->SetUnitState(UnitData::Chase);
		}
	}
	
	UnitBase->SetWalkSpeed(UnitBase->MaxRunSpeed);

	const FVector UnitLocation = UnitBase->GetActorLocation();
	const float Distance = sqrt((UnitLocation.X-UnitBase->RunLocation.X)*(UnitLocation.X-UnitBase->RunLocation.X)+(UnitLocation.Y-UnitBase->RunLocation.Y)*(UnitLocation.Y-UnitBase->RunLocation.Y));

	if (Distance <= UnitBase->StopRunTolerance) {
		UnitBase->SetUnitState(UnitData::Idle);
	}
}

void AUnitControllerBase::PatrolUEPathfinding(AUnitBase* UnitBase, float DeltaSeconds)
{
	UnitBase->SetWalkSpeed(UnitBase->MaxRunSpeed);
				
	if(UnitBase->UnitToChase && UnitBase->UnitToChase->GetUnitState() != UnitData::Dead)
	{
		if(UnitBase->SetNextUnitToChase())
		{
			UnitBase->SetUEPathfinding = true;
			UnitBase->SetUnitState(UnitData::Chase);
		}
		
	} else if (UnitBase->NextWaypoint != nullptr)
	{
		SetUEPathfinding(UnitBase, DeltaSeconds);
	}
	else
	{
		UnitBase->SetUnitState(UnitData::Idle);
	}
}
void AUnitControllerBase::SetUEPathfinding(AUnitBase* UnitBase, float DeltaSeconds)
{
	if(!UnitBase->SetUEPathfinding)
		return;
		
	APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	if(PlayerController)
	{
		AControllerBase* ControllerBase = Cast<AControllerBase>(PlayerController);
		if (ControllerBase != nullptr)
		{
			UnitBase->SetWalkSpeed(UnitBase->MaxRunSpeed);
			// You can use the controller here
			// For example, you can use the MoveToLocationUEPathFinding function if it's defined in your controller class.
			UnitBase->SetUEPathfinding = false;
			ControllerBase->MoveToLocationUEPathFinding(UnitBase, UnitBase->NextWaypoint->GetActorLocation());
			
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
			UnitBase->SetWalkSpeed(UnitBase->MaxRunSpeed);
			// You can use the controller here
			// For example, you can use the MoveToLocationUEPathFinding function if it's defined in your controller class.
			UnitBase->SetUEPathfinding = false;
			ControllerBase->MoveToLocationUEPathFinding(UnitBase, Location);
	
		}
	}
}

void AUnitControllerBase::CreateProjectile(AUnitBase* UnitBase)
{
	if(UnitBase->UseProjectile && !ProjectileSpawned)
	{
		UnitBase->SpawnProjectile(UnitBase->UnitToChase, UnitBase);
		ProjectileSpawned = true;
	}
}

