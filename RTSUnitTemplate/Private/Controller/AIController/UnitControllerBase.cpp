// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Controller/AIController/UnitControllerBase.h"
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
#include "Controller/PlayerController/ControllerBase.h"
#include "Net/UnrealNetwork.h"
#include "GAS/GameplayAbilityBase.h"
#include "NavigationSystem.h"
#include "GameModes/RTSGameModeBase.h"

#include "Navigation/PathFollowingComponent.h"
#include "Widgets/UnitBaseHealthBar.h"

void AUnitControllerBase::DetectAndLoseUnits()
{
	// Assuming UnitBase is an accessible variable or parameter
	 // Replace this with actual reference to UnitBase
	//UE_LOG(LogTemp, Warning, TEXT("DetectAndLoseUnits!!!!!!!!!!! "));
	if (MyUnitBase)
	{
		bool SetState = MyUnitBase->GetToggleUnitDetection();

		if(SetState || MyUnitBase->GetUnitState() == UnitData::Patrol || MyUnitBase->GetUnitState() == UnitData::PatrolRandom || MyUnitBase->GetUnitState() == UnitData::PatrolIdle)
		{
			//UE_LOG(LogTemp, Warning, TEXT("SetState TRUE! "));
			DetectUnitsAndSetState(MyUnitBase, 0, true);
			LoseUnitToChase(MyUnitBase);
		}
		else
		{
			//UE_LOG(LogTemp, Warning, TEXT("SetState FALSE! "));
			DetectUnitsAndSetState(MyUnitBase, 0, SetState);
			LoseUnitToChase(MyUnitBase);
		}
	}
}

AUnitControllerBase::AUnitControllerBase()
{
	
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = TickInterval;
}

void AUnitControllerBase::BeginPlay()
{
	Super::BeginPlay();

	if(MyUnitBase)
	{
		if(!ControllerBase)
		{
			ControllerBase = Cast<AControllerBase>(GetWorld()->GetFirstPlayerController());
			
			if(ControllerBase)
			{
				MyUnitBase = Cast<AUnitBase>(GetPawn());
				RTSGameMode = Cast<ARTSGameModeBase>(GetWorld()->GetAuthGameMode());
				
				if(MyUnitBase->HealthWidgetComp)HealthBarWidget = Cast<UUnitBaseHealthBar>(MyUnitBase->HealthWidgetComp->GetUserWidgetObject());
			
			}
		}
	}
}

void AUnitControllerBase::OnPossess(APawn* PawN)
{
	Super::OnPossess(PawN);

	MyUnitBase = Cast<AUnitBase>(GetPawn());

	if(MyUnitBase)
	{
		SightRadius = MyUnitBase->SightRadius;
		if(!ControllerBase)
		{
			ControllerBase = Cast<AControllerBase>(GetWorld()->GetFirstPlayerController());
		
			if(ControllerBase)
			{
				RTSGameMode = Cast<ARTSGameModeBase>(GetWorld()->GetAuthGameMode());
				
				if(MyUnitBase->HealthWidgetComp)HealthBarWidget = Cast<UUnitBaseHealthBar>(MyUnitBase->HealthWidgetComp->GetUserWidgetObject());
			
			}
		}
	}

}


void AUnitControllerBase::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AUnitControllerBase, UnitDetectionTimer);
	DOREPLIFETIME(AUnitControllerBase, NewDetectionTime);
	DOREPLIFETIME(AUnitControllerBase, IsUnitDetected);

}


void AUnitControllerBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UnitControlStateMachine(MyUnitBase, DeltaSeconds);
	
	
	if(ControllerBase && MyUnitBase)
	{
		//MyUnitBase->CheckVisibility(ControllerBase->SelectableTeamId);
		//MyUnitBase->UpdateFogOfWarLight(ControllerBase->SelectableTeamId, SightRadius);
	}
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

void AUnitControllerBase::OnUnitDetected(const TArray<AActor*>& DetectedUnits, bool SetState)
{
	//AUnitBase* CurrentUnit = Cast<AUnitBase>(GetPawn());
	AUnitBase* CurrentUnit = MyUnitBase;
	if (!CurrentUnit || CurrentUnit->GetUnitState() == UnitData::Dead || CurrentUnit->GetUnitState() == UnitData::Evasion) return;

	// Loop through each detected unit
	for (AActor* Actor : DetectedUnits)
	{
		AUnitBase* DetectedUnit = Cast<AUnitBase>(Actor);
		if (DetectedUnit && DetectedUnit->GetUnitState() != UnitData::Dead)
		{
			bool isFriendlyUnit = DetectedUnit->TeamId == CurrentUnit->TeamId;
			if ((DetectFriendlyUnits && isFriendlyUnit) || (!DetectFriendlyUnits && !isFriendlyUnit))
			{
				CurrentUnit->UnitsToChase.Emplace(DetectedUnit);
			}
		}
		
	}

	// The actual state change logic is refactored here, outside the loop.

	if(!SetState) return;
	
	//if(!CurrentUnit->UnitToChase)
	if (CurrentUnit->SetNextUnitToChase())
	{
		bool isUnitChasing = CurrentUnit->GetUnitState() == UnitData::Chase;
		bool canChangeState = !isUnitChasing && CurrentUnit->GetUnitState() != UnitData::Attack && CurrentUnit->GetUnitState() != UnitData::Casting;
        
		if (DetectFriendlyUnits)
		{
			bool shouldChase = canChangeState && CurrentUnit->UnitToChase->Attributes->GetHealth() < CurrentUnit->UnitToChase->Attributes->GetMaxHealth();
			if (shouldChase) CurrentUnit->SetUnitState(UnitData::Chase);
		}
		else if (canChangeState)
		{
			CurrentUnit->SetUnitState(UnitData::Chase);
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

void AUnitControllerBase::CheckUnitDetectionTimer(float DeltaSeconds)
{
	if(IsUnitDetected)
	{
		UnitDetectionTimer += DeltaSeconds;
		if(UnitDetectionTimer >= NewDetectionTime)
		{
			UnitDetectionTimer = 0.f;
			IsUnitDetected = false;
		}
	}
}

void AUnitControllerBase::UnitControlStateMachine(AUnitBase* UnitBase, float DeltaSeconds)
{

		//UE_LOG(LogTemp, Warning, TEXT("Controller UnitBase->Attributes! %f"), UnitBase->Attributes->GetAttackDamage());
		if(!UnitBase) return;
	
		if (UnitBase->Attributes && UnitBase->Attributes->GetHealth() <= 0.f && UnitBase->GetUnitState() != UnitData::Dead) {
			KillUnitBase(UnitBase);
			UnitBase->UnitControlTimer = 0.f;
		}

		CheckUnitDetectionTimer(DeltaSeconds);
		//DetectAndLoseUnits();
	
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
			//UE_LOG(LogTemp, Warning, TEXT("Patrol"));
				
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
					UnitBase->SetUEPathfinding = true;
					UnitBase->SetUnitState(UnitData::Chase);
					return;
				}
				
				DetectAndLoseUnits();
				
				if(UnitBase->GetUnitState() != UnitData::Chase)
				{
					UnitBase->SetWalkSpeed(UnitBase->Attributes->GetRunSpeed());
					SetUEPathfindingRandomLocation(UnitBase, DeltaSeconds);
				}

			}
			break;
		case UnitData::PatrolIdle:
			{
				//if(UnitBase->TeamId == 3)UE_LOG(LogTemp, Warning, TEXT("PatrolIdle"));
				if(UnitBase->SetNextUnitToChase())
				{
					UnitBase->SetUEPathfinding = true;
					UnitBase->SetUnitState(UnitData::Chase);
					return;
				}
				
				DetectAndLoseUnits();
				
				if(UnitBase->GetUnitState() != UnitData::Chase)
				{
					UnitBase->SetWalkSpeed(0);
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
			//if(UnitBase->TeamId == 1)UE_LOG(LogTemp, Warning, TEXT("IsAttacked"));
			IsAttacked(UnitBase, DeltaSeconds);
		}
		break;
		case UnitData::EvasionChase:
			{
				if(UnitBase->UnitControlTimer >= 7.f)
				{
					UnitBase->CollisionUnit = nullptr;
					UnitBase->UnitControlTimer = 0.f;
					UnitBase->SetUEPathfinding = true;
					UnitBase->SetUnitState(UnitBase->UnitStatePlaceholder);
	
				}else if(UnitBase->CollisionUnit)
				{
					EvasionChase(UnitBase, DeltaSeconds, UnitBase->CollisionLocation);
					UnitBase->UnitControlTimer += DeltaSeconds;
				}
			}
		break;
		case UnitData::Evasion:
		{

				
			//if(UnitBase->TeamId == 3)UE_LOG(LogTemp, Warning, TEXT("Evasion"));
			
			if(UnitBase->CollisionUnit)
			{
				EvasionIdle(UnitBase, UnitBase->CollisionUnit->GetActorLocation());
				UnitBase->UnitControlTimer += DeltaSeconds;
			}else
			{
				UnitBase->SetUnitState(UnitData::Run);
			}
				
		}
		break;
		case UnitData::Rooted:
			{
				//if(UnitBase->TeamId == 3)UE_LOG(LogTemp, Warning, TEXT("Idle"));
				Rooted(UnitBase, DeltaSeconds);
			}
			break;
		case UnitData::Casting:
			{
				//if(UnitBase->TeamId == 3)UE_LOG(LogTemp, Warning, TEXT("Idle"));
				Casting(UnitBase, DeltaSeconds);
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
}

void AUnitControllerBase::Rooted(AUnitBase* UnitBase, float DeltaSeconds)
{
	UnitBase->SetWalkSpeed(0);
	UnitBase->UnitControlTimer += DeltaSeconds;
	if (UnitBase->UnitControlTimer > IsRootedDuration)
	{
		UnitBase->SetWalkSpeed(UnitBase->Attributes->GetRunSpeed());
		UnitBase->UnitControlTimer = 0.f;
		UnitBase->SetUnitState(UnitBase->UnitStatePlaceholder);
	}
}

void AUnitControllerBase::Casting(AUnitBase* UnitBase, float DeltaSeconds)
{
	if (!UnitBase || !UnitBase->Attributes) return;
	
	//DetectUnits(UnitBase, DeltaSeconds, false);
	
	UnitBase->SetWalkSpeed(0);
	RotateToAttackUnit(UnitBase, UnitBase->UnitToChase);
	UnitBase->UnitControlTimer += DeltaSeconds;
	/*
	if(UnitBase->UnitToChase && UnitBase->UnitToChase->GetUnitState() == UnitData::Dead)
	{
		UnitBase->SetWalkSpeed(UnitBase->Attributes->GetRunSpeed());
		UnitBase->UnitControlTimer = 0.f;
		UnitBase->SetUnitState(UnitBase->UnitStatePlaceholder);
	}else
	*/
	if (UnitBase->UnitControlTimer > UnitBase->CastTime)
	{
		if (UnitBase->ActivatedAbilityInstance)
			UnitBase->ActivatedAbilityInstance->OnAbilityCastComplete();
		
		UnitBase->SetWalkSpeed(UnitBase->Attributes->GetRunSpeed());
		UnitBase->UnitControlTimer = 0.f;
		UnitBase->SetUnitState(UnitBase->UnitStatePlaceholder);
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

	UnitBase->SetWalkSpeed(0);			
	UnitBase->UnitControlTimer = (UnitBase->UnitControlTimer + DeltaSeconds);

	
	if(RTSGameMode)
	{
		RTSGameMode->AllUnits.Remove(UnitBase);
	}

	
	UnitBase->SpawnPickupsArray();

	if (UnitBase->UnitControlTimer >= DespawnTime) {
		if(UnitBase->DestroyAfterDeath) UnitBase->Destroy(true, false);
	}
}


void AUnitControllerBase::DetectUnitsFromGameMode(AUnitBase* DetectingUnit, TArray<AActor*>& DetectedUnits, float Sight, float LoseSight, bool DetectFriendly, int PlayerTeamId)
{
	if(!RTSGameMode) return;
	//TArray<int> DetectedCount;
	DetectingUnit->IsInFog = true;
	
	for (int32 i = 0; i < RTSGameMode->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(RTSGameMode->AllUnits[i]);
		

		if (Unit && !DetectFriendlyUnits && Unit->TeamId != DetectingUnit->TeamId)
		{
			float Distance = FVector::Dist(DetectingUnit->GetActorLocation(), Unit->GetActorLocation());
			
			if (Distance <= Sight &&
				Unit->GetUnitState() != UnitData::Dead &&
				DetectingUnit->GetUnitState() != UnitData::Dead)
			{

				DetectedUnits.Emplace(Unit);
			}
		}else if (Unit && DetectFriendlyUnits && Unit->TeamId == DetectingUnit->TeamId)
		{

			float Distance = FVector::Dist(DetectingUnit->GetActorLocation(), Unit->GetActorLocation());

			if (Distance <= Sight)
				DetectedUnits.Emplace(Unit);

		}
	}
	
}


void AUnitControllerBase::DetectUnitsAndSetState(AUnitBase* UnitBase, float DeltaSeconds, bool SetState)
{

	if(IsUnitDetected) return;
	
	if (ControllerBase)
	{
	
		TArray<AActor*> DetectedUnits;
		DetectUnitsFromGameMode(UnitBase, DetectedUnits, SightRadius, LoseSightRadius, DetectFriendlyUnits,ControllerBase->SelectableTeamId);
		OnUnitDetected(DetectedUnits, SetState);
		IsUnitDetected = true;

	}

}

void AUnitControllerBase::Patrol(AUnitBase* UnitBase, float DeltaSeconds)
{

	if(UnitBase->SetNextUnitToChase())
	{
		UnitBase->SetUEPathfinding = true;
		UnitBase->SetUnitState(UnitData::Chase);
		return;
	}
	
	UnitBase->SetWalkSpeed(UnitBase->Attributes->GetRunSpeed());

	DetectAndLoseUnits();
	
	if(UnitBase->GetUnitState() == UnitData::Chase)//UnitBase->UnitToChase && UnitBase->UnitToChase->GetUnitState() != UnitData::Dead)
	{
		//if(UnitBase->SetNextUnitToChase())
			//UnitBase->SetUnitState(UnitData::Chase);
		
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
	if(UnitBase->GetToggleUnitDetection())
	{
		if(UnitBase->SetNextUnitToChase())
		{
			UnitBase->SetUEPathfinding = true;
			UnitBase->SetUnitState(UnitData::Chase);
			return;
		}
	}
	if(UnitBase->CollisionUnit && UnitBase->CollisionUnit->TeamId == UnitBase->TeamId && UnitBase->CollisionUnit->GetUnitState() != UnitData::Dead)
	{
		UnitBase->SetUnitState(UnitData::Evasion);
		UnitBase->UnitStatePlaceholder = UnitData::Run;
		return;
	}
	
	DetectAndLoseUnits();
				
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
    if (!UnitBase) return;
	
    // Check for immediate collision with an enemy unit.
	//DetectUnits(UnitBase, DeltaSeconds, false);
	LoseUnitToChase(UnitBase);
	
	
	if(UnitBase->CollisionUnit 
       && UnitBase->CollisionUnit->GetUnitState() != UnitData::Dead && UnitBase->CollisionUnit->TeamId != UnitBase->TeamId)
    {
    		UnitBase->UnitToChase = UnitBase->CollisionUnit;
    		UnitBase->UnitsToChase.Emplace(UnitBase->CollisionUnit);
    		UnitBase->CollisionUnit = nullptr;
    } else if (!UnitBase->UnitToChase) // If no unit is being chased, try to find one, otherwise set the pathfinding.
    {
        UnitBase->SetUEPathfinding = true;
        UnitBase->SetUnitState(UnitBase->UnitStatePlaceholder);
    } else
    {
       UnitBase->SetWalkSpeed(UnitBase->Attributes->GetRunSpeed());
       RotateToAttackUnit(UnitBase, UnitBase->UnitToChase);

        if (IsUnitToChaseInRange(UnitBase))
        {
            // Check if we can attack or should pause.
            if(UnitBase->UnitControlTimer >= UnitBase->PauseDuration)
            {
                UnitBase->ServerStartAttackEvent_Implementation();
                UnitBase->SetUnitState(UnitData::Attack);
                ActivateCombatAbilities(UnitBase);
            }
            else
            {
                UnitBase->SetUnitState(UnitData::Pause);
            }
        }
        else
        {
        	if(UnitBase->CollisionUnit && UnitBase->CollisionUnit->GetUnitState() != UnitData::Dead && UnitBase->CollisionUnit->TeamId == UnitBase->TeamId)
        	{
        		StopMovementCommand(UnitBase);
        		UnitBase->UnitControlTimer = 0.f;
        		UnitBase->SetUnitState(UnitData::EvasionChase);
        		return;
        	}
        	
            // If the target is flying and we're not, check if we should idle.
            if(UnitBase->UnitToChase->IsFlying && !UnitBase->IsFlying 
               && DistanceToUnitToChase > UnitBase->StopRunToleranceForFlying)
            {
                UnitBase->SetUnitState(UnitData::Idle);
            }
            // If we are flying, adjust the chase location.
            FVector UnitToChaseLocation = CalculateChaseLocation(UnitBase);

            UnitBase->ActivateAbilityByInputID(UnitBase->ThrowAbilityID, UnitBase->ThrowAbilities);
            UnitBase->ActivateAbilityByInputID(UnitBase->OffensiveAbilityID, UnitBase->OffensiveAbilities);
            UnitBase->SetUEPathfinding = true;
            SetUEPathfinding(UnitBase, DeltaSeconds, UnitToChaseLocation);
        }

        // If we lose sight of the unit, reset chase.
    }
}

void AUnitControllerBase::ActivateCombatAbilities(AUnitBase* UnitBase)
{
    UnitBase->ActivateAbilityByInputID(UnitBase->AttackAbilityID, UnitBase->AttackAbilities);
    UnitBase->ActivateAbilityByInputID(UnitBase->ThrowAbilityID, UnitBase->ThrowAbilities);
    if(UnitBase->Attributes->GetRange() >= 600.f)
        UnitBase->ActivateAbilityByInputID(UnitBase->OffensiveAbilityID, UnitBase->OffensiveAbilities);
}


FVector AUnitControllerBase::CalculateChaseLocation(AUnitBase* UnitBase)
{
    FVector UnitToChaseLocation = UnitBase->UnitToChase->GetActorLocation();
    if(UnitBase->IsFlying)
    {
        UnitToChaseLocation.Z = UnitBase->FlyHeight;
    }
    return UnitToChaseLocation;
}

void AUnitControllerBase::LoseUnitToChase(AUnitBase* UnitBase)
{
	
				if (UnitBase->UnitToChase)
				{
					float Distance = GetPawn()->GetDistanceTo(UnitBase->UnitToChase);
					if(Distance > LoseSightRadius)
					{
						//UnitBase->UnitsToChase[i]->SetVisibility(false, ControllerBase->SelectableTeamId);
						UnitBase->UnitsToChase.Remove(UnitBase->UnitToChase);
						UnitBase->UnitToChase = nullptr;
					}
				}
	
	

	/*
    if(!UnitBase->TeamId && UnitBase->FollowPath)
    {
        // Handle path reset logic.
       ResetPath(UnitBase);
    }
    else
    {
        UnitBase->SetUEPathfinding = true;
        UnitBase->SetUnitState(UnitBase->UnitStatePlaceholder);
    	
    }
	*/
}

void AUnitControllerBase::ResetPath(AUnitBase* UnitBase)
{
    UnitBase->RunLocationArray.Empty();
    UnitBase->RunLocationArrayIterator = 0;
    UnitBase->DijkstraStartPoint = UnitBase->GetActorLocation();
    UnitBase->DijkstraEndPoint = UnitBase->NextWaypoint->GetActorLocation();
    UnitBase->DijkstraSetPath = true;
    UnitBase->FollowPath = false;
    UnitBase->SetUEPathfinding = true;
    UnitBase->SetUnitState(UnitBase->UnitStatePlaceholder);
}

void AUnitControllerBase::Attack(AUnitBase* UnitBase, float DeltaSeconds)
{
	if (!UnitBase) return;
	//DetectUnits(UnitBase, DeltaSeconds, false);
	
	UnitBase->SetWalkSpeed(0);	
	RotateToAttackUnit(UnitBase, UnitBase->UnitToChase);
	UnitBase->UnitControlTimer = (UnitBase->UnitControlTimer + DeltaSeconds);

	if(UnitBase->SetNextUnitToChase())
	{
		
		if (UnitBase->UnitControlTimer > AttackDuration + UnitBase->PauseDuration) {
		
			if(!UnitBase->UseProjectile )
			{
				// Attack without Projectile
				if(IsUnitToChaseInRange(UnitBase))
				{
					float NewDamage = UnitBase->Attributes->GetAttackDamage() - UnitBase->UnitToChase->Attributes->GetArmor();
			
					if(UnitBase->IsDoingMagicDamage)
						NewDamage = UnitBase->Attributes->GetAttackDamage() - UnitBase->UnitToChase->Attributes->GetMagicResistance();
				
					if(UnitBase->UnitToChase->Attributes->GetShield() <= 0)
						UnitBase->UnitToChase->SetHealth_Implementation(UnitBase->UnitToChase->Attributes->GetHealth()-NewDamage);
					else
						UnitBase->UnitToChase->SetShield_Implementation(UnitBase->UnitToChase->Attributes->GetShield()-UnitBase->Attributes->GetAttackDamage());

					UnitBase->LevelData.Experience++;
					
					if (HealthBarWidget)
					{
						HealthBarWidget->UpdateWidget();
					}
					
					UnitBase->ServerMeeleImpactEvent();
					UnitBase->UnitToChase->ActivateAbilityByInputID(UnitBase->UnitToChase->DefensiveAbilityID, UnitBase->UnitToChase->DefensiveAbilities);
				
					if (!UnitBase->UnitToChase->UnitsToChase.Contains(UnitBase))
					{
						// If not, add UnitBase to the array
						UnitBase->UnitToChase->UnitsToChase.Emplace(UnitBase);
						UnitBase->UnitToChase->SetNextUnitToChase();
					}
				
					if(UnitBase->UnitToChase->GetUnitState() != UnitData::Run &&
						UnitBase->UnitToChase->GetUnitState() != UnitData::Attack &&
						UnitBase->UnitToChase->GetUnitState() != UnitData::Casting &&
						UnitBase->UnitToChase->GetUnitState() != UnitData::Rooted &&
						UnitBase->UnitToChase->GetUnitState() != UnitData::Pause)
					{
						UnitBase->UnitToChase->UnitControlTimer = 0.f;
						UnitBase->UnitToChase->SetUnitState( UnitData::IsAttacked );
					}else if(UnitBase->UnitToChase->GetUnitState() == UnitData::Casting)
					{
						UnitBase->UnitToChase->UnitControlTimer -= UnitBase->UnitToChase->ReduceCastTime;
					}else if(UnitBase->UnitToChase->GetUnitState() == UnitData::Rooted)
					{
						UnitBase->UnitToChase->UnitControlTimer -= UnitBase->UnitToChase->ReduceRootedTime;
					}
				}else
				{
					UnitBase->SetUnitState( UnitData::Chase );
				}
			}

			ProjectileSpawned = false;
			UnitBase->UnitControlTimer = 0.f;
			UnitBase->SetUnitState( UnitData::Pause);
		}
	}else
	{
		UnitBase->SetUnitState(UnitBase->UnitStatePlaceholder);
	}
}


void AUnitControllerBase::Pause(AUnitBase* UnitBase, float DeltaSeconds)
{
	if (!UnitBase) return;
	//DetectUnits(UnitBase, DeltaSeconds, false);
	
	UnitBase->SetWalkSpeed(0);
	RotateToAttackUnit(UnitBase, UnitBase->UnitToChase);
				
	UnitBase->UnitControlTimer = (UnitBase->UnitControlTimer + DeltaSeconds);
	
	if(!UnitBase->SetNextUnitToChase()) {
		UnitBase->SetUEPathfinding = true;
		UnitBase->SetUnitState( UnitBase->UnitStatePlaceholder );
	} else if (UnitBase->UnitControlTimer > UnitBase->PauseDuration) {
		
		ProjectileSpawned = false;

		UnitBase->SetWalkSpeed(UnitBase->Attributes->GetRunSpeed());
		
		if (IsUnitToChaseInRange(UnitBase)) {
				UnitBase->ServerStartAttackEvent_Implementation();
				UnitBase->SetUnitState(UnitData::Attack);
				UnitBase->ActivateAbilityByInputID(UnitBase->AttackAbilityID, UnitBase->AttackAbilities);
				UnitBase->ActivateAbilityByInputID(UnitBase->ThrowAbilityID, UnitBase->ThrowAbilities);
				if(UnitBase->Attributes->GetRange() >= 600.f) UnitBase->ActivateAbilityByInputID(UnitBase->OffensiveAbilityID, UnitBase->OffensiveAbilities);
			
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

	//DetectUnits(UnitBase, DeltaSeconds, true);
	//LoseUnitToChase(UnitBase);
	
	if(UnitBase->CollisionUnit && UnitBase->CollisionUnit->TeamId != UnitBase->TeamId && UnitBase->CollisionUnit->GetUnitState() != UnitData::Dead && !UnitBase->IsOnPlattform)
	{
		UnitBase->UnitToChase = UnitBase->CollisionUnit;
		UnitBase->UnitsToChase.Emplace(UnitBase->CollisionUnit);
		UnitBase->CollisionUnit = nullptr;
	}else if(UnitBase->CollisionUnit && UnitBase->CollisionUnit->TeamId == UnitBase->TeamId && UnitBase->CollisionUnit->GetUnitState() != UnitData::Dead && !UnitBase->IsOnPlattform)
	{
		UnitBase->UnitStatePlaceholder = UnitData::Idle;
		UnitBase->RunLocation = UnitBase->GetActorLocation();
		UnitBase->SetUnitState(UnitData::Evasion);
	}

	if(UnitBase->SetNextUnitToChase() && !UnitBase->IsOnPlattform)
	{
		UnitBase->SetUnitState(UnitData::Chase);
	}else if(!UnitBase->IsOnPlattform)
		SetUnitBackToPatrol(UnitBase, DeltaSeconds);
}

void AUnitControllerBase::EvasionChase(AUnitBase* UnitBase, float DeltaSeconds, FVector CollisionLocation)
{
	// Ensure the unit runs at its designated run speed.
	UnitBase->SetWalkSpeed(UnitBase->Attributes->GetRunSpeed());
				
	const FVector UnitLocation = UnitBase->GetActorLocation();
	const FVector CollisionUnitLocation = UnitBase->CollisionUnit->GetActorLocation();
	// Adjust collision location for flying units.
	if(UnitBase->IsFlying)
	{
		CollisionLocation.Z = UnitBase->FlyHeight;
	}
	
	FVector AwayDirection = (UnitLocation - CollisionUnitLocation).GetSafeNormal();
	
	const FVector RotatedDirection = FRotator(0.f,40.f,0.f).RotateVector(AwayDirection);
	const FVector LocationToGo = UnitBase->GetActorLocation() + RotatedDirection*(UnitBase->EvadeDistanceChase+100.f);
	UnitBase->SetUEPathfinding = true;
	SetUEPathfinding(UnitBase, DeltaSeconds, LocationToGo);
	
	// Simplify distance calculation using FVector::Dist.
	float Distance = FVector::Dist(UnitLocation, CollisionLocation);
	
	if (Distance >= UnitBase->EvadeDistanceChase) {
		UnitBase->SetUEPathfinding = true;
		UnitBase->SetUnitState(UnitBase->UnitStatePlaceholder);
		UnitBase->CollisionUnit = nullptr;
		UnitBase->UnitControlTimer = 0.f;
	}
}

void AUnitControllerBase::EvasionIdle(AUnitBase* UnitBase, FVector CollisionLocation)
{
	UnitBase->SetWalkSpeed(UnitBase->Attributes->GetRunSpeed());
				
	const FVector UnitLocation = UnitBase->GetActorLocation();
				
	if(UnitBase->IsFlying)
	{
		CollisionLocation = FVector(CollisionLocation.X, CollisionLocation.Y, UnitBase->FlyHeight);
	}
	
	const FVector ADirection = UKismetMathLibrary::GetDirectionUnitVector(UnitLocation, CollisionLocation);
	UnitBase->AddMovementInput(-1*ADirection, UnitBase->Attributes->GetRunSpeedScale());

	const float Distance = sqrt((UnitLocation.X-CollisionLocation.X)*(UnitLocation.X-CollisionLocation.X)+(UnitLocation.Y-CollisionLocation.Y)*(UnitLocation.Y-CollisionLocation.Y));

	if (Distance >= UnitBase->EvadeDistance) {
		UnitBase->SetUEPathfinding = true;
		UnitBase->SetUnitState(UnitData::Run);
		UnitBase->CollisionUnit = nullptr;
	}
	UnitBase->UnitControlTimer = 0.f;
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
	if(UnitBase->GetToggleUnitDetection())
	{
		if(UnitBase->SetNextUnitToChase())
		{
			UnitBase->SetUEPathfinding = true;
			UnitBase->SetUnitState(UnitData::Chase);
			return;
		}
	}
	if(UnitBase->CollisionUnit && UnitBase->CollisionUnit->TeamId == UnitBase->TeamId && UnitBase->CollisionUnit->GetUnitState() != UnitData::Dead)
	{
		UnitBase->SetUEPathfinding = true;
		UnitBase->SetUnitState(UnitData::Evasion);
		UnitBase->UnitStatePlaceholder = UnitData::Run;
		return;
	}


	DetectAndLoseUnits();

	UnitBase->SetWalkSpeed(UnitBase->Attributes->GetRunSpeed());

	const FVector UnitLocation = UnitBase->GetActorLocation();
	const float Distance = sqrt((UnitLocation.X-UnitBase->RunLocation.X)*(UnitLocation.X-UnitBase->RunLocation.X)+(UnitLocation.Y-UnitBase->RunLocation.Y)*(UnitLocation.Y-UnitBase->RunLocation.Y));

	if (Distance <= UnitBase->StopRunTolerance) {
		UnitBase->SetUnitState(UnitData::Idle);
		return;
	}

	if(UnitBase->GetVelocity().X == 0.0f && UnitBase->GetVelocity().Y == 0.0f) UnitBase->SetUEPathfinding = true;
	
	if(!UnitBase->SetUEPathfinding) return;

	SetUEPathfinding(UnitBase, DeltaSeconds, UnitBase->RunLocation);
}

void AUnitControllerBase::PatrolUEPathfinding(AUnitBase* UnitBase, float DeltaSeconds)
{


	if(UnitBase->SetNextUnitToChase())
	{
		UnitBase->SetUEPathfinding = true;
		UnitBase->SetUnitState(UnitData::Chase);
		return;
	}
	
	
	UnitBase->SetWalkSpeed(UnitBase->Attributes->GetRunSpeed());

	DetectAndLoseUnits();
	
	if(UnitBase->CollisionUnit && UnitBase->CollisionUnit->TeamId !=  UnitBase->TeamId && UnitBase->CollisionUnit->GetUnitState() != UnitData::Dead)
	{
		UnitBase->UnitToChase = UnitBase->CollisionUnit;
		UnitBase->UnitsToChase.Emplace(UnitBase->CollisionUnit);
		UnitBase->CollisionUnit = nullptr;
		UnitBase->SetUEPathfinding = true;
		UnitBase->SetUnitState(UnitData::Chase);
	}
	else if(UnitBase->GetUnitState() == UnitData::Chase )//UnitBase->UnitToChase && UnitBase->UnitToChase->GetUnitState() != UnitData::Dead)
	{

		
	}
	else if (UnitBase->NextWaypoint != nullptr)
	{
		if(UnitBase->IsFlying)
		{
			FVector WaypointLocation = UnitBase->NextWaypoint->GetActorLocation();
			WaypointLocation =  FVector(WaypointLocation.X, WaypointLocation.Y, UnitBase->FlyHeight);
			const FVector ADirection = UKismetMathLibrary::GetDirectionUnitVector(UnitBase->GetActorLocation(), WaypointLocation);
			UnitBase->AddMovementInput(ADirection, UnitBase->Attributes->GetRunSpeedScale());
		}else
		{
			SetUEPathfinding(UnitBase, DeltaSeconds, UnitBase->NextWaypoint->GetActorLocation());
		}
	}
	else
	{
		UnitBase->SetUnitState(UnitData::Idle);
	}
}

FVector AUnitControllerBase::GetCloseLocation(FVector ToLocation, float Distance)
{
	while(true)
	{
		FVector RandomCloseLocation = ToLocation
		+ FMath::RandPointInBox(FBox(FVector(-Distance, -Distance, 0),
		FVector(Distance, Distance, 0)));

		// Now adjust the Z-coordinate of PatrolCloseLocation to ensure it's above terrain
		const FVector Start = FVector(RandomCloseLocation.X, RandomCloseLocation.Y, RandomCloseLocation.Z + 1000.f);  // Start from a point high above the PatrolCloseLocation
		const FVector End = FVector(RandomCloseLocation.X, RandomCloseLocation.Y, RandomCloseLocation.Z - 1000.f);  // End at a point below the PatrolCloseLocation

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
				RandomCloseLocation.Z = HitResult.ImpactPoint.Z;
				return RandomCloseLocation;
			}
		}
	}
}

void AUnitControllerBase::SetPatrolCloseLocation(AUnitBase* UnitBase)
{
	
	for(int i = 0; i < 10; i++)
	{
		UnitBase->RandomPatrolLocation = UnitBase->NextWaypoint->GetActorLocation()
		+ FMath::RandPointInBox(FBox(FVector(-UnitBase->NextWaypoint->PatrolCloseOffset.X, -UnitBase->NextWaypoint->PatrolCloseOffset.Y, 0),
		FVector(UnitBase->NextWaypoint->PatrolCloseOffset.X, UnitBase->NextWaypoint->PatrolCloseOffset.Y, 0)));

		// Now adjust the Z-coordinate of PatrolCloseLocation to ensure it's above terrain
		const FVector Start = FVector(UnitBase->RandomPatrolLocation.X, UnitBase->RandomPatrolLocation.Y, UnitBase->RandomPatrolLocation.Z + UnitBase->LineTraceZDistance);  // Start from a point high above the PatrolCloseLocation
		const FVector End = FVector(UnitBase->RandomPatrolLocation.X, UnitBase->RandomPatrolLocation.Y, UnitBase->RandomPatrolLocation.Z - UnitBase->LineTraceZDistance);  // End at a point below the PatrolCloseLocation

		FHitResult HitResult;
		FCollisionQueryParams CollisionParams;
		CollisionParams.AddIgnoredActor(this);  // Ignore this actor during the trace

		if (GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, CollisionParams))
		{
			AActor* HitActor = HitResult.GetActor();

			// Check if we hit the landscape
			if (HitActor && (HitActor->IsA(ALandscape::StaticClass()) || HitActor->IsA(SurfaceClass) ))
			{
				// Hit landscape
				// Set the Z-coordinate accordingly
				UnitBase->RandomPatrolLocation.Z = HitResult.ImpactPoint.Z;
				return;
			}
		}
		
	}
	
	UnitBase->RandomPatrolLocation = UnitBase->NextWaypoint->GetActorLocation();
	UnitBase->RandomPatrolLocation.X += FMath::RandRange(-300.0f, 300.0f);
	UnitBase->RandomPatrolLocation.Y += FMath::RandRange(-300.0f, 300.0f);
}

void AUnitControllerBase::SetUEPathfindingRandomLocation(AUnitBase* UnitBase, float DeltaSeconds)
{
	if(!UnitBase ||  !UnitBase->NextWaypoint) return;

	FVector ActorLocation = UnitBase->GetActorLocation();
	FVector WaypointLocation = UnitBase->NextWaypoint->GetActorLocation();
	

	
		if (ActorLocation.Equals(UnitBase->RandomPatrolLocation, 200.f) ||
			UnitBase->UnitControlTimer > UnitBase->NextWaypoint->RandomTime) 
		{
			if (FMath::FRand() * 100.0f < UnitBase->NextWaypoint->PatrolCloseIdlePercentage &&
											!ActorLocation.Equals(WaypointLocation, 200.f) &&
											ActorLocation.Equals(UnitBase->RandomPatrolLocation, 200.f)) // && DistanceToWaypoint > 500.f
			{
				UnitBase->UnitControlTimer = 0.f;
				UnitBase->SetUnitState(UnitData::PatrolIdle);
				return;
			}
			
			UnitBase->UnitControlTimer = 0.f;
			UnitBase->SetUEPathfinding = true;
		}

	if(UnitBase->CollisionUnit)
	{
		UnitBase->SetUEPathfinding = true;
		UnitBase->CollisionUnit = nullptr;
	}

	if(UnitBase->GetVelocity().X == 0.0f && UnitBase->GetVelocity().Y == 0.0f) UnitBase->SetUEPathfinding = true;
	
	UnitBase->UnitControlTimer += DeltaSeconds;

	
	if(!UnitBase->SetUEPathfinding)
		return;
	
	bool Succeeded = false;
	// Execute SetPatrolCloseLocation again if Succeeded is false
	int X = 0;
	while (!Succeeded && X < 5)
	{
		SetPatrolCloseLocation(UnitBase);
		Succeeded = SetUEPathfinding(UnitBase, DeltaSeconds, UnitBase->RandomPatrolLocation);
		Succeeded = Succeeded && UnitBase->GetVelocity().X != 0.0f && UnitBase->GetVelocity().Y != 0.0f;
		X++;
	}

	// Draw debug line from UnitBase to RandomPatrolLocation
	if (GEngine && DebugPatrolRandom)
	{
		DrawDebugLine(GetWorld(), UnitBase->GetActorLocation(), UnitBase->RandomPatrolLocation, FColor::Red, false, 5.0, 0, 5.0f);
	}
}

bool AUnitControllerBase::SetUEPathfinding(AUnitBase* UnitBase, float DeltaSeconds, FVector Location, AUnitBase* UnitToIgnore)
{
	
	if(UnitBase->GetVelocity().X == 0.0f && UnitBase->GetVelocity().Y == 0.0f) UnitBase->SetUEPathfinding = true;
	
	if(!UnitBase->SetUEPathfinding)
		return false;
	
	//if (ControllerBase)
	//{
		UnitBase->SetWalkSpeed(UnitBase->Attributes->GetRunSpeed());
		// You can use the controller here

		// For example, you can use the MoveToLocationUEPathFinding function if it's defined in your controller class.
		UnitBase->SetUEPathfinding = false;
		if(UnitToIgnore) return MoveToLocationUEPathFinding(UnitBase, Location, UnitToIgnore);
		return MoveToLocationUEPathFinding(UnitBase, Location);
	//}

	
}

void AUnitControllerBase::SetUEPathfindingTo(AUnitBase* UnitBase, float DeltaSeconds, FVector Location)
{
	if(!UnitBase->SetUEPathfinding)
		return;
	
	if (ControllerBase != nullptr)
	{
			UnitBase->SetWalkSpeed(UnitBase->Attributes->GetRunSpeed());
			// You can use the controller here
			// For example, you can use the MoveToLocationUEPathFinding function if it's defined in your controller class.
			UnitBase->SetUEPathfinding = false;
			ControllerBase->MoveToLocationUEPathFinding(UnitBase, Location);
	
	}
}

bool AUnitControllerBase::MoveToLocationUEPathFinding(AUnitBase* Unit, const FVector& DestinationLocation, AUnitBase* UnitToIgnore)
{
	// Check server authority
	if(!HasAuthority())
	{
		return false;
	}

	// Check if the unit is valid and can move
	if (!Unit || !Unit->GetCharacterMovement())
	{
		return false;
	}
	
	// Check for a valid navigation system
	UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(GetWorld());
	if (!NavSystem)
	{
		return false;
	}

	FVector StartLocation = Unit->GetActorLocation();
	FVector EndLocation = DestinationLocation;
	FHitResult HitResult;
	FCollisionQueryParams CollisionParams;
	
	if(UnitToIgnore) CollisionParams.AddIgnoredActor(UnitToIgnore);
	
	CollisionParams.AddIgnoredActor(Unit); // Ignore the unit that is moving

	PendingUnit = Unit;
	PendingDestination = DestinationLocation;

	Unit->SetRunLocation(EndLocation);
	Unit->UEPathfindingUsed = true;
    
	// Attempt to move the unit to the destination location
	FAIMoveRequest MoveRequest;
	MoveRequest.SetGoalLocation(EndLocation);
	MoveRequest.SetAcceptanceRadius(AcceptanceRadius);

	FNavPathSharedPtr NavPath;
	MoveTo(MoveRequest, &NavPath);
	
	// Check if a valid path was found
	if (!NavPath || !NavPath->IsValid())
	{
		return false;
	}
	// Check if pathfinding indicates that the location is unreachable
	if (NavPath->IsPartial())
	{
		return false; // Destination is unreachable, e.g., separated by obstacles or on an island
	}

	FNavMeshPath* NavMeshPath = NavPath->CastPath<FNavMeshPath>();
	if (NavMeshPath)
	{
		NavMeshPath->OffsetFromCorners(100.f);
	}

	return true; // Pathfinding succeeded and the unit can move
}

void AUnitControllerBase::StopMovementCommand(AUnitBase* Unit)
{
	if (!Unit || !Unit->GetCharacterMovement())
	{
		return;
	}
	// Stop the current movement request
	StopMovement();
	
	// Clear the pending unit and destination
	PendingUnit = nullptr;
	PendingDestination = FVector::ZeroVector;

	// If necessary, reset any flags or states in the unit

	Unit->UEPathfindingUsed = false;
	Unit->SetRunLocation(FVector::ZeroVector);

}

void AUnitControllerBase::CreateProjectile_Implementation(AUnitBase* UnitBase)
{
	if(UnitBase->UseProjectile && !ProjectileSpawned)
	{
		UnitBase->SpawnProjectile(UnitBase->UnitToChase, UnitBase);
		ProjectileSpawned = true;
	}
}

