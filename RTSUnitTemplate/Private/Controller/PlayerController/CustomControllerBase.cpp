// Fill out your copyright notice in the Description page of Project Settings.


#include "Controller/PlayerController/CustomControllerBase.h"

#include "EngineUtils.h"
#include "Landscape.h"
#include "Characters/Camera/ExtendedCameraBase.h"
#include "Characters/Camera/RLAgent.h"

#include "MassEntitySubsystem.h"     // Needed for FMassEntityManager, UMassEntitySubsystem
#include "MassNavigationFragments.h" // Needed for the engine's FMassMoveTargetFragment
#include "MassMovementFragments.h"  // Needed for EMassMovementAction, FMassVelocityFragment
#include "MassExecutor.h"          // Provides Defer() method context typically
#include "MassCommandBuffer.h"      // Needed for FMassDeferredSetCommand, AddFragmentInstance, PushCommand
#include "NavModifierVolume.h"
#include "Actors/FogActor.h"
#include "Actors/SelectionCircleActor.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Mass/Signals/MySignals.h"
#include "Actors/MinimapActor.h" 
#include "Characters/Unit/BuildingBase.h"
#include "NavAreas/NavArea_Null.h"
#include "NavMesh/RecastNavMesh.h"


void ACustomControllerBase::Multi_SetMyTeamUnits_Implementation(const TArray<AActor*>& AllUnits)
{
	if (!IsLocalController()) return;
	
	// TSGameMode->AllUnits is onyl available on Server why we start running on server
	// We Mutlicast to CLient and send him AllUnits as parameter
	TArray<AUnitBase*> NewSelection;
	for (int32 i = 0; i < AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(AllUnits[i]);
		// Every Controller can Check his own TeamId
		if (Unit && Unit->GetUnitState() != UnitData::Dead && Unit->TeamId == SelectableTeamId)
		{
			NewSelection.Add(Unit);
		}
	}

	// And we can create the FogManager
	for (int32 i = 0; i < NewSelection.Num(); i++)
	{
		if (NewSelection[i] && NewSelection[i]->TeamId == SelectableTeamId)
		{
			NewSelection[i]->IsMyTeam = true;
			//NewSelection[i]->OwningPlayerController = this;
			//NewSelection[i]->SetOwningPlayerController();
			//NewSelection[i]->SpawnFogOfWarManager(this);
		}
	}

	AExtendedCameraBase* Camera = Cast<AExtendedCameraBase>(GetPawn());
	if (Camera)
		Camera->SetupResourceWidget(this);
}

/*
void ACustomControllerBase::Multi_SetFogManagerUnit_Implementation(APerformanceUnit* Unit)
{
	if (IsValid(Unit))
		if (Unit->TeamId == SelectableTeamId)
		{
			Unit->IsMyTeam = true;
			Unit->SpawnFogOfWarManager(this);
		}
}*/

void ACustomControllerBase::Multi_ShowWidgetsWhenLocallyControlled_Implementation()
{
	UE_LOG(LogTemp, Log, TEXT("Multi_HideWidgetWhenNoControl_Implementation - TeamId is now: %d"), SelectableTeamId);
	
	AExtendedCameraBase* Camera = Cast<AExtendedCameraBase>(CameraBase);
	if (Camera)
		Camera->ShowWidgetsWhenLocallyControlled();
}

void ACustomControllerBase::Multi_SetCamLocation_Implementation(FVector NewLocation)
{
	//if (!IsLocalController()) return;
	UE_LOG(LogTemp, Log, TEXT("Multi_HideWidgetWhenNoControl_Implementation - TeamId is now: %d"), SelectableTeamId);
	
	AExtendedCameraBase* Camera = Cast<AExtendedCameraBase>(CameraBase);
	if (Camera)
		Camera->SetActorLocation(NewLocation);
}

void ACustomControllerBase::Multi_HideEnemyWaypoints_Implementation()
{
	// Only execute for the local controller
	if (!IsLocalController())
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("Multi_HideEnemyWaypoints_Implementation - TeamId is now: %d"), SelectableTeamId);

	// Retrieve all waypoints from the world
	TArray<AActor*> FoundWaypoints;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWaypoint::StaticClass(), FoundWaypoints);

	// Iterate over each waypoint
	for (AActor* Actor : FoundWaypoints)
	{
		if (AWaypoint* Waypoint = Cast<AWaypoint>(Actor))
		{
			// Hide the waypoint if its TeamId is different from our team AND it isn't neutral (TeamId == 0)
			if (Waypoint->TeamId != SelectableTeamId && Waypoint->TeamId != 0)
			{
				Waypoint->SetActorHiddenInGame(true);
			}
		}
	}
}


void ACustomControllerBase::Multi_InitFogOfWar_Implementation()
{
	for (TActorIterator<AFogActor> It(GetWorld()); It; ++It)
	{
		AFogActor* FogActor = *It;
		if (FogActor)
		{
			// We call this now to ensure all Player Controllers have their Team Ids assigned first.
			FogActor->InitializeFogPostProcess();
		}
	}
}

void ACustomControllerBase::AgentInit_Implementation()
{
	// Only execute for the local controller
	if (!IsLocalController())
	{
		return;
	}

	ARLAgent* Camera = Cast<ARLAgent>(CameraBase);
	if (Camera)
		Camera->AgentInitialization();
}


void ACustomControllerBase::CorrectSetUnitMoveTarget_Implementation(UObject* WorldContextObject, AUnitBase* Unit, const FVector& NewTargetLocation, float DesiredSpeed, float AcceptanceRadius, bool AttackT)
{
	if (!Unit) return;
	
	if (!Unit->IsInitialized) return;
	
	if (!Unit->CanMove) return;
	
	if (Unit->CurrentSnapshot.AbilityClass)
	{

		UGameplayAbilityBase* AbilityCDO = Unit->CurrentSnapshot.AbilityClass->GetDefaultObject<UGameplayAbilityBase>();
		
		if (AbilityCDO && !AbilityCDO->AbilityCanBeCanceled) return;

		CancelCurrentAbility(Unit);
	}
	
    UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("SetUnitMoveTarget: WorldContextObject is invalid or could not provide World."));
        return;
    }

    UMassEntitySubsystem* MassSubsystem = World->GetSubsystem<UMassEntitySubsystem>();
    if (!MassSubsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("SetUnitMoveTarget: MassEntitySubsystem not found. Is Mass enabled?"));
        return;
    }

    FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();

	FMassEntityHandle MassEntityHandle =  Unit->MassActorBindingComponent->GetMassEntityHandle();
	
    if (!EntityManager.IsEntityValid(MassEntityHandle))
    {
        UE_LOG(LogTemp, Warning, TEXT("SetUnitMoveTarget: Provided Entity Handle %s is invalid."), *MassEntityHandle.DebugGetDescription());
        return;
    }

    // --- Access the PER-ENTITY fragment ---
    FMassMoveTargetFragment* MoveTargetFragmentPtr = EntityManager.GetFragmentDataPtr<FMassMoveTargetFragment>(MassEntityHandle);
	FMassAIStateFragment* AiStatePtr = EntityManager.GetFragmentDataPtr<FMassAIStateFragment>(MassEntityHandle);
	
    if (!MoveTargetFragmentPtr || !AiStatePtr)
    {
        UE_LOG(LogTemp, Error, TEXT("SetUnitMoveTarget: Entity %s does not have an FMassMoveTargetFragment."), *MassEntityHandle.DebugGetDescription());
        return;
    }

	AiStatePtr->StoredLocation = NewTargetLocation;
	AiStatePtr->PlaceholderSignal = UnitSignals::Run;

	UpdateMoveTarget(*MoveTargetFragmentPtr, NewTargetLocation, DesiredSpeed, World);
	
	EntityManager.Defer().AddTag<FMassStateRunTag>(MassEntityHandle);
	
	if (AttackT)
	{
		if (AiStatePtr->CanAttack && AiStatePtr->IsInitialized) EntityManager.Defer().AddTag<FMassStateDetectTag>(MassEntityHandle);
	}else
	{
		EntityManager.Defer().RemoveTag<FMassStateDetectTag>(MassEntityHandle);
	}

	EntityManager.Defer().RemoveTag<FMassStateIdleTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStateChaseTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStateAttackTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStatePauseTag>(MassEntityHandle);
	//EntityManager.Defer().RemoveTag<FMassStateDeadTag>(MassEntityHandle); 
	EntityManager.Defer().RemoveTag<FMassStatePatrolRandomTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStatePatrolIdleTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStateCastingTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStateIsAttackedTag>(MassEntityHandle);

	EntityManager.Defer().RemoveTag<FMassStateGoToBaseTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStateGoToBuildTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStateBuildTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStateGoToResourceExtractionTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStateResourceExtractionTag>(MassEntityHandle);
}

void ACustomControllerBase::CorrectSetUnitMoveTargetForAbility_Implementation(UObject* WorldContextObject, AUnitBase* Unit, const FVector& NewTargetLocation, float DesiredSpeed, float AcceptanceRadius, bool AttackT)
{
	if (!Unit) return;
	
	if (!Unit->IsInitialized) return;
	
	if (!Unit->CanMove) return;
	
    UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("SetUnitMoveTarget: WorldContextObject is invalid or could not provide World."));
        return;
    }

    UMassEntitySubsystem* MassSubsystem = World->GetSubsystem<UMassEntitySubsystem>();
    if (!MassSubsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("SetUnitMoveTarget: MassEntitySubsystem not found. Is Mass enabled?"));
        return;
    }

    FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();

	FMassEntityHandle MassEntityHandle =  Unit->MassActorBindingComponent->GetMassEntityHandle();
	
    if (!EntityManager.IsEntityValid(MassEntityHandle))
    {
        UE_LOG(LogTemp, Warning, TEXT("SetUnitMoveTarget: Provided Entity Handle %s is invalid."), *MassEntityHandle.DebugGetDescription());
        return;
    }

    // --- Access the PER-ENTITY fragment ---
    FMassMoveTargetFragment* MoveTargetFragmentPtr = EntityManager.GetFragmentDataPtr<FMassMoveTargetFragment>(MassEntityHandle);
	FMassAIStateFragment* AiStatePtr = EntityManager.GetFragmentDataPtr<FMassAIStateFragment>(MassEntityHandle);
	
    if (!MoveTargetFragmentPtr || !AiStatePtr)
    {
        UE_LOG(LogTemp, Error, TEXT("SetUnitMoveTarget: Entity %s does not have an FMassMoveTargetFragment."), *MassEntityHandle.DebugGetDescription());
        return;
    }

	AiStatePtr->StoredLocation = NewTargetLocation;
	AiStatePtr->PlaceholderSignal = UnitSignals::Run;

	UpdateMoveTarget(*MoveTargetFragmentPtr, NewTargetLocation, DesiredSpeed, World);
	
	EntityManager.Defer().AddTag<FMassStateRunTag>(MassEntityHandle);
	
	if (AttackT)
	{
		if (AiStatePtr->CanAttack && AiStatePtr->IsInitialized) EntityManager.Defer().AddTag<FMassStateDetectTag>(MassEntityHandle);
	}else
	{
		EntityManager.Defer().RemoveTag<FMassStateDetectTag>(MassEntityHandle);
	}

	EntityManager.Defer().RemoveTag<FMassStateIdleTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStateChaseTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStateAttackTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStatePauseTag>(MassEntityHandle);
	//EntityManager.Defer().RemoveTag<FMassStateDeadTag>(MassEntityHandle); 
	EntityManager.Defer().RemoveTag<FMassStatePatrolRandomTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStatePatrolIdleTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStateCastingTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStateIsAttackedTag>(MassEntityHandle);

	EntityManager.Defer().RemoveTag<FMassStateGoToBaseTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStateGoToBuildTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStateBuildTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStateGoToResourceExtractionTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStateResourceExtractionTag>(MassEntityHandle);
}


void ACustomControllerBase::LoadUnitsMass_Implementation(const TArray<AUnitBase*>& UnitsToLoad, AUnitBase* Transporter)
{
		if (Transporter && Transporter->IsATransporter) // Transporter->IsATransporter
		{

			// Set up start and end points for the line trace (downward direction)
			FVector Start = Transporter->GetActorLocation();
			FVector End = Start - FVector(0.f, 0.f, 10000.f); // Trace far enough downwards

			FHitResult HitResult;
			FCollisionQueryParams QueryParams;
			// Ignore the transporter itself
			QueryParams.AddIgnoredActor(Transporter);

			// Perform the line trace on a suitable collision channel, e.g., ECC_Visibility or a custom one
			bool DidHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, QueryParams);
			
			
			for (int32 i = 0; i < UnitsToLoad.Num(); i++)
			{
				if (UnitsToLoad[i] && UnitsToLoad[i]->UnitState != UnitData::Dead && UnitsToLoad[i]->CanBeTransported)
				{
					UnitsToLoad[i]->RemoveFocusEntityTarget();
					// Calculate the distance between the selected unit and the transport unit in X/Y space only.
					float Distance = FVector::Dist2D(UnitsToLoad[i]->GetActorLocation(), Transporter->GetActorLocation());

					// If the unit is within 250 units, load it instantly.
					if (Distance <= Transporter->InstantLoadRange)
					{
						Transporter->LoadUnit(UnitsToLoad[i]);
					}
					else
					{
						// Otherwise, set it as ready for transport so it can move towards the transporter.
						UnitsToLoad[i]->SetRdyForTransport(true);
					}
					// Perform the line trace on a suitable collision channel, e.g., ECC_Visibility or a custom one
					if (DidHit)
					{
						// Use the hit location's Z coordinate and keep X and Y from the transporter
						FVector NewRunLocation = Transporter->GetActorLocation();
						NewRunLocation.Z = HitResult.Location.Z+50.f;
						UnitsToLoad[i]->RunLocation = NewRunLocation;
					}
					else
					{
						// Fallback: if no hit, subtract a default fly height
						UnitsToLoad[i]->RunLocation = Transporter->GetActorLocation();
					}
					

                    bool UnitIsValid = true;
                    
                    if (!SelectedUnits[i]->IsInitialized) UnitIsValid = false;
                    if (!SelectedUnits[i]->CanMove) UnitIsValid = false;
                
                    if (SelectedUnits[i]->CurrentSnapshot.AbilityClass)
                    {

                        UGameplayAbilityBase* AbilityCDO = SelectedUnits[i]->CurrentSnapshot.AbilityClass->GetDefaultObject<UGameplayAbilityBase>();
                    
                        if (AbilityCDO && !AbilityCDO->AbilityCanBeCanceled) UnitIsValid = false;
                        else CancelCurrentAbility(SelectedUnits[i]);
                    }

					if (SelectedUnits[i]->bIsMassUnit && UnitIsValid)
					{
						float Speed = SelectedUnits[i]->Attributes->GetBaseRunSpeed();
						CorrectSetUnitMoveTarget(GetWorld(), SelectedUnits[i], UnitsToLoad[i]->RunLocation, Speed, 40.f);
						SetUnitState_Replication(SelectedUnits[i], 1);
					}
					else
					{
						RightClickRunUEPF(UnitsToLoad[i], UnitsToLoad[i]->RunLocation, true);
					}
				}
			}
			
			SetUnitState_Replication(Transporter,0);

		}else
		{
			for (int32 i = 0; i < UnitsToLoad.Num(); i++)
			{
				if (UnitsToLoad[i] && UnitsToLoad[i]->UnitState != UnitData::Dead && UnitsToLoad[i]->CanBeTransported)
				{
					UnitsToLoad[i]->SetRdyForTransport(false);
				}
			}
		}
	
}

bool ACustomControllerBase::CheckClickOnTransportUnitMass(FHitResult Hit_Pawn)
{
	if (!Hit_Pawn.bBlockingHit) return false;


		AActor* HitActor = Hit_Pawn.GetActor();
		
		AUnitBase* UnitBase = Cast<AUnitBase>(HitActor);

		LoadUnitsMass(SelectedUnits, UnitBase);
	
		if (UnitBase && UnitBase->IsATransporter){
			UnitBase->RemoveFocusEntityTarget();
			TArray<AUnitBase*> NewSelection;

			NewSelection.Emplace(UnitBase);
			
			FTimerHandle TimerHandle;
			GetWorld()->GetTimerManager().SetTimer(
				TimerHandle,
				[this, NewSelection]()
				{
					Client_UpdateHUDSelection(NewSelection, SelectableTeamId);
				},
				1.0f,  // Delay time in seconds (change as needed)
				false  // Do not loop
			);
			return true;
		}
	return false;
}

void ACustomControllerBase::RightClickPressedMass()
{
	
	AttackToggled = false;
	FHitResult Hit;
	GetHitResultUnderCursor(ECollisionChannel::ECC_Pawn, false, Hit);
	
	if (!CheckClickOnTransportUnitMass(Hit))
	{
		if(!SelectedUnits.Num() || !SelectedUnits[0]->CurrentDraggedWorkArea)
		{
			GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, false, Hit);
			if(!CheckClickOnWorkArea(Hit))
			{
					RunUnitsAndSetWaypointsMass(Hit);
			}
		}
	}


	if (SelectedUnits.Num() && SelectedUnits[0] && SelectedUnits[0]->CurrentDraggedWorkArea)
	{
		DestroyDraggedArea(SelectedUnits[0]);
	}
	
}

// Helper to get a unit's world location (actor or ISM)
FVector ACustomControllerBase::GetUnitWorldLocation(const AUnitBase* Unit) const
{
    if (!Unit) return FVector::ZeroVector;
    if (Unit->bUseSkeletalMovement || !Unit->ISMComponent)
    {
        return Unit->GetActorLocation();
    }
    FTransform InstanceTransform;
    if (Unit->ISMComponent->GetInstanceTransform(Unit->InstanceIndex, InstanceTransform, true))
    {
        return InstanceTransform.GetLocation();
    }
    return FVector::ZeroVector;
}

TArray<FVector> ACustomControllerBase::ComputeSlotOffsets(int32 NumUnits) const
{
    int32 GridSize = ComputeGridSize(NumUnits);
    float Half = (GridSize - 1) * GridSpacing / 2.0f;
    FVector Center(Half, Half, 0.f);
    TArray<FVector> Offsets;
    Offsets.Reserve(NumUnits);
    for (int32 i = 0; i < NumUnits; ++i)
    {
        int32 Row = i / GridSize;
        int32 Col = i % GridSize;
        Offsets.Add(FVector(Col * GridSpacing, Row * GridSpacing, 0.f) - Center);
    }
    return Offsets;
}

TArray<TArray<float>> ACustomControllerBase::BuildCostMatrix(
    const TArray<FVector>& UnitPositions,
    const TArray<FVector>& SlotOffsets,
    const FVector& TargetCenter) const
{
    int32 N = UnitPositions.Num();
    TArray<TArray<float>> Cost;
    Cost.SetNum(N);
    for (int32 i = 0; i < N; ++i)
    {
        Cost[i].SetNum(N);
        for (int32 j = 0; j < N; ++j)
        {
            FVector SlotWorld = TargetCenter + SlotOffsets[j];
            Cost[i][j] = FVector::DistSquared(UnitPositions[i], SlotWorld);
        }
    }
    return Cost;
}

TArray<int32> ACustomControllerBase::SolveHungarian(const TArray<TArray<float>>& Matrix) const
{
    int n = Matrix.Num();
    TArray<float> u; u.Init(0.f, n+1);
    TArray<float> v; v.Init(0.f, n+1);
    TArray<int32> p; p.Init(0, n+1);
    TArray<int32> way; way.Init(0, n+1);

    for (int i = 1; i <= n; ++i)
    {
        p[0] = i;
        int j0 = 0;
        TArray<float> minv; minv.Init(FLT_MAX, n+1);
        TArray<bool> used; used.Init(false, n+1);
        do
        {
            used[j0] = true;
            int i0 = p[j0];
            float delta = FLT_MAX;
            int j1 = 0;
            for (int j = 1; j <= n; ++j)
            {
                if (!used[j])
                {
                    float cur = Matrix[i0-1][j-1] - u[i0] - v[j];
                    if (cur < minv[j]) { minv[j] = cur; way[j] = j0; }
                    if (minv[j] < delta) { delta = minv[j]; j1 = j; }
                }
            }
            for (int j = 0; j <= n; ++j)
            {
                if (used[j]) { u[p[j]] += delta; v[j] -= delta; }
                else { minv[j] -= delta; }
            }
            j0 = j1;
        } while (p[j0] != 0);

        do
        {
            int j1 = way[j0];
            p[j0] = p[j1];
            j0 = j1;
        } while (j0);
    }

    TArray<int32> Assignment;
    Assignment.SetNum(n);
    for (int j = 1; j <= n; ++j)
    {
        if (p[j] > 0) Assignment[p[j] - 1] = j - 1;
    }
    return Assignment;
}

bool ACustomControllerBase::ShouldRecalculateFormation() const
{
    if (bForceFormationRecalculation) return true;
    if (SelectedUnits.Num() != LastFormationUnits.Num()) return true;
    TSet<TWeakObjectPtr<AUnitBase>> LastSet(LastFormationUnits);
    for (AUnitBase* U : SelectedUnits)
        if (!LastSet.Contains(U)) return true;
    return false;
}

void ACustomControllerBase::RecalculateFormation(const FVector& TargetCenter)
{
    int32 N = SelectedUnits.Num();
    UnitFormationOffsets.Empty();
    LastFormationUnits.Empty();

    auto Offsets = ComputeSlotOffsets(N);
    TArray<FVector> Positions;
    Positions.Reserve(N);
    for (AUnitBase* U : SelectedUnits)
        Positions.Add(GetUnitWorldLocation(U));

    auto Cost = BuildCostMatrix(Positions, Offsets, TargetCenter);
    auto Assign = SolveHungarian(Cost);

    for (int32 i = 0; i < N; ++i)
    {
        UnitFormationOffsets.Add(SelectedUnits[i], Offsets[Assign[i]]);
        LastFormationUnits.Add(SelectedUnits[i]);
    }
    bForceFormationRecalculation = false;
}

void ACustomControllerBase::RunUnitsAndSetWaypointsMass(FHitResult Hit)
{
    // 1. Setup
    if (SelectedUnits.Num() == 0) return;
    AWaypoint* BWaypoint = nullptr;
    bool PlayWaypoint = false, PlayRun = false;

    // 2. Formation check
    if (ShouldRecalculateFormation())
    {
        //UE_LOG(LogTemp, Warning, TEXT("Recalculating formation..."));
        RecalculateFormation(Hit.Location);
    }

    // 3. Assign final positions
    TMap<AUnitBase*, FVector> Finals;
    for (AUnitBase* U : SelectedUnits)
    {
		bool UnitIsValid = true;
    	
    	if (!U->IsInitialized) UnitIsValid = false;
	
    	if (U->CurrentSnapshot.AbilityClass)
    	{

    		UGameplayAbilityBase* AbilityCDO = U->CurrentSnapshot.AbilityClass->GetDefaultObject<UGameplayAbilityBase>();
		
    		if (AbilityCDO && !AbilityCDO->AbilityCanBeCanceled) UnitIsValid = false;
			else
			{
				ABuildingBase* BuildingBase = Cast<ABuildingBase>(U);
				if (!BuildingBase || !BuildingBase->HasWaypoint)
					CancelCurrentAbility(U);
			}
    	}

    	if (UnitIsValid)
    	{
    		FVector Off = UnitFormationOffsets.FindRef(U);
    		Finals.Add(U, Hit.Location + Off);
    	}
    }

    // 4. Issue moves & sounds
    for (auto& P : Finals)
    {
        auto* U = P.Key;
        FVector Loc = P.Value;
        if (!U || U == CameraUnitWithTag || U->UnitState == UnitData::Dead) continue;

        bool bNavMod;
        Loc = TraceRunLocation(Loc, bNavMod);
        if (bNavMod) continue;

    	U->RemoveFocusEntityTarget();
        float Speed = U->Attributes->GetBaseRunSpeed();
        if (SetBuildingWaypoint(Loc, U, BWaypoint, PlayWaypoint))
        {
            PlayWaypoint = true;
        }
        else if (IsShiftPressed)
        {
            DrawDebugCircleAtLocation(GetWorld(), Loc, FColor::Green);
            if (!U->IsInitialized || !U->CanMove) continue;
            if (U->bIsMassUnit)
            {
            	if (U->GetUnitState() != UnitData::Run)
            	{
            		CorrectSetUnitMoveTarget(GetWorld(), U, Loc, Speed, 40.f);
            	}
	           RightClickRunShift(U, Loc);
            	SetUnitState_Replication(U, 1);
            }
            PlayRun = true;
        }
        else
        {
            DrawDebugCircleAtLocation(GetWorld(), Loc, FColor::Green);
            if (!U->IsInitialized || !U->CanMove) continue;
            if (U->bIsMassUnit)
                CorrectSetUnitMoveTarget(GetWorld(), U, Loc, Speed, 40.f), SetUnitState_Replication(U, 1);
            else if (UseUnrealEnginePathFinding)
                RightClickRunUEPF(U, Loc, true);
            else
                RightClickRunDijkstraPF(U, Loc, -1);
            PlayRun = true;
        }
    }

    if (WaypointSound && PlayWaypoint) UGameplayStatics::PlaySound2D(this, WaypointSound);
    if (RunSound && PlayRun &&
        GetWorld()->GetTimeSeconds() - LastRunSoundTime >= RunSoundDelayTime)
    {
        UGameplayStatics::PlaySound2D(this, RunSound);
        LastRunSoundTime = GetWorld()->GetTimeSeconds();
    }
}


void ACustomControllerBase::LeftClickPressedMass()
{
    LeftClickIsPressed = true;
    AbilityArrayIndex = 0;

    if (!CameraBase || CameraBase->TabToggled) return;

    // --- ALT: cancel / destroy area ---
	if (AltIsPressed)
    {
        DestroyWorkArea();
        for (AUnitBase* U : SelectedUnits)
        {
            CancelAbilitiesIfNoBuilding(U);
        }
    }
    else if (AttackToggled)
    {
        // 1) get world hit under cursor
        FHitResult Hit;
        GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, false, Hit);

        int32 NumUnits = SelectedUnits.Num();
        if (NumUnits == 0) return;

        // 2) precompute grid offsets
        TArray<FVector> Offsets = ComputeSlotOffsets(NumUnits);

        AWaypoint* BWaypoint = nullptr;
        bool PlayWaypointSound = false;
        bool PlayAttackSound   = false;

        // 3) issue each unit
        for (int32 i = 0; i < NumUnits; ++i)
        {
            AUnitBase* U = SelectedUnits[i];
            if (U == nullptr || U == CameraUnitWithTag) continue;

            // apply the same slot-by-index approach
            FVector RunLocation = Hit.Location + Offsets[i];

            bool bNavMod;
            RunLocation = TraceRunLocation(RunLocation, bNavMod);

            if (SetBuildingWaypoint(RunLocation, U, BWaypoint, PlayWaypointSound, !bNavMod))
            {
                // waypoint placed
            }
            else
            {

                if (U->bIsMassUnit)
                {
                    LeftClickAttackMass(U, RunLocation, AttackToggled, !bNavMod);
                }
                else if (!bNavMod)
                {
                	DrawDebugCircleAtLocation(GetWorld(), RunLocation, FColor::Red);
                    LeftClickAttack(U, RunLocation);
                }

                if (!bNavMod) PlayAttackSound = true;
            }

            // still fire any dragged ability on each unit
            FireAbilityMouseHit(U, Hit);
        }

        AttackToggled = false;

        // 4) play sounds
        if (WaypointSound && PlayWaypointSound)
        {
            UGameplayStatics::PlaySound2D(this, WaypointSound);
        }
        if (AttackSound && PlayAttackSound)
        {
            UGameplayStatics::PlaySound2D(this, AttackSound);
        }
    }
    // --- NORMAL CLICK / SELECTION / ABILITIES ---
    else
    {
        DropWorkArea();

        // handle any ability under the cursor
        FHitResult HitPawn;
        GetHitResultUnderCursor(ECollisionChannel::ECC_Pawn, false, HitPawn);

        bool AbilityFired   = false;
        bool AbilityUnSynced = false;
        for (AUnitBase* U : SelectedUnits)
        {
            if (U && U->CurrentSnapshot.AbilityClass && U->CurrentDraggedAbilityIndicator)
            {
                FireAbilityMouseHit(U, HitPawn);
                AbilityFired = true;
            }
            else
            {
                AbilityUnSynced = true;
            }
        }
        if (AbilityFired && !AbilityUnSynced)
        {
            return;
        }

        // if we hit a pawn, try to select it
        if (HitPawn.bBlockingHit && HUDBase)
        {
            AActor* HitActor = HitPawn.GetActor();
            if (!HitActor->IsA(ALandscape::StaticClass()))
                ClickedActor = HitActor;
            else
                ClickedActor = nullptr;

            AUnitBase* HitUnit = Cast<AUnitBase>(HitActor);
            ASpeakingUnit* SUnit = Cast<ASpeakingUnit>(HitActor);

            if (HitUnit && (HitUnit->TeamId == SelectableTeamId || SelectableTeamId == 0) && !SUnit)
            {
            	if (IsCtrlPressed)
            	{
            		FGameplayTag Tag = HitUnit->UnitTags.First();
            		SelectUnitsWithTag(Tag, SelectableTeamId);
            	}else
            	{
            		HUDBase->DeselectAllUnits();
            		HUDBase->SetUnitSelected(HitUnit);
            		DragUnitBase(HitUnit);

            		if (CameraBase->AutoLockOnSelect)
            			LockCameraToUnit = true;	
            	}
            }
            else
            {
                HUDBase->InitialPoint = HUDBase->GetMousePos2D();
                HUDBase->bSelectFriendly = true;
            }
        }
    }
}

void ACustomControllerBase::Server_ReportUnitVisibility_Implementation(APerformanceUnit* Unit, bool bVisible)
{
	if (IsValid(Unit))
	{
		Unit->SetClientVisibility(bVisible);
	}
}

void ACustomControllerBase::LeftClickAttackMass_Implementation(AUnitBase* Unit, FVector Location, bool AttackT, bool TraceSuccess)
{
	if (Unit && Unit->UnitState != UnitData::Dead) {
	
		FHitResult Hit_Pawn;
		GetHitResultUnderCursor(ECollisionChannel::ECC_Pawn, false, Hit_Pawn);

		if (Hit_Pawn.bBlockingHit)
		{
			AUnitBase* TargetUnitBase = Cast<AUnitBase>(Hit_Pawn.GetActor());
					
			if(TargetUnitBase)
			{
				/// Focus Enemy Units ///
				Unit->UnitToChase = TargetUnitBase;
				Unit->FocusEntityTarget(TargetUnitBase);
				SetUnitState_Replication(Unit, 3);
				Unit->SwitchEntityTagByState(UnitData::Chase, Unit->UnitStatePlaceholder);
			}else if(UseUnrealEnginePathFinding && TraceSuccess)
			{
					
				if (Unit && Unit->UnitState != UnitData::Dead)
				{
					DrawDebugCircleAtLocation(GetWorld(), Location, FColor::Red);
					LeftClickAMoveUEPFMass(Unit, Location, AttackT);
				}
				Unit->RemoveFocusEntityTarget();
			}else if (TraceSuccess)
			{
				DrawDebugCircleAtLocation(GetWorld(), Location, FColor::Red);
				LeftClickAMove(Unit, Location);
				Unit->RemoveFocusEntityTarget();
			}
		}else if(UseUnrealEnginePathFinding && TraceSuccess)
		{
			if (Unit && Unit->UnitState != UnitData::Dead)
			{
				/// A-Move Units ///
				DrawDebugCircleAtLocation(GetWorld(), Location, FColor::Red);
				LeftClickAMoveUEPFMass(Unit, Location, AttackT);
				Unit->RemoveFocusEntityTarget();
			}
					
		}
			
	}
}

void ACustomControllerBase::LeftClickAMoveUEPFMass_Implementation(AUnitBase* Unit, FVector Location, bool AttackT)
{
	if (!Unit) return;

    if (!Unit->IsInitialized) return;
    if (!Unit->CanMove) return;
	
    if (Unit->CurrentSnapshot.AbilityClass)
    {

    	UGameplayAbilityBase* AbilityCDO = Unit->CurrentSnapshot.AbilityClass->GetDefaultObject<UGameplayAbilityBase>();
		
    	if (AbilityCDO && !AbilityCDO->AbilityCanBeCanceled) return;
		else CancelCurrentAbility(Unit);
    }

	float Speed = Unit->Attributes->GetBaseRunSpeed();

	SetUnitState_Replication(Unit,1);
	CorrectSetUnitMoveTarget(GetWorld(), Unit, Location, Speed, 40.f, AttackT);
}

void ACustomControllerBase::LeftClickReleasedMass()
{
	
	LeftClickIsPressed = false;
	HUDBase->bSelectFriendly = false;
	SelectedUnits = HUDBase->SelectedUnits;

	DropUnitBase();
	int BestIndex = GetHighestPriorityWidgetIndex();
	CurrentUnitWidgetIndex = BestIndex;
	//UpdateSelectionCircles();
	AExtendedCameraBase* ExtendedCameraBase = Cast<AExtendedCameraBase>(CameraBase);
	if (ExtendedCameraBase)
	{
		if(ExtendedCameraBase->TabToggled)
		{
			SetWidgets(BestIndex);
		}
	}
	
}

void ACustomControllerBase::UpdateFogMaskWithCircles(const TArray<FMassEntityHandle>& Entities)
{
    UWorld* World = GetWorld();
    if (!ensure(World)) return;

    // Prepare three parallel arrays
    TArray<FVector_NetQuantize> Positions;
    TArray<float>               WorldRadii;
    TArray<uint8>               UnitTeamIds;

    if (UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>())
    {
        FMassEntityManager& EM = EntitySubsystem->GetMutableEntityManager();

        for (const FMassEntityHandle& E : Entities)
        {
            if (!EM.IsEntityValid(E))
                continue;

            const FTransformFragment*               TF       = EM.GetFragmentDataPtr<FTransformFragment>(E);
            const FMassCombatStatsFragment*         StateFrag= EM.GetFragmentDataPtr<FMassCombatStatsFragment>(E);
            const FMassAIStateFragment*             AI       = EM.GetFragmentDataPtr<FMassAIStateFragment>(E);
            const FMassAgentCharacteristicsFragment* CharFrag = EM.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(E);

            if (!TF || !StateFrag || !AI || !CharFrag)
                continue;

            // skip dead/despawned
            if (StateFrag->Health <= 0.f && AI->StateTimer >= CharFrag->DespawnTime)
                continue;

                // 1) world‐loc
                Positions.Add(FVector_NetQuantize(TF->GetTransform().GetLocation()));
                // 2) world‐space sight radius
                WorldRadii.Add(StateFrag->SightRadius);
                // 3) unit’s team
                UnitTeamIds.Add(StateFrag->TeamId);
        }
    }

    // **One** multicast to all FogActors
    for (TActorIterator<AFogActor> It(World); It; ++It)
    {
        It->Multicast_UpdateFogMaskWithCircles(Positions, WorldRadii, UnitTeamIds);
    }
}

// In CustomControllerBase.cpp

void ACustomControllerBase::UpdateMinimap(const TArray<FMassEntityHandle>& Entities)
{
    UWorld* World = GetWorld();
    if (!ensure(World)) return;

    // Bereite die Arrays vor. NEU: Ein Array für die Aktor-Referenzen.
    TArray<AUnitBase*>             ActorRefs;
    TArray<FVector_NetQuantize> Positions;
    TArray<float>               UnitRadii;
    TArray<float>               FogRadii;
    TArray<uint8>               UnitTeamIds;

    if (UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>())
    {
       FMassEntityManager& EM = EntitySubsystem->GetMutableEntityManager();

       for (const FMassEntityHandle& E : Entities)
       {
          if (!EM.IsEntityValid(E)) continue;

          // Hole alle notwendigen Fragmente
          FMassActorFragment* ActorFrag = EM.GetFragmentDataPtr<FMassActorFragment>(E);
          const FTransformFragment* TF = EM.GetFragmentDataPtr<FTransformFragment>(E);
          const FMassCombatStatsFragment* StateFrag = EM.GetFragmentDataPtr<FMassCombatStatsFragment>(E);
          const FMassAIStateFragment* AI = EM.GetFragmentDataPtr<FMassAIStateFragment>(E);
          const FMassAgentCharacteristicsFragment* CharFrag = EM.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(E);

          // Wir brauchen den Aktor, also ist das ActorFragment jetzt zwingend erforderlich
          if (!ActorFrag || !TF || !StateFrag || !AI || !CharFrag) continue;
          if (StateFrag->Health <= 0.f && AI->StateTimer >= CharFrag->DespawnTime) continue;

       	AActor* UnitActor = ActorFrag->GetMutable();
       	if (!UnitActor) continue;
       	// --- NEUE LOGIK ZUR ERMITTLUNG DES DYNAMISCHEN RADIUS ---
       	float CapsuleRadius = 150.f; // Wir verwenden 150.f als Fallback-Wert.
          
       	// Wir casten zum ACharacter, da dieser standardmäßig eine Kapsel hat.
       	// Wenn Ihre AUnitBase von ACharacter erbt, ist dies der richtige Weg.
       	AUnitBase* Unit = Cast<AUnitBase>(UnitActor);
       	if (Unit && Unit->GetCapsuleComponent())
       	{
       		// GetScaledCapsuleRadius() berücksichtigt die Skalierung des Aktors.
       		CapsuleRadius = Unit->GetCapsuleComponent()->GetScaledCapsuleRadius()*3.f;
       	}
       	
       	if (!Unit) continue;
       	// --- ENDE DER NEUEN LOGIK ---
          // --- Fülle die Arrays ---
          ActorRefs.Add(Unit); // NEU: Füge den Aktor-Pointer hinzu
          Positions.Add(FVector_NetQuantize(TF->GetTransform().GetLocation()));
          UnitRadii.Add(CapsuleRadius); 
          FogRadii.Add(StateFrag->SightRadius);
          UnitTeamIds.Add(StateFrag->TeamId);
       }
    }

    // Rufe den Multicast auf allen MinimapActors auf und sende ALLE Arrays.
    for (TActorIterator<AMinimapActor> It(World); It; ++It)
    {
       It->Multicast_UpdateMinimap(ActorRefs, Positions, UnitRadii, FogRadii, UnitTeamIds);
    }
}


void ACustomControllerBase::UpdateSelectionCircles()
{
	UWorld* World = GetWorld();
	if (!ensure(World)) return;

	TArray<FVector_NetQuantize> Positions;
	TArray<float> WorldRadii;
    
	if (UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>())
	{
		FMassEntityManager& EM = EntitySubsystem->GetMutableEntityManager();

		for (AUnitBase* SelectedUnit : SelectedUnits)
		{
			if (!SelectedUnit) continue;

			FMassEntityHandle Entity = SelectedUnit->MassActorBindingComponent->GetEntityHandle();
			if (!EM.IsEntityValid(Entity)) continue;
            
			const FTransformFragment* TransformFragment = EM.GetFragmentDataPtr<FTransformFragment>(Entity);
			if (!TransformFragment) continue;

			// Use a default or calculated radius for the selection circle
			float SelectionRadius = 50.f; // Example radius, adjust as needed

			Positions.Add(FVector_NetQuantize(TransformFragment->GetTransform().GetLocation()));
			WorldRadii.Add(SelectionRadius);
		}
	}
    
	// Multicast to all SelectionCircleActors
	for (TActorIterator<ASelectionCircleActor> It(World); It; ++It)
	{
		if (It->TeamId == SelectableTeamId)
			It->Multicast_UpdateSelectionCircles(Positions, WorldRadii);
	}
}

void ACustomControllerBase::Multi_SetupPlayerMiniMap_Implementation()
{
	
	if (!CameraBase)
	{
		UE_LOG(LogTemp, Warning, TEXT("SetupPlayerUI: Could not get AExtendedCameraBase pawn."));
		return;
	}
	// Get the pawn this controller is possessing.
	AExtendedCameraBase* ExtendedCameraBase = Cast<AExtendedCameraBase>(CameraBase);
	if (!ExtendedCameraBase)
	{
		UE_LOG(LogTemp, Warning, TEXT("SetupPlayerUI: Could not get AExtendedCameraBase pawn."));
		return;
	}
	
	// Get the minimap widget instance from the pawn.
	UMinimapWidget* Minimap = Cast<UMinimapWidget>(ExtendedCameraBase->MinimapWidget->GetUserWidgetObject());
	if (Minimap)
	{
		Minimap->InitializeForTeam(SelectableTeamId);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("SetupPlayerUI: MinimapWidgetInstance was not valid on the camera pawn. Has it been created?"));
	}
}
