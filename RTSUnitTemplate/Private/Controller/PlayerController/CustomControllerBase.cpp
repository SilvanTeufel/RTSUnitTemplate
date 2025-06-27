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
		if (AiStatePtr->CanDetect && AiStatePtr->IsInitialized) EntityManager.Defer().AddTag<FMassStateDetectTag>(MassEntityHandle);
	}else
	{
		EntityManager.Defer().RemoveTag<FMassStateDetectTag>(MassEntityHandle);
	}

	EntityManager.Defer().RemoveTag<FMassStateIdleTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStateChaseTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStateAttackTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStatePauseTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStateDeadTag>(MassEntityHandle); 
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
					
					if (SelectedUnits[i]->bIsMassUnit)
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
        UE_LOG(LogTemp, Warning, TEXT("Recalculating formation..."));
        RecalculateFormation(Hit.Location);
    }

    // 3. Assign final positions
    TMap<AUnitBase*, FVector> Finals;
    for (AUnitBase* U : SelectedUnits)
    {
        FVector Off = UnitFormationOffsets.FindRef(U);
        Finals.Add(U, Hit.Location + Off);
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
                CorrectSetUnitMoveTarget(GetWorld(), U, Loc, Speed, 40.f), SetUnitState_Replication(U, 1);
            else
                RightClickRunShift(U, Loc);
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


/*
void ACustomControllerBase::RunUnitsAndSetWaypointsMass(FHitResult Hit)
{
    // --- 1. INITIAL SETUP & VALIDATION ---
    int32 NumUnits = SelectedUnits.Num();
    if (NumUnits == 0) return;

    AWaypoint* BWaypoint = nullptr;
    bool PlayWaypointSound = false;
    bool PlayRunSound = false;

    // Helper lambda to get a unit's world location, whether it's a regular actor or an ISM instance.
    auto GetUnitWorldLocation = [](const AUnitBase* Unit) -> FVector
    {
        if (!Unit) return FVector::ZeroVector;
        if (Unit->bUseSkeletalMovement || !Unit->ISMComponent)
        {
            return Unit->GetActorLocation();
        }
        else
        {
            FTransform InstanceTransform;
            if (Unit->ISMComponent->GetInstanceTransform(Unit->InstanceIndex, InstanceTransform, true))
            {
                return InstanceTransform.GetLocation();
            }
        }
        return FVector::ZeroVector; // Fallback
    };
    
    // --- 2. CHECK IF FORMATION NEEDS RECALCULATION ---
    bool bShouldRecalculate = bForceFormationRecalculation;
    if (!bShouldRecalculate)
    {
        // Check if the number of units has changed.
        if (SelectedUnits.Num() != LastFormationUnits.Num())
        {
            bShouldRecalculate = true;
        }
        else
        {
            // If the count is the same, check if the actual units are different.
            TSet<TWeakObjectPtr<AUnitBase>> CurrentUnitSet;
            for(AUnitBase* Unit : SelectedUnits) CurrentUnitSet.Add(Unit);

        	TSet<TWeakObjectPtr<AUnitBase>> LastUnitSet(LastFormationUnits);
        	for (AUnitBase* CurrentUnit : SelectedUnits)
        	{
        		if (!LastUnitSet.Contains(CurrentUnit))
        		{
        			bShouldRecalculate = true;
        			break; // Found a difference, no need to check further.
        		}
        	}
        }
    }
    
    // --- 3. RECALCULATE FORMATION (IF REQUIRED) ---
    if (bShouldRecalculate)
    {
        UE_LOG(LogTemp, Warning, TEXT("Recalculating Formation..."));

        UnitFormationOffsets.Empty();
        LastFormationUnits.Empty();
        
        const int32 GridSize = ComputeGridSize(NumUnits);
        const float GridHalfSize = (GridSize - 1) * GridSpacing / 2.0f;
        const FVector GridCenterOffset = FVector(GridHalfSize, GridHalfSize, 0.f);

        // Calculate the centroid of the current unit positions to use as a reference.
        FVector CurrentCentroid = FVector::ZeroVector;
        int32 ValidUnitCount = 0;
        for (AUnitBase* Unit : SelectedUnits)
        {
            if (Unit)
            {
                CurrentCentroid += GetUnitWorldLocation(Unit);
                ValidUnitCount++;
            }
        }
        if(ValidUnitCount > 0) CurrentCentroid /= ValidUnitCount;

        // Create a list of ideal grid slot positions, centered around (0,0,0)
        TArray<FVector> TargetGridOffsets;
        TargetGridOffsets.Reserve(NumUnits);
        for (int32 i = 0; i < NumUnits; ++i)
        {
            const int32 Row = i / GridSize;
            const int32 Col = i % GridSize;
            FVector SlotOffset = FVector(Col * GridSpacing, Row * GridSpacing, 0.f) - GridCenterOffset;
            TargetGridOffsets.Add(SlotOffset);
        }
        
        // To assign units to the formation slots intelligently, we sort both lists.
        // This attempts to keep units that are already on the left, on the left of the new formation.
        TArray<AUnitBase*> SortedUnits = SelectedUnits;
        SortedUnits.Sort([&](const AUnitBase& A, const AUnitBase& B)
        {
            FVector PosA = GetUnitWorldLocation(&A) - CurrentCentroid;
            FVector PosB = GetUnitWorldLocation(&B) - CurrentCentroid;
            if (FMath::Abs(PosA.Y - PosB.Y) > KINDA_SMALL_NUMBER) return PosA.Y < PosB.Y;
            return PosA.X < PosB.X;
        });

        // Assign each sorted unit to an ideal grid offset and save it for future moves.
        for (int32 i = 0; i < SortedUnits.Num(); ++i)
        {
            if (SortedUnits[i] && TargetGridOffsets.IsValidIndex(i))
            {
                UnitFormationOffsets.Add(SortedUnits[i], TargetGridOffsets[i]);
            }
        }

        // Update the state for the next command.
        for(AUnitBase* Unit : SelectedUnits) LastFormationUnits.Add(Unit);
        bForceFormationRecalculation = false; // Reset the flag
    }

    // --- 4. GENERATE FINAL ASSIGNMENTS USING THE STORED FORMATION ---
    TMap<AUnitBase*, FVector> FinalAssignments;
    FVector TargetCentroid = Hit.Location; // The new center of the formation is the click location.

    for (AUnitBase* Unit : SelectedUnits)
    {
        if (Unit)
        {
            // Find the unit's stored offset. If it doesn't exist (edge case), use a zero offset.
            const FVector* Offset = UnitFormationOffsets.Find(Unit);
            const FVector FinalLocation = TargetCentroid + (Offset ? *Offset : FVector::ZeroVector);
            FinalAssignments.Add(Unit, FinalLocation);
        }
    }
    
    // --- 5. ISSUE MOVE COMMANDS (UNCHANGED LOGIC) ---
    for (const TPair<AUnitBase*, FVector>& Assignment : FinalAssignments)
    {
        AUnitBase* Unit = Assignment.Key;
        FVector RunLocation = Assignment.Value;

        if (Unit && Unit != CameraUnitWithTag && Unit->UnitState != UnitData::Dead)
        {
            bool HitNavModifier;
            RunLocation = TraceRunLocation(RunLocation, HitNavModifier);
            if (HitNavModifier) continue;
            
            float Speed = Unit->Attributes->GetBaseRunSpeed();
       
            if(SetBuildingWaypoint(RunLocation, Unit, BWaypoint, PlayWaypointSound))
            {
                PlayWaypointSound = true;
            }
            else if (IsShiftPressed)
            {
                DrawDebugCircleAtLocation(GetWorld(), RunLocation, FColor::Green);
                if (!Unit->IsInitialized || !Unit->CanMove) continue;
       
                if (Unit->bIsMassUnit)
                {
                    CorrectSetUnitMoveTarget(GetWorld(), Unit, RunLocation, Speed, 40.f);
                    SetUnitState_Replication(Unit, 1);
                }
                else
                {
                    RightClickRunShift(Unit, RunLocation);
                }
                PlayRunSound = true;
            }
            else
            {
                DrawDebugCircleAtLocation(GetWorld(), RunLocation, FColor::Green);
                if (!Unit->IsInitialized || !Unit->CanMove) continue;

                if (Unit->bIsMassUnit)
                {
                    CorrectSetUnitMoveTarget(GetWorld(), Unit, RunLocation, Speed, 40.f);
                    SetUnitState_Replication(Unit, 1);
                }
                else if (UseUnrealEnginePathFinding)
                {
                    RightClickRunUEPF(Unit, RunLocation, true);
                }
                else
                {
                    RightClickRunDijkstraPF(Unit, RunLocation, -1);
                }
                PlayRunSound = true;
            }
        }
    }
    
    // --- 6. SOUND LOGIC (UNCHANGED) ---
    if (WaypointSound && PlayWaypointSound)
    {
       UGameplayStatics::PlaySound2D(this, WaypointSound);
    }

    if (RunSound && PlayRunSound)
    {
       const float CurrentTime = GetWorld()->GetTimeSeconds();
       if (CurrentTime - LastRunSoundTime >= RunSoundDelayTime)
       {
          UGameplayStatics::PlaySound2D(this, RunSound);
          LastRunSoundTime = CurrentTime;
       }
    }
}
*/
/*
void ACustomControllerBase::RunUnitsAndSetWaypointsMass(FHitResult Hit)
{
    int32 NumUnits = SelectedUnits.Num();
    if (NumUnits == 0) return;

    const int32 GridSize = ComputeGridSize(NumUnits);
    AWaypoint* BWaypoint = nullptr;
    bool PlayWaypointSound = false;
    bool PlayRunSound = false;

    // --- NEUE, ROBUSTE LÖSUNG ZUR ZIELZUWWEISUNG ---

    // 1. Hilfs-Lambda, um die korrekte Weltposition einer Einheit zu erhalten (egal ob Skeletal oder ISM)
    auto GetUnitWorldLocation = [](const AUnitBase* Unit) -> FVector
    {
        if (Unit)
        {
            if (Unit->bUseSkeletalMovement || !Unit->ISMComponent)
            {
                return Unit->GetActorLocation();
            }
            else
            {
                FTransform InstanceTransform;
                if (Unit->ISMComponent->GetInstanceTransform(Unit->InstanceIndex, InstanceTransform, true))
                {
                    return InstanceTransform.GetLocation();
                }
            }
        }
        return FVector::ZeroVector; // Fallback
    };
    
    // 2. Berechne zuerst ALLE Zielpunkte im Gitter
    const float GridHalfSize = (GridSize - 1) * GridSpacing / 2.0f;
    const FVector GridCenterOffset = FVector(GridHalfSize, GridHalfSize, 0.f);
    TArray<FVector> TargetGridLocations;
    TargetGridLocations.Reserve(NumUnits);
    for (int32 i = 0; i < NumUnits; ++i)
    {
        const int32 Row = i / GridSize;
        const int32 Col = i % GridSize;
        FVector TargetLoc = (Hit.Location - GridCenterOffset) + CalculateGridOffset(Row, Col);
        TargetGridLocations.Add(TargetLoc);
    }

    // 3. Führe eine intelligente Zuweisung durch: Jede Einheit bekommt den nächstgelegenen freien Gitterplatz
    TArray<AUnitBase*> UnassignedUnits = SelectedUnits;
    TMap<AUnitBase*, FVector> FinalAssignments;

    for (const FVector& TargetPoint : TargetGridLocations)
    {
        if (UnassignedUnits.IsEmpty()) break;

        float ClosestDistSq = FLT_MAX;
        int32 ClosestUnitIndex = -1;

        // Finde die beste (nächste) Einheit für diesen Gitterpunkt
        for (int32 i = 0; i < UnassignedUnits.Num(); ++i)
        {
            if (UnassignedUnits[i])
            {
                const float DistSq = FVector::DistSquared(GetUnitWorldLocation(UnassignedUnits[i]), TargetPoint);
                if (DistSq < ClosestDistSq)
                {
                    ClosestDistSq = DistSq;
                    ClosestUnitIndex = i;
                }
            }
        }

        // Weise die gefundene Einheit dem Gitterpunkt zu und entferne sie aus dem Pool
        if (ClosestUnitIndex != -1)
        {
            AUnitBase* AssignedUnit = UnassignedUnits[ClosestUnitIndex];
            FinalAssignments.Add(AssignedUnit, TargetPoint);
            UnassignedUnits.RemoveAtSwap(ClosestUnitIndex);
        }
    }

    // 4. Gehe die finalen Zuweisungen durch und erteile die Bewegungsbefehle
    for (const TPair<AUnitBase*, FVector>& Assignment : FinalAssignments)
    {
        AUnitBase* Unit = Assignment.Key;
        FVector RunLocation = Assignment.Value;

        if (Unit != CameraUnitWithTag && Unit && Unit->UnitState != UnitData::Dead)
        {
            bool HitNavModifier;
            RunLocation = TraceRunLocation(RunLocation, HitNavModifier);
            if (HitNavModifier) continue;
            
            float Speed = Unit->Attributes->GetBaseRunSpeed();
       
            if(SetBuildingWaypoint(RunLocation, Unit, BWaypoint, PlayWaypointSound))
            {
                // ...
            }
            else if (IsShiftPressed)
            {
                DrawDebugCircleAtLocation(GetWorld(), RunLocation, FColor::Green);
                if (!Unit->IsInitialized || !Unit->CanMove) continue;
       
                if (Unit->bIsMassUnit)
                {
                    CorrectSetUnitMoveTarget(GetWorld(), Unit, RunLocation, Speed, 40.f);
                    SetUnitState_Replication(Unit, 1);
                }
                else
                {
                    RightClickRunShift(Unit, RunLocation);
                }
                PlayRunSound = true;
            }
            else // Default-Verhalten (UseUnrealEnginePathFinding oder Dijkstra)
            {
                DrawDebugCircleAtLocation(GetWorld(), RunLocation, FColor::Green);
                if (!Unit->IsInitialized || !Unit->CanMove) continue;

                if (Unit->bIsMassUnit)
                {
                    CorrectSetUnitMoveTarget(GetWorld(), Unit, RunLocation, Speed, 40.f);
                    SetUnitState_Replication(Unit, 1);
                }
                else if (UseUnrealEnginePathFinding)
                {
                    RightClickRunUEPF(Unit, RunLocation, true);
                }
                else
                {
                    // Dein Index 'i' ist hier nicht mehr verfügbar, da wir über eine Map iterieren.
                    // Wenn DijkstraPF einen Index benötigt, musst du ihn anders übergeben oder die Logik anpassen.
                    // Fürs Erste lasse ich den Index weg, da er wahrscheinlich nicht kritisch ist.
                    RightClickRunDijkstraPF(Unit, RunLocation, -1); // -1 als Indikator für einen unbekannten Index
                }
                PlayRunSound = true;
            }
        }
    }

    // Sound-Logik
    if (WaypointSound && PlayWaypointSound)
    {
       UGameplayStatics::PlaySound2D(this, WaypointSound);
    }

    if (RunSound && PlayRunSound)
    {
       const float CurrentTime = GetWorld()->GetTimeSeconds();
       if (CurrentTime - LastRunSoundTime >= RunSoundDelayTime)
       {
          UGameplayStatics::PlaySound2D(this, RunSound);
          LastRunSoundTime = CurrentTime;
       }
    }
}
*/

/*
void ACustomControllerBase::RunUnitsAndSetWaypointsMass(FHitResult Hit)
{
	
	int32 NumUnits = SelectedUnits.Num();
	if (NumUnits == 0) return; 
	//int32 GridSize = FMath::CeilToInt(FMath::Sqrt((float)NumUnits));
	const int32 GridSize = ComputeGridSize(NumUnits);
	AWaypoint* BWaypoint = nullptr;

	bool PlayWaypointSound = false;
	bool PlayRunSound = false;

	
	// 2. Sortiere die Einheiten basierend auf ihrer Entfernung zum Ziel.
	// Einheiten, die bereits näher am Ziel sind, kommen in der Liste nach vorne.
	// Dies stellt sicher, dass die vorderen Einheiten die vorderen Plätze im Gitter bekommen.
	SelectedUnits.Sort([&Hit](const AUnitBase& A, const AUnitBase& B)
	{
		// Wir vergleichen die quadrierten Distanzen, da dies schneller ist als die Wurzel zu ziehen.
		return FVector::DistSquared(A.GetActorLocation(), Hit.Location) < FVector::DistSquared(B.GetActorLocation(), Hit.Location);
	});
	// --- ENDE DES NEUEN CODES ---

	
	// --- ADD THIS CODE ---
	// Calculate the total offset required to center the grid.
	const float GridHalfSize = (GridSize - 1) * GridSpacing / 2.0f;
	const FVector GridCenterOffset = FVector(GridHalfSize, GridHalfSize, 0.f);
	// --- END OF ADDED CODE ---
	
	for (int32 i = 0; i < SelectedUnits.Num(); i++) {
		if (SelectedUnits[i] != CameraUnitWithTag)
		if (SelectedUnits[i] && SelectedUnits[i]->UnitState != UnitData::Dead) {
			
			//FVector RunLocation = Hit.Location + FVector(i / 2 * 100, i % 2 * 100, 0.f);
			int32 Row = i / GridSize;     // Row index
			int32 Col = i % GridSize;     // Column index

			//FVector RunLocation = Hit.Location + FVector(Col * 100, Row * 100, 0.f);  // Adjust x and y positions equally for a square grid
			//FVector RunLocation = Hit.Location + CalculateGridOffset(Row, Col);

			FVector RunLocation = (Hit.Location - GridCenterOffset) + CalculateGridOffset(Row, Col);
			
			bool HitNavModifier;
			RunLocation = TraceRunLocation(RunLocation, HitNavModifier);
			if (HitNavModifier) continue;
			
			float Speed = SelectedUnits[i]->Attributes->GetBaseRunSpeed();
		
			
			//UE_LOG(LogTemp, Warning, TEXT("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!Speed : %f"), Speed);
			if(SetBuildingWaypoint(RunLocation, SelectedUnits[i], BWaypoint, PlayWaypointSound))
			{
				//UE_LOG(LogTemp, Warning, TEXT("This is a Building!!"));
				//PlayWaypointSound = true;
			}else if (IsShiftPressed) {
				//UE_LOG(LogTemp, Warning, TEXT("IsShiftPressed!"));
				
				DrawDebugCircleAtLocation(GetWorld(), RunLocation, FColor::Green);
				
				if (!SelectedUnits[i]->IsInitialized) return;
				if (!SelectedUnits[i]->CanMove) return;
		
				if (SelectedUnits[i]->bIsMassUnit)
				{
					CorrectSetUnitMoveTarget(GetWorld(), SelectedUnits[i], RunLocation, Speed, 40.f);
					SetUnitState_Replication(SelectedUnits[i], 1);
				}
				else
				{
					RightClickRunShift(SelectedUnits[i], RunLocation);
				}
				
				PlayRunSound = true;
			}else if(UseUnrealEnginePathFinding)
			{
				DrawDebugCircleAtLocation(GetWorld(), RunLocation, FColor::Green);
				
				if (!SelectedUnits[i]->IsInitialized) return;
				if (!SelectedUnits[i]->CanMove) return;

				if (SelectedUnits[i]->bIsMassUnit)
				{
					CorrectSetUnitMoveTarget(GetWorld(), SelectedUnits[i], RunLocation, Speed, 40.f);
					SetUnitState_Replication(SelectedUnits[i], 1);
				}
				else
				{
					RightClickRunUEPF(SelectedUnits[i], RunLocation, true);
				}
				
				PlayRunSound = true;
			}
			else {
				//UE_LOG(LogTemp, Warning, TEXT("DIJKSTRA!"));
				DrawDebugCircleAtLocation(GetWorld(), RunLocation, FColor::Green);

				if (!SelectedUnits[i]->IsInitialized) return;
				if (!SelectedUnits[i]->CanMove) return;
				
				if (SelectedUnits[i]->bIsMassUnit)
				{
					CorrectSetUnitMoveTarget(GetWorld(), SelectedUnits[i], RunLocation, Speed, 40.f);
					SetUnitState_Replication(SelectedUnits[i], 1);
				}
				else
					RightClickRunDijkstraPF(SelectedUnits[i], RunLocation, i);
				
				
				PlayRunSound = true;
			}
		}
	}

	if (WaypointSound && PlayWaypointSound)
	{
		UGameplayStatics::PlaySound2D(this, WaypointSound);
	}

	if (RunSound && PlayRunSound)
	{
		const float CurrentTime = GetWorld()->GetTimeSeconds();
		if (CurrentTime - LastRunSoundTime >= RunSoundDelayTime) // Check if 3 seconds have passed
		{
			UGameplayStatics::PlaySound2D(this, RunSound);
			LastRunSoundTime = CurrentTime; // Update the timestamp
		}
	}
}
*/

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
    // --- ATTACK GRID MODE ---
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
            if (bNavMod) continue;

            if (SetBuildingWaypoint(RunLocation, U, BWaypoint, PlayWaypointSound))
            {
                // waypoint placed
            }
            else
            {
                DrawDebugCircleAtLocation(GetWorld(), RunLocation, FColor::Red);

                if (U->bIsMassUnit)
                {
                    LeftClickAttackMass(U, RunLocation, AttackToggled);
                }
                else
                {
                    LeftClickAttack(U, RunLocation);
                }

                PlayAttackSound = true;
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
                HUDBase->DeselectAllUnits();
                HUDBase->SetUnitSelected(HitUnit);
                DragUnitBase(HitUnit);

                if (CameraBase->AutoLockOnSelect)
                    LockCameraToUnit = true;
            }
            else
            {
                HUDBase->InitialPoint = HUDBase->GetMousePos2D();
                HUDBase->bSelectFriendly = true;
            }
        }
    }
}


/*
void ACustomControllerBase::LeftClickPressedMass()
{
	LeftClickIsPressed = true;
	AbilityArrayIndex = 0;
	
	if (!CameraBase || CameraBase->TabToggled) return;
	
	if(AltIsPressed)
	{
		DestroyWorkArea();
		for (int32 i = 0; i < SelectedUnits.Num(); i++)
		{
			CancelAbilitiesIfNoBuilding(SelectedUnits[i]);
		}
		
	}else if (AttackToggled) {
		FHitResult Hit;
		GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, false, Hit);

		int32 NumUnits = SelectedUnits.Num();
		const int32 GridSize = ComputeGridSize(NumUnits);
		AWaypoint* BWaypoint = nullptr;

		bool PlayWaypointSound = false;
		bool PlayAttackSound = false;
		
		for (int32 i = 0; i < SelectedUnits.Num(); i++)
		{
			if (SelectedUnits[i] != CameraUnitWithTag)
			{
				int32 Row = i / GridSize;     // Row index
				int32 Col = i % GridSize;     // Column index
				
				FVector RunLocation = Hit.Location + CalculateGridOffset(Row, Col);

				bool HitNavModifier;
				RunLocation = TraceRunLocation(RunLocation, HitNavModifier);
				if (HitNavModifier) continue;
				
				if(SetBuildingWaypoint(RunLocation, SelectedUnits[i], BWaypoint, PlayWaypointSound))
				{
					// Do Nothing
				}else
				{
					DrawDebugCircleAtLocation(GetWorld(), RunLocation, FColor::Red);
					if (SelectedUnits[i]->bIsMassUnit)
					{
						LeftClickAttackMass(SelectedUnits[i], RunLocation, AttackToggled);
					}else
					{
						LeftClickAttack(SelectedUnits[i], RunLocation);
					}
					
					PlayAttackSound = true;
				}
			}
			
			if (SelectedUnits[i])
				FireAbilityMouseHit(SelectedUnits[i], Hit);
		}
		
		AttackToggled = false;
		
		if (WaypointSound && PlayWaypointSound)
		{
			UGameplayStatics::PlaySound2D(this, WaypointSound);
		}

		if (AttackSound && PlayAttackSound)
		{
			UGameplayStatics::PlaySound2D(this, AttackSound);
		}
		
	}
	else {
		DropWorkArea();
		//LeftClickSelect_Implementation();

		
		FHitResult Hit_Pawn;
		GetHitResultUnderCursor(ECollisionChannel::ECC_Pawn, false, Hit_Pawn);

		bool AbilityFired = false;
		bool AbilityUnSynced = false;
		for (int32 i = 0; i < SelectedUnits.Num(); i++)
		{
				if (SelectedUnits[i] && SelectedUnits[i]->CurrentSnapshot.AbilityClass && SelectedUnits[i]->CurrentDraggedAbilityIndicator)
				{
						FireAbilityMouseHit(SelectedUnits[i], Hit_Pawn);
						AbilityFired = true;
				}else
				{
					AbilityUnSynced = true;
				}
		}
		
		if (AbilityFired && !AbilityUnSynced) return;
		
		if (Hit_Pawn.bBlockingHit && HUDBase)
		{
			AActor* HitActor = Hit_Pawn.GetActor();
			
			if(!HitActor->IsA(ALandscape::StaticClass()))
				ClickedActor = Hit_Pawn.GetActor();
			else
				ClickedActor = nullptr;
			
			AUnitBase* UnitBase = Cast<AUnitBase>(Hit_Pawn.GetActor());
			const ASpeakingUnit* SUnit = Cast<ASpeakingUnit>(Hit_Pawn.GetActor());
			
			if (UnitBase && (UnitBase->TeamId == SelectableTeamId || SelectableTeamId == 0) && !SUnit )
			{
				HUDBase->DeselectAllUnits();
				HUDBase->SetUnitSelected(UnitBase);
				DragUnitBase(UnitBase);
		
				
				if(CameraBase->AutoLockOnSelect)
					LockCameraToUnit = true;
			}
			else {
				HUDBase->InitialPoint = HUDBase->GetMousePos2D();
				HUDBase->bSelectFriendly = true;
			}
		}
	}
	
}
*/
void ACustomControllerBase::Server_ReportUnitVisibility_Implementation(APerformanceUnit* Unit, bool bVisible)
{
	if (IsValid(Unit))
	{
		Unit->SetClientVisibility(bVisible);
	}
}

void ACustomControllerBase::LeftClickAttackMass_Implementation(AUnitBase* Unit, FVector Location, bool AttackT)
{
	if (Unit && Unit->UnitState != UnitData::Dead) {
	
		FHitResult Hit_Pawn;
		GetHitResultUnderCursor(ECollisionChannel::ECC_Pawn, false, Hit_Pawn);

		if (Hit_Pawn.bBlockingHit)
		{
			AUnitBase* UnitBase = Cast<AUnitBase>(Hit_Pawn.GetActor());
					
			if(UnitBase && !UnitBase->TeamId)
			{
				/// Focus Enemy Units ///
				Unit->UnitToChase = UnitBase;
				SetUnitState_Replication(Unit, 3);
				
			}else if(UseUnrealEnginePathFinding)
			{
					
				if (Unit && Unit->UnitState != UnitData::Dead)
				{
					LeftClickAMoveUEPFMass(Unit, Location, AttackT);
				}
					
			}else
			{
				LeftClickAMove(Unit, Location);
			}
		}else if(UseUnrealEnginePathFinding)
		{
			if (Unit && Unit->UnitState != UnitData::Dead)
			{
				/// A-Move Units ///
				LeftClickAMoveUEPFMass(Unit, Location, AttackT);
			}
					
		}
			
	}
}

void ACustomControllerBase::LeftClickAMoveUEPFMass_Implementation(AUnitBase* Unit, FVector Location, bool AttackT)
{
	if (!Unit) return;

	if (Unit->CurrentSnapshot.AbilityClass)
	{
		UGameplayAbilityBase* AbilityCDO = Unit->CurrentSnapshot.AbilityClass->GetDefaultObject<UGameplayAbilityBase>();
		if (AbilityCDO && !AbilityCDO->AbilityCanBeCanceled) return;

		CancelCurrentAbility(Unit);
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
	UpdateSelectionCircles();
	if(Cast<AExtendedCameraBase>(GetPawn())->TabToggled)
	{
		SetWidgets(BestIndex);
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
