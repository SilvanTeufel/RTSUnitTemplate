// Fill out your copyright notice in the Description page of Project Settings.


#include "Controller/PlayerController/ExtendedControllerBase.h"

#include "EngineUtils.h"
#include "GameplayTagsManager.h"
#include "Landscape.h"
#include "MassSignalSubsystem.h"
#include "Characters/Camera/ExtendedCameraBase.h"
#include "Characters/Camera/RLAgent.h"
#include "Characters/Unit/BuildingBase.h"
#include "Actors/WorkArea.h"
#include "Actors/AbilityIndicator.h"
#include "Kismet/KismetSystemLibrary.h"  
#include "GameModes/ResourceGameMode.h"
#include "Mass/Signals/MySignals.h"
#include "Mass/MassActorBindingComponent.h"
#include "Hud/HUDBase.h"
#include "GameModes/RTSGameModeBase.h"
#include "Characters/Unit/UnitBase.h"
#include "Components/PrimitiveComponent.h"
#include "Components/CapsuleComponent.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavAreas/NavArea_Default.h"
#include "Engine/EngineTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Characters/Unit/ConstructionUnit.h"
#include "Mass/UnitMassTag.h"

// Helper: compute snap center/extent for any actor (works with ISMs too)
static bool GetActorBoundsForSnap(AActor* Actor, FVector& OutCenter, FVector& OutExtent)
{
	if (!Actor)
	{
		return false;
	}

	// Prefer explicit mesh on WorkArea
	if (AWorkArea* WA = Cast<AWorkArea>(Actor))
	{
		if (WA->Mesh)
		{
			const FBoxSphereBounds B = WA->Mesh->CalcBounds(WA->Mesh->GetComponentTransform());
			OutCenter = B.Origin;
			OutExtent = B.BoxExtent;
			return true;
		}
	}

	// For buildings: approximate footprint as a square using the capsule radius (radius*2 side length)
	if (ABuildingBase* Bld = Cast<ABuildingBase>(Actor))
	{
		if (UCapsuleComponent* Capsule = Bld->FindComponentByClass<UCapsuleComponent>())
		{
			const float R = Capsule->GetScaledCapsuleRadius();
			OutCenter = Bld->GetActorLocation();
			// Use radius for XY half-extents; keep Z from capsule half-height
			OutExtent.X = R;
			OutExtent.Y = R;
			OutExtent.Z = Capsule->GetScaledCapsuleHalfHeight();
			return true;
		}
	}

	// Otherwise, fall back to aggregate component bounds (works for ISM as well)
	const FBox CompBox = Actor->GetComponentsBoundingBox(/*bNonColliding=*/true);
	OutCenter = CompBox.GetCenter();
	OutExtent = CompBox.GetExtent();
	return true;
}

void AExtendedControllerBase::BeginPlay()
{
	Super::BeginPlay();

	if (LogSelectedEntity)
	{
		GetWorld()->GetTimerManager().SetTimer(
		  LogTagsTimerHandle, 
		  this, 
		  &AExtendedControllerBase::LogSelectedUnitsTags, 
		  0.5f, 
		  true 
	  );
	}
}

void AExtendedControllerBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	MoveDraggedUnit_Implementation(DeltaSeconds);
	MoveWorkArea_Local(DeltaSeconds);
	MoveAbilityIndicator_Local(DeltaSeconds);
	UpdateExtractionSounds(DeltaSeconds);
}

void AExtendedControllerBase::LogSelectedUnitsTags()
{
	if (!LogSelectedEntity) return;
    // Get the world, which is the entry point to all engine subsystems.
    const UWorld* World = GetWorld();
    if (!IsValid(World))
    {
        UE_LOG(LogTemp, Error, TEXT("LogSelectedUnitsTags: World is not valid."));
        return;
    }

    // Access the Mass Entity Subsystem, which is the main manager for Mass entities.
    const UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
    if (!EntitySubsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("LogSelectedUnitsTags: UMassEntitySubsystem could not be found. Is the Mass plugin enabled and configured?"));
        return;
    }

    // Get a reference to the EntityManager. This is used to interact with the underlying entity data.
    const FMassEntityManager& EntityManager = EntitySubsystem->GetEntityManager();

    if (SelectedUnits.IsEmpty())
    {
        // It can be useful to know when the function runs but there's nothing to process.
        // UE_LOG(LogTemp, Verbose, TEXT("LogSelectedUnitsTags: Called, but SelectedUnits array is empty."));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("--- Logging Tags for %d Selected Units ---"), SelectedUnits.Num());

    // Iterate over each pointer in the SelectedUnits array.
    for (int32 Index = 0; Index < SelectedUnits.Num(); ++Index)
    {
        AUnitBase* SelectedUnit = SelectedUnits[Index];
        if (!IsValid(SelectedUnit))
        {
            UE_LOG(LogTemp, Warning, TEXT("LogSelectedUnitsTags: Unit at index %d is not valid."), Index);
            continue; // Skip to the next element in the array.
        }

        // Each actor that is part of the Mass simulation should have a MassActorBindingsComponent.
        const UMassActorBindingComponent* BindingsComponent = SelectedUnit->FindComponentByClass<UMassActorBindingComponent>();
        if (!BindingsComponent)
        {
            UE_LOG(LogTemp, Warning, TEXT("LogSelectedUnitsTags: Unit '%s' does not have a UMassActorBindingsComponent."), *SelectedUnit->GetName());
            continue;
        }

        // Get the actual Mass Entity handle from the component.
        const FMassEntityHandle Entity = BindingsComponent->GetMassEntityHandle();
        if (!Entity.IsSet())
        {
            UE_LOG(LogTemp, Warning, TEXT("LogSelectedUnitsTags: Unit '%s' has a binding component but does not have a valid Mass Entity."), *SelectedUnit->GetName());
            continue;
        }

        // This is the core debug utility that prints the entity's tags.
        UE::Mass::Debug::LogEntityTags(Entity, EntityManager, World);
    }

    UE_LOG(LogTemp, Log, TEXT("--- Finished Logging Tags ---"));
}

void AExtendedControllerBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}

void AExtendedControllerBase::Client_ApplyCustomizations_Implementation(USoundBase* InWaypointSound,
	USoundBase* InRunSound, USoundBase* InAbilitySound, USoundBase* InAttackSound,
	USoundBase* InDropWorkAreaFailedSound, USoundBase* InDropWorkAreaSound)
{
	WaypointSound = InWaypointSound;
	RunSound = InRunSound;
	AbilitySound = InAbilitySound;
	AttackSound = InAttackSound;
	DropWorkAreaFailedSound = InDropWorkAreaFailedSound;
	DropWorkAreaSound = InDropWorkAreaSound;
}

void AExtendedControllerBase::Client_PlaySound2D_Implementation(USoundBase* Sound, float VolumeMultiplier, float PitchMultiplier)
{
	if (Sound)
	{
		UGameplayStatics::PlaySound2D(this, Sound, VolumeMultiplier * GetSoundMultiplier(), PitchMultiplier);
	}
}

void AExtendedControllerBase::ActivateAbilitiesByIndex_Implementation(AGASUnit* UnitBase, EGASAbilityInputID InputID, const FHitResult& HitResult)
{
	if (!UnitBase)
	{
		return;
	}


	
	switch (AbilityArrayIndex)
	{
	case 0:
		ActivateDefaultAbilities(UnitBase, InputID, HitResult);
		break;
	case 1:
		ActivateSecondAbilities(UnitBase, InputID, HitResult);
		break;
	case 2:
		ActivateThirdAbilities(UnitBase, InputID, HitResult);
		break;
	case 3:
		ActivateFourthAbilities(UnitBase, InputID, HitResult);
		break;
	default:
		// Optionally handle invalid indices
			break;
	}
}

void AExtendedControllerBase::ActivateDefaultAbilities_Implementation(AGASUnit* UnitBase, EGASAbilityInputID InputID, const FHitResult& HitResult)
{
	if(UnitBase && UnitBase->DefaultAbilities.Num())
	{
		UnitBase->ActivateAbilityByInputID(InputID, UnitBase->DefaultAbilities, HitResult, this);
	}
	
}

void AExtendedControllerBase::ActivateSecondAbilities_Implementation(AGASUnit* UnitBase, EGASAbilityInputID InputID, const FHitResult& HitResult)
{
	if (UnitBase && UnitBase->SecondAbilities.Num() > 0)
	{
		UnitBase->ActivateAbilityByInputID(InputID, UnitBase->SecondAbilities, HitResult, this);
	}
}

void AExtendedControllerBase::ActivateThirdAbilities_Implementation(AGASUnit* UnitBase, EGASAbilityInputID InputID, const FHitResult& HitResult)
{
	if (UnitBase && UnitBase->ThirdAbilities.Num() > 0)
	{
		UnitBase->ActivateAbilityByInputID(InputID, UnitBase->ThirdAbilities, HitResult, this);
	}
}

void AExtendedControllerBase::ActivateFourthAbilities_Implementation(AGASUnit* UnitBase, EGASAbilityInputID InputID, const FHitResult& HitResult)
{
	if (UnitBase && UnitBase->FourthAbilities.Num() > 0)
	{
		UnitBase->ActivateAbilityByInputID(InputID, UnitBase->FourthAbilities, HitResult, this);
	}
}

void AExtendedControllerBase::ActivateAbilities_Implementation(AGASUnit* UnitBase, EGASAbilityInputID InputID, const TArray<TSubclassOf<UGameplayAbilityBase>>& Abilities)
{
	if (UnitBase && Abilities.Num() > 0)
	{
		UnitBase->ActivateAbilityByInputID(InputID, Abilities, FHitResult(), this);
	}
}

void AExtendedControllerBase::ActivateDefaultAbilitiesByTag(EGASAbilityInputID InputID, FGameplayTag Tag)
{
	for (int32 i = 0; i < RTSGameMode->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(RTSGameMode->AllUnits[i]);
		if (Unit && Unit->UnitTags.HasAnyExact(FGameplayTagContainer(Tag)) && (Unit->TeamId == SelectableTeamId))
		{
			ActivateDefaultAbilities_Implementation(Unit, InputID, FHitResult());
		}
	}
}


TArray<TSubclassOf<UGameplayAbilityBase>> AExtendedControllerBase::GetAbilityArrayByIndex()
{

	if (!SelectedUnits.Num()) return TArray<TSubclassOf<UGameplayAbilityBase>>();
	if (CurrentUnitWidgetIndex < 0 || CurrentUnitWidgetIndex >= SelectedUnits.Num()) return TArray<TSubclassOf<UGameplayAbilityBase>>();
	if (!SelectedUnits[CurrentUnitWidgetIndex]) return TArray<TSubclassOf<UGameplayAbilityBase>>();
	
	switch (AbilityArrayIndex)
	{
	case 0:
		return SelectedUnits[CurrentUnitWidgetIndex]->DefaultAbilities;
	case 1:
		return SelectedUnits[CurrentUnitWidgetIndex]->SecondAbilities;
	case 2:
		return SelectedUnits[CurrentUnitWidgetIndex]->ThirdAbilities;
	case 3:
		return SelectedUnits[CurrentUnitWidgetIndex]->FourthAbilities;
	default:
		return SelectedUnits[CurrentUnitWidgetIndex]->DefaultAbilities;
	}
}

void AExtendedControllerBase::AddAbilityIndex(int Add)
{
	if (!SelectedUnits.Num()) return;
	if (CurrentUnitWidgetIndex < 0 || CurrentUnitWidgetIndex >= SelectedUnits.Num()) return;
	if (!SelectedUnits[CurrentUnitWidgetIndex]) return;
	

	int NewIndex = AbilityArrayIndex+=Add;


	if (NewIndex < 0)
	{
		NewIndex = MaxAbilityArrayIndex;
		AddToCurrentUnitWidgetIndex(Add);
	}

	if (NewIndex == 0 && !SelectedUnits[CurrentUnitWidgetIndex]->DefaultAbilities.Num())
		NewIndex+=Add;
	
	if (NewIndex == 1 && !SelectedUnits[CurrentUnitWidgetIndex]->SecondAbilities.Num())
		NewIndex+=Add;
	 
	if (NewIndex == 2 && !SelectedUnits[CurrentUnitWidgetIndex]->ThirdAbilities.Num())
		NewIndex+=Add;
	
	if (NewIndex == 3 && !SelectedUnits[CurrentUnitWidgetIndex]->FourthAbilities.Num())
		NewIndex+=Add;

	if (NewIndex == 2 && !SelectedUnits[CurrentUnitWidgetIndex]->ThirdAbilities.Num())
		NewIndex+=Add;

	if (NewIndex == 1 && !SelectedUnits[CurrentUnitWidgetIndex]->SecondAbilities.Num())
		NewIndex+=Add;

	if (NewIndex == 0 && !SelectedUnits[CurrentUnitWidgetIndex]->DefaultAbilities.Num())
		NewIndex+=Add;

	if (NewIndex > MaxAbilityArrayIndex)
	{
		NewIndex = 0;
		AddToCurrentUnitWidgetIndex(Add);
	}
	
	AbilityArrayIndex=NewIndex;
}


void AExtendedControllerBase::ApplyMovementInputToUnit_Implementation(const FVector& Direction, float Scale, AUnitBase* Unit, int TeamId)
{
	if (Unit && (Unit->TeamId == TeamId))
	{
		if (Unit->GetController()) // Ensure the unit has a valid Controller
		{
			Unit->AddMovementInput(Direction, Scale);
			Unit->SetUnitState(UnitData::Run);
		}
	}
}

void AExtendedControllerBase::GetClosestUnitTo(FVector Position, int PlayerTeamId, EGASAbilityInputID InputID)
{
		if(!RTSGameMode || !RTSGameMode->AllUnits.Num()) return;
	
		AUnitBase* ClosestUnit = nullptr;
		float ClosestDistanceSquared = FLT_MAX;
		// If we are on the server, run the logic directly.
		for (AActor* UnitActor  : RTSGameMode->AllUnits)
		{
 			// Cast to AGASUnit to make sure it's of the correct type
			AUnitBase* Unit = Cast<AUnitBase>(UnitActor);
			// Check if the unit is valid and has the same TeamId as the camera and is eligible for selection
			if (Unit && Unit->IsWorker && Unit->TeamId == PlayerTeamId && !Unit->BuildArea && Unit->CanBeSelected) // && !Unit->BuildArea
			{
				float DistanceSquared = FVector::DistSquared(Position, Unit->GetActorLocation());
				// Check if this unit is closer than the currently tracked closest unit
				if (DistanceSquared < ClosestDistanceSquared)
				{
					ClosestDistanceSquared = DistanceSquared;
					ClosestUnit = Unit;
				}
			}
		}
		
		if (ClosestUnit)
		{
	
			ClosestUnit->SetSelected();
			SelectedUnits.Emplace(ClosestUnit);
			HUDBase->SetUnitSelected(ClosestUnit);
			AbilityArrayIndex = 0;
		}
}

void AExtendedControllerBase::ServerGetClosestUnitTo_Implementation(FVector Position, int PlayerTeamId, EGASAbilityInputID InputID)
{

	if(!RTSGameMode || !RTSGameMode->AllUnits.Num()) return;
	
	AUnitBase* ClosestUnit = nullptr;
	float ClosestDistanceSquared = FLT_MAX;

	for (AActor* UnitActor : RTSGameMode->AllUnits)
	{
		// Cast to AUnitBase to make sure it's of the correct type
		AUnitBase* Unit = Cast<AUnitBase>(UnitActor);
		// Check if the unit is valid, belongs to the player, is a worker, not building, and is selectable
		if (Unit && Unit->IsWorker && Unit->TeamId == PlayerTeamId && !Unit->BuildArea && Unit->CanBeSelected) // && !Unit->BuildArea
		{
			float DistanceSquared = FVector::DistSquared(Position, Unit->GetActorLocation());
			// Check if this unit is closer than the currently tracked closest unit
			if (DistanceSquared < ClosestDistanceSquared)
			{
				ClosestDistanceSquared = DistanceSquared;
				ClosestUnit = Unit;
			}
		}
	}

	// After finding the closest unit, notify the client
	ClientReceiveClosestUnit(ClosestUnit, InputID);
	//ActivateDefaultAbilities_Implementation(ClosestUnit, InputID, FHitResult());
}

void AExtendedControllerBase::ClientReceiveClosestUnit_Implementation(AUnitBase* ClosestUnit, EGASAbilityInputID InputID)
{

	if (ClosestUnit)
	{
		ClosestUnit->SetSelected();
		SelectedUnits.Emplace(ClosestUnit);
		HUDBase->SetUnitSelected(ClosestUnit);
	}
	// Update your local CloseUnit reference with ClosestUnit here.
	// Example:
	// CloseUnit = ClosestUnit;
}

void AExtendedControllerBase::ActivateKeyboardAbilitiesOnCloseUnits(EGASAbilityInputID InputID, FVector CameraLocation, int PlayerTeamId, AHUDBase* HUD)
{

	if (HasAuthority())
	{
		GetClosestUnitTo(CameraLocation, PlayerTeamId, InputID);
	}
	else
	{
		// If we are on the client, request it from the server.
		ServerGetClosestUnitTo(CameraLocation, PlayerTeamId, InputID);
	}
}

void AExtendedControllerBase::ActivateKeyboardAbilitiesOnMultipleUnits(EGASAbilityInputID InputID)
{
	
	FHitResult Hit;
	GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, false, Hit);


	if (AbilitySound)
	{
		UGameplayStatics::PlaySound2D(this, AbilitySound, GetSoundMultiplier());
	}
	
	bool bActivatedAny = false;
	if (SelectedUnits.Num() > 0)
	{
		// Validate CurrentUnitWidgetIndex is within bounds
		if (CurrentUnitWidgetIndex < 0 || CurrentUnitWidgetIndex >= SelectedUnits.Num())
		{
			CurrentUnitWidgetIndex = 0;
		}
		
		AUnitBase* CurrentUnit = SelectedUnits[CurrentUnitWidgetIndex];
		
		// Check if the current unit is valid before accessing its properties
		if (CurrentUnit && CurrentUnit->IsWorker && CurrentUnit->CanActivateAbilities)
		{
			ActivateAbilitiesByIndex_Implementation(CurrentUnit, InputID, Hit);
			bActivatedAny = true;
			HUDBase->SetUnitSelected(CurrentUnit);
			CurrentUnitWidgetIndex = 0;
			SelectedUnits = HUDBase->SelectedUnits;
		}else if (CurrentUnit) {
			bool bAnyHasTag = false;

			
			for (AUnitBase* SelectedUnit : SelectedUnits)
			{
				if (SelectedUnit && SelectedUnit->CanActivateAbilities && !SelectedUnit->IsWorker && SelectedUnit->AbilitySelectionTag == CurrentUnit->AbilitySelectionTag)
				{
					bAnyHasTag = true;
					ActivateAbilitiesByIndex_Implementation(SelectedUnit, InputID, Hit);
					bActivatedAny = true;
				}
			}

			if (!bAnyHasTag)
			{
				for (AUnitBase* SelectedUnit : SelectedUnits)
				{
					if (SelectedUnit && SelectedUnit->CanActivateAbilities && !SelectedUnit->IsWorker)
					{
						ActivateAbilitiesByIndex_Implementation(SelectedUnit, InputID, Hit);
						bActivatedAny = true;
					}
				}
			}
			
		}
	}
	else if (CameraUnitWithTag && CameraUnitWithTag->CanActivateAbilities)
	{
		CameraUnitWithTag->SetSelected();
		SelectedUnits.Emplace(CameraUnitWithTag);
		HUDBase->SetUnitSelected(CameraUnitWithTag);
		ActivateAbilitiesByIndex_Implementation(CameraUnitWithTag, InputID, Hit);
		bActivatedAny = true;
	}
	else if(HUDBase)
	{
		FVector CameraLocation = GetPawn()->GetActorLocation();
		ActivateKeyboardAbilitiesOnCloseUnits(InputID, CameraLocation, SelectableTeamId, HUDBase);
		// Activation may happen asynchronously via server response; leave flag unchanged here
	}

	if (bActivatedAny)
	{
		bUsedKeyboardAbilityBeforeClick = true;
	}
}

void AExtendedControllerBase::SetWorkAreaPosition_Implementation(AWorkArea* DraggedArea, FTransform NewActorTransform)
{
   if (!DraggedArea) return;
	
    UStaticMeshComponent* MeshComponent = DraggedArea->FindComponentByClass<UStaticMeshComponent>();
    if (!MeshComponent)
    {
        DraggedArea->SetActorTransform(NewActorTransform);
        return;
    }

    // 1) Apply transform (Location and Rotation)
    DraggedArea->SetActorTransform(NewActorTransform);
	
    // 2) Bounds neu holen (jetzt mit richtiger Weltposition und Rotation)
    FBoxSphereBounds MeshBounds = MeshComponent->Bounds;
    // „halbe Höhe“ des Meshes (inkl. Scale & Rotation, da in Weltkoordinaten)
    float HalfHeight = MeshBounds.BoxExtent.Z;

    // 3) LineTrace von etwas über dem Bodensatz des Meshes nach unten
    FVector MeshBottomWorld = MeshBounds.Origin - FVector(0.f, 0.f, HalfHeight);
    FVector TraceStart = MeshBottomWorld + FVector(0.f, 0.f, 1000.f);
    FVector TraceEnd   = MeshBottomWorld - FVector(0.f, 0.f, 100000.f);

    FHitResult HitResult;
    FCollisionQueryParams TraceParams(FName(TEXT("WorkAreaGroundTrace")), true, DraggedArea);
    //TraceParams.AddIgnoredActor(DraggedArea);
	for (TActorIterator<AWorkArea> It(GetWorld()); It; ++It)
	{
		TraceParams.AddIgnoredActor(*It);
	}

    {
        // Iteratively ignore non-landscape hits until we find Landscape or no hit
        FHitResult LocalHit;
        FCollisionQueryParams LocalParams = TraceParams;
        FVector LocalStart = TraceStart;
        FVector LocalEnd = TraceEnd;
        const int32 MaxTries = 8;
        int32 Tries = 0;
        bool bFoundLandscape = false;
        FVector AdjustedLocation = NewActorTransform.GetLocation();
        while (Tries < MaxTries && GetWorld()->LineTraceSingleByChannel(LocalHit, LocalStart, LocalEnd, ECC_Visibility, LocalParams))
        {
            if (LocalHit.GetActor() && LocalHit.GetActor()->IsA(ALandscape::StaticClass()))
            {
                // Setze die neue Z-Position so, dass MeshBottomWorld auf dem Boden liegt
                float DeltaZ = LocalHit.ImpactPoint.Z - MeshBottomWorld.Z;
                AdjustedLocation.Z += DeltaZ;
                bFoundLandscape = true;
                break;
            }
            LocalParams.AddIgnoredActor(LocalHit.GetActor());
            LocalStart = LocalHit.ImpactPoint - FVector(0.f, 0.f, 1.f);
            ++Tries;
        }
        if (bFoundLandscape)
        {
            DraggedArea->SetActorLocation(AdjustedLocation);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("SetWorkAreaPosition_Implementation: Kein Landscape-Boden getroffen!"));
        }
    }
	
	DraggedArea->ForceNetUpdate();
}

void AExtendedControllerBase::BroadcastWorkAreaPositionToTeam(AWorkArea* DraggedArea, const FTransform& FinalTransform, AUnitBase* UnitBase)
{
	if (!DraggedArea) return;
	
	UWorld* World = GetWorld();
	if (World)
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			AExtendedControllerBase* ExtPC = Cast<AExtendedControllerBase>(PC);
			if (!ExtPC) continue;

			// Compare the controller's selectable team against the WorkArea's team
			if (ExtPC->SelectableTeamId == DraggedArea->TeamId)
			{
				ExtPC->Client_UpdateWorkAreaPosition(DraggedArea, FinalTransform, UnitBase);
			}
		}
	}
}

void AExtendedControllerBase::Server_FinalizeWorkAreaPosition_Implementation(AWorkArea* DraggedArea, FTransform NewActorTransform, AUnitBase* UnitBase)
{
	if (!DraggedArea) return;

	FTransform FinalTransform = NewActorTransform;

	if (!DraggedArea->IsExtensionArea)
	{
		// Compute grounded position on the server to ensure mesh bottom sits on ground
		FinalTransform.SetLocation(ComputeGroundedLocation(DraggedArea, NewActorTransform.GetLocation()));
	}
	
	// Only delay if both IsExtensionArea and InstantDrop are true
	if (DraggedArea->IsExtensionArea && DraggedArea->InstantDrop)
	{
		// Delay the client update by 1 second
		FTimerHandle TimerHandle;
		TWeakObjectPtr<AWorkArea> WeakDraggedArea = DraggedArea;
		TWeakObjectPtr<AUnitBase> WeakUnitBase = UnitBase;
		
		GetWorldTimerManager().SetTimer(TimerHandle, [this, WeakDraggedArea, FinalTransform, WeakUnitBase]()
		{
			if (!WeakDraggedArea.IsValid()) return;
			BroadcastWorkAreaPositionToTeam(WeakDraggedArea.Get(), FinalTransform, WeakUnitBase.Get());
		}, 1.0f, false);
	}
	else
	{
		// Execute immediately without delay
		BroadcastWorkAreaPositionToTeam(DraggedArea, FinalTransform, UnitBase);
	}
}

void AExtendedControllerBase::Multicast_ApplyWorkAreaPosition_Implementation(AWorkArea* DraggedArea, FTransform NewActorTransform, AUnitBase* UnitBase)
{
	if (!DraggedArea) return;

	DraggedArea->SetActorTransform(NewActorTransform);

	//UnitBase->ShowWorkAreaIfNoFog(DraggedArea);
}

void AExtendedControllerBase::Client_UpdateWorkAreaPosition_Implementation(AWorkArea* DraggedArea, FTransform NewActorTransform, AUnitBase* UnitBase)
{
	if (!DraggedArea) return;
	DraggedArea->SetActorTransform(NewActorTransform);

	UnitBase->ShowWorkAreaIfNoFog_Implementation(DraggedArea);
	
}

FVector AExtendedControllerBase::ComputeGroundedLocation(AWorkArea* DraggedArea, const FVector& DesiredLocation) const
{
	if (!DraggedArea) return DesiredLocation;

	UStaticMeshComponent* MeshComponent = DraggedArea->FindComponentByClass<UStaticMeshComponent>();
	if (!MeshComponent)
	{
		return DesiredLocation; // No mesh, use desired location as-is
	}

	// Use current bounds to understand bottom offset from actor location
	const FBoxSphereBounds MeshBounds = MeshComponent->CalcBounds(MeshComponent->GetComponentTransform());
	const float HalfHeight = MeshBounds.BoxExtent.Z;
	const float CurrentBottomZ = MeshBounds.Origin.Z - HalfHeight;
	const float OffsetActorToBottom = CurrentBottomZ - DraggedArea->GetActorLocation().Z; // how far below actor loc the mesh bottom currently is

	// Trace straight down from above the desired XY to find ground (Landscape only)
	const FVector TraceStart = FVector(DesiredLocation.X, DesiredLocation.Y, DesiredLocation.Z + 2000.f);
	const FVector TraceEnd   = FVector(DesiredLocation.X, DesiredLocation.Y, DesiredLocation.Z - 10000.f);

	FHitResult Hit;
	FVector Result = DesiredLocation;
	if (UWorld* World = DraggedArea->GetWorld())
	{
		FCollisionQueryParams Params(FName(TEXT("WorkArea_GroundTrace")), true, DraggedArea);
		// Ignore all WorkAreas so we hit world
		for (TActorIterator<AWorkArea> It(World); It; ++It)
		{
			Params.AddIgnoredActor(*It);
		}

		// Iteratively ignore non-landscape hits until we find Landscape or no hit
		const int32 MaxTries = 8;
		int32 Tries = 0;
		FCollisionQueryParams LocalParams = Params;
		FVector LocalStart = TraceStart;
		FVector LocalEnd = TraceEnd;
		while (Tries < MaxTries && World->LineTraceSingleByChannel(Hit, LocalStart, LocalEnd, ECC_Visibility, LocalParams))
		{
			if (Hit.GetActor() && Hit.GetActor()->IsA(ALandscape::StaticClass()))
			{
				Result.Z = Hit.ImpactPoint.Z - OffsetActorToBottom;
				return Result;
			}
			// Not a landscape: ignore and continue tracing further down from just below this hit
			LocalParams.AddIgnoredActor(Hit.GetActor());
			LocalStart = Hit.ImpactPoint - FVector(0.f, 0.f, 1.f);
			++Tries;
		}
	}
	return Result;
}


AActor* AExtendedControllerBase::CheckForSnapOverlap(AWorkArea* DraggedActor, const FVector& TestLocation)
{
	if (!DraggedActor) return nullptr;

	// Get the bounding box of DraggedActor’s mesh
	UStaticMeshComponent* DraggedMesh = DraggedActor->FindComponentByClass<UStaticMeshComponent>();
	if (!DraggedMesh) return nullptr;

	FBoxSphereBounds DraggedBounds = DraggedMesh->CalcBounds(DraggedMesh->GetComponentTransform());
	FVector Extent = DraggedBounds.BoxExtent; // half-size
	// We'll test with a BoxOverlap at the new position
	TArray<AActor*> OverlappedActors;

	bool bAnyOverlap = UKismetSystemLibrary::BoxOverlapActors(
		this,                              // WorldContext
		TestLocation,                      // Location for the box center
		Extent,                            // Half-size extents
		TArray<TEnumAsByte<EObjectTypeQuery>>(),  // Object types to consider
		AActor::StaticClass(),             // Class filter
		TArray<AActor*>(),                 // Actors to ignore
		OverlappedActors
	);

	if (bAnyOverlap)
	{
		for (AActor* Overlapped : OverlappedActors)
		{
			if (!Overlapped || Overlapped == DraggedActor)
				continue;

			// If it’s a WorkArea or Building, it's a snap-relevant overlap
			if (Cast<AWorkArea>(Overlapped) || Cast<ABuildingBase>(Overlapped))
			{
				return Overlapped; // Return the first relevant overlap
			}
		}
	}

	return nullptr; // No relevant overlaps
}


void AExtendedControllerBase::SnapToActor(AWorkArea* DraggedActor, AActor* OtherActor, UStaticMeshComponent* OtherMesh)
{
    if (!DraggedActor || !OtherActor || DraggedActor == OtherActor)
        return;

    // Throttle initiating a new snap target to avoid flicker
    const float Now = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f);
    const bool bTryingNewTarget = (CurrentSnapActor != OtherActor);
    if (bTryingNewTarget && Now < NextAllowedSnapTime)
    {
        return;
    }

    // 1) Get the dragged mesh
    UStaticMeshComponent* DraggedMesh = DraggedActor->Mesh;
    if (!DraggedMesh)
    {
        UE_LOG(LogTemp, Warning, TEXT("DraggedActor has no StaticMeshComponent!"));
        return;
    }

    // 2) Compute each actor's bounds in world space
    const FBoxSphereBounds DraggedBounds = DraggedMesh->CalcBounds(DraggedMesh->GetComponentTransform());
    const FVector DraggedCenter = DraggedBounds.Origin;
    const FVector DraggedExtent = DraggedBounds.BoxExtent; // half-size in X, Y, Z
    const FVector CenterToActorOffset = DraggedCenter - DraggedActor->GetActorLocation(); // place actor so mesh center lands where we compute

    FVector OtherCenter, OtherExtent;
    if (OtherMesh)
    {
        const FBoxSphereBounds OtherBounds = OtherMesh->CalcBounds(OtherMesh->GetComponentTransform());
        OtherCenter = OtherBounds.Origin;
        OtherExtent = OtherBounds.BoxExtent;
    }
    else
    {
        if (!GetActorBoundsForSnap(OtherActor, OtherCenter, OtherExtent))
        {
            UE_LOG(LogTemp, Warning, TEXT("SnapToActor: Could not derive bounds for OtherActor %s"), *OtherActor->GetName());
            return;
        }
    }

    // Determine effective gap (allow per-building adjustment)
    float EffectiveGap = SnapGap;
    ABuildingBase* TargetBuilding = Cast<ABuildingBase>(OtherActor);
    if (TargetBuilding)
    {
        EffectiveGap = FMath::Max(0.f, SnapGap + TargetBuilding->SnapGapAdjustment);

        // Treat the building like a cube with length/width = CapsuleRadius*2.
        // Use the capsule as the footprint when available and take actor location as center.
        if (UCapsuleComponent* Capsule = TargetBuilding->FindComponentByClass<UCapsuleComponent>())
        {
            const float R = Capsule->GetScaledCapsuleRadius();
            OtherExtent.X = R;
            OtherExtent.Y = R;
            OtherCenter = TargetBuilding->GetActorLocation();
        }
    }

    // Prevent vertical/on-top stacking: compute a "too close" threshold based on target footprint
    float TooCloseThreshold = 0.f;
    {
        float CandidateRadius = FMath::Max(OtherExtent.X, OtherExtent.Y);
        if (TargetBuilding)
        {
            if (UCapsuleComponent* Capsule = TargetBuilding->FindComponentByClass<UCapsuleComponent>())
            {
                CandidateRadius = Capsule->GetScaledCapsuleRadius();
            }
        }
        TooCloseThreshold = CandidateRadius * (2.f / 3.f);
    }

    // 3) Mouse reference on ground
    FVector Ref = DraggedActor->GetActorLocation();
    {
        FHitResult CursorHit;
        if (GetHitResultUnderCursor(ECC_Visibility, false, CursorHit))
        {
            Ref = CursorHit.Location;
        }
        else
        {
            // Fallback: deproject and trace to ground
            FVector MousePos, MouseDir;
            if (DeprojectMousePositionToWorld(MousePos, MouseDir))
            {
                const FVector TraceStart = MousePos;
                const FVector TraceEnd = TraceStart + MouseDir * 1000000.f;
                FCollisionQueryParams Params;
                for (TActorIterator<AWorkArea> It(GetWorld()); It; ++It)
                {
                    Params.AddIgnoredActor(*It);
                }
                FHitResult GroundHit;
                if (GetWorld() && GetWorld()->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility, Params))
                {
                    Ref = GroundHit.Location;
                }
            }
        }
    }

    // Constrain reference to NavMesh; if not on nav, do not move (freeze)
    if (UWorld* World = GetWorld())
    {
        if (UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World))
        {
            FNavLocation NavLoc;
            if (NavSys->ProjectPointToNavigation(Ref, NavLoc, FVector(100.f,100.f,300.f)))
            {
                Ref = NavLoc.Location;
            }
            else
            {
                return; // cursor not over navmesh; stop snapping/moving
            }
        }
    }

    // 4) Dynamic release: based on MOUSE to TARGET center
    const float DragXY = FMath::Max(DraggedExtent.X, DraggedExtent.Y);
    const float OtherXY = FMath::Max(OtherExtent.X, OtherExtent.Y);
    const float ReleaseThreshold = DragXY + OtherXY + 150.f;
    const float MouseToTarget = FVector::Dist2D(Ref, OtherCenter);
    const float NowTime = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f);

    // Debounced unsnap: require sustained time beyond release threshold
    if (MouseToTarget > ReleaseThreshold)
    {
        if (LastBeyondReleaseTime < 0.f)
        {
            LastBeyondReleaseTime = NowTime;
        }
        const float Elapsed = NowTime - LastBeyondReleaseTime;
        if (Elapsed >= UnsnapGraceSeconds)
        {
            if (CurrentSnapActor == OtherActor) { CurrentSnapActor = nullptr; }
            WorkAreaIsSnapped = false;
            LastBeyondReleaseTime = -1.f;
            // Immediately move dragged area to follow the cursor so it visually releases this frame
            FVector Desired = FVector(Ref.X, Ref.Y, DraggedActor->GetActorLocation().Z + DraggedAreaZOffset);
            const FVector GroundedFree = ComputeGroundedLocation(DraggedActor, Desired);
            DraggedActor->SetActorLocation(GroundedFree);
            return;
        }
        else
        {
            // Keep snapping while grace period not elapsed
            // Fall through to compute snap position
        }
    }
    else
    {
        // Back within threshold, reset timer
        LastBeyondReleaseTime = -1.f;
    }

    // If this is a new target, apply an acquire hysteresis (tighter threshold) to commit the snap
    const float AcquireThreshold = ReleaseThreshold * FMath::Clamp(AcquireHysteresisFactor, 0.1f, 1.0f);
    if (bTryingNewTarget && MouseToTarget > AcquireThreshold)
    {
        return;
    }

    // 5) Axis-aligned snapping (consistent gap, corrected for actor/center offset)
    FVector SnappedActorLocation = DraggedActor->GetActorLocation();

    const float XOffset = DraggedExtent.X + OtherExtent.X + EffectiveGap;
    const float YOffset = DraggedExtent.Y + OtherExtent.Y + EffectiveGap;

    const float dx = FMath::Abs(Ref.X - OtherCenter.X);
    const float dy = FMath::Abs(Ref.Y - OtherCenter.Y);

    FVector DesiredCenter = DraggedCenter; // start from current center
    if (dx < dy)
    {
        // Closer on X, keep same X to align columns
        DesiredCenter.X = OtherCenter.X;
        // Offset Y so they’re side-by-side, choose side from reference point
        const float SignY = (Ref.Y >= OtherCenter.Y) ? 1.f : -1.f;
        DesiredCenter.Y = OtherCenter.Y + SignY * YOffset;
    }
    else
    {
        // Closer on Y, keep same Y to align rows
        DesiredCenter.Y = OtherCenter.Y;
        // Offset X side from reference point
        const float SignX = (Ref.X >= OtherCenter.X) ? 1.f : -1.f;
        DesiredCenter.X = OtherCenter.X + SignX * XOffset;
    }

    // Convert desired center to actor location
    SnappedActorLocation.X = DesiredCenter.X - CenterToActorOffset.X;
    SnappedActorLocation.Y = DesiredCenter.Y - CenterToActorOffset.Y;

    // Safety: if the resulting center is still too close to the target center, push out along the dominant axis
    {
        const FVector ResultingCenter = FVector(
            SnappedActorLocation.X + CenterToActorOffset.X,
            SnappedActorLocation.Y + CenterToActorOffset.Y,
            0.f);
        const float CenterDist2D = FVector::Dist2D(ResultingCenter, FVector(OtherCenter.X, OtherCenter.Y, 0.f));
        if (CenterDist2D < TooCloseThreshold)
        {
            if (dx < dy)
            {
                // We aligned X to target; push along Y further
                const float SignY = (Ref.Y >= OtherCenter.Y) ? 1.f : -1.f;
                DesiredCenter.Y = OtherCenter.Y + SignY * (YOffset + (TooCloseThreshold - CenterDist2D));
            }
            else
            {
                // We aligned Y to target; push along X further
                const float SignX = (Ref.X >= OtherCenter.X) ? 1.f : -1.f;
                DesiredCenter.X = OtherCenter.X + SignX * (XOffset + (TooCloseThreshold - CenterDist2D));
            }
            SnappedActorLocation.X = DesiredCenter.X - CenterToActorOffset.X;
            SnappedActorLocation.Y = DesiredCenter.Y - CenterToActorOffset.Y;
        }
    }

    // Ensure snapped position lies on NavMesh; if not, do not move (freeze)
    if (UWorld* World = GetWorld())
    {
        if (UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World))
        {
            FNavLocation NavLoc;
            if (NavSys->ProjectPointToNavigation(SnappedActorLocation, NavLoc, FVector(100.f,100.f,300.f)))
            {
                // Use projected XY from navmesh
                SnappedActorLocation.X = NavLoc.Location.X;
                SnappedActorLocation.Y = NavLoc.Location.Y;
            }
            else
            {
                return; // final snapped spot not on navmesh
            }
        }
    }

    const FVector GroundedSnap = ComputeGroundedLocation(DraggedActor, SnappedActorLocation);

    // ---- Validate collision BEFORE committing placement ----
    // Prepare arrays for overlap query
    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(DraggedActor);
    ActorsToIgnore.Add(OtherActor);

    TArray<AActor*> OverlappingActors;
    bool bWouldOverlap = false;

    // Perform box overlap at the prospective snapped position using the dragged actor's half-extents
    {
        // Inflate extents by SnapGap so we respect the required clearance in XY
        const FVector InflatedExtent(DraggedExtent.X + SnapGap, DraggedExtent.Y + SnapGap, DraggedExtent.Z);
        // Build explicit object type filters (WorldStatic, WorldDynamic, PhysicsBody, Pawn)
        TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
        ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_WorldStatic));
        ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_WorldDynamic));
        ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_PhysicsBody));
        ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_Pawn));

        // Test around the prospective mesh center (actor location + center offset)
        const FVector ProspectiveCenter = GroundedSnap + CenterToActorOffset;

        bool bSuccess = UKismetSystemLibrary::BoxOverlapActors(
            GetWorld(),
            ProspectiveCenter,           // Test at the final grounded mesh center position
            InflatedExtent,              // Half-extent (X, Y, Z) + gap to ensure clearance
            ObjectTypes,
            AActor::StaticClass(),
            ActorsToIgnore,
            OverlappingActors
        );

        if (bSuccess)
        {
            for (AActor* Overlapped : OverlappingActors)
            {
                if (!Overlapped || Overlapped == DraggedActor || Overlapped == OtherActor || Overlapped->IsA(ALandscape::StaticClass()))
                {
                    continue;
                }
                if (Cast<AWorkArea>(Overlapped) || Cast<ABuildingBase>(Overlapped))
                {
                    bWouldOverlap = true;
                    break;
                }
            }
        }
    }

    if (!bWouldOverlap)
    {
        // Safe to place: commit the snapped location
        DraggedActor->SetActorLocation(GroundedSnap);
        WorkAreaIsSnapped = true;

        const bool bWasDifferentTarget = (CurrentSnapActor != OtherActor);
        CurrentSnapActor = OtherActor;
        if (bWasDifferentTarget)
        {
            const float NowLocal = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f);
            NextAllowedSnapTime = NowLocal + FMath::Max(0.f, SnapCooldownSeconds);
        }
    }
    else
    {
        // Invalid snap: immediately unsnap and move to a free, grounded cursor position
        WorkAreaIsSnapped = false;
        if (CurrentSnapActor == OtherActor)
        {
            CurrentSnapActor = nullptr;
        }

        // Follow the cursor without snapping so we never leave the actor overlapping this frame
        FVector FreeDesired = FVector(Ref.X, Ref.Y, DraggedActor->GetActorLocation().Z + DraggedAreaZOffset);
        const FVector FreeGrounded = ComputeGroundedLocation(DraggedActor, FreeDesired);
        DraggedActor->SetActorLocation(FreeGrounded);
    }
}


// ---------------- Simplified helper implementations for MoveWorkArea_Local ----------------

bool AExtendedControllerBase::TraceMouseToGround(FVector& OutMouseGround, FHitResult& OutHit) const
{
    OutMouseGround = FVector::ZeroVector;

    FVector MousePosition, MouseDirection;
    if (!DeprojectMousePositionToWorld(MousePosition, MouseDirection))
    {
        return false;
    }

    const FVector Start = MousePosition;
    const FVector End = Start + MouseDirection * 1000000.f;

    FCollisionQueryParams Params(SCENE_QUERY_STAT(MoveWorkAreaTrace), true);
    Params.bTraceComplex = true;

    // Ignore all actors of class AWorkArea
    for (TActorIterator<AWorkArea> It(GetWorld()); It; ++It)
    {
        Params.AddIgnoredActor(*It);
    }

    if (!GetWorld()->LineTraceSingleByChannel(OutHit, Start, End, ECC_Visibility, Params))
    {
        return false;
    }

    if (UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld()))
    {
        FNavLocation NavLoc;
        if (NavSys->ProjectPointToNavigation(OutHit.Location, NavLoc, FVector(100.f, 100.f, 300.f)))
        {
            OutMouseGround = NavLoc.Location;
            return true;
        }
    }

    return false;
}

bool AExtendedControllerBase::MaintainOrReleaseCurrentSnap(AWorkArea* DraggedWorkArea, const FVector& MouseGround, bool bHit)
{
    if (!CurrentSnapActor)
    {
        return false; // nothing to maintain
    }

    if (!IsValid(CurrentSnapActor))
    {
        CurrentSnapActor = nullptr;
        WorkAreaIsSnapped = false;
        return false;
    }

    if (!bHit)
    {
        return false;
    }

    FVector OtherCenter, OtherExtent;
    if (!GetActorBoundsForSnap(CurrentSnapActor, OtherCenter, OtherExtent))
    {
        CurrentSnapActor = nullptr;
        WorkAreaIsSnapped = false;
        return false;
    }

    const UStaticMeshComponent* DragMesh = DraggedWorkArea ? DraggedWorkArea->Mesh : nullptr;
    const FBoxSphereBounds DragBounds = DragMesh ? DragMesh->CalcBounds(DragMesh->GetComponentTransform()) : FBoxSphereBounds();
    const FVector DragExtent = DragBounds.BoxExtent;

    const float DragXY = FMath::Max(DragExtent.X, DragExtent.Y);
    const float OtherXY = FMath::Max(OtherExtent.X, OtherExtent.Y);
    const float ReleaseThreshold = DragXY + OtherXY + 150.f;

    const float MouseToSnapTarget = FVector::Dist2D(MouseGround, OtherCenter);
    const float NowTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

    if (MouseToSnapTarget > ReleaseThreshold)
    {
        if (LastBeyondReleaseTime < 0.f)
        {
            LastBeyondReleaseTime = NowTime;
        }
        if ((NowTime - LastBeyondReleaseTime) >= UnsnapGraceSeconds)
        {
            CurrentSnapActor = nullptr;
            WorkAreaIsSnapped = false;
            LastBeyondReleaseTime = -1.f;
        }
        return false;
    }

    LastBeyondReleaseTime = -1.f;
    SnapToActor(DraggedWorkArea, CurrentSnapActor, nullptr);
    return true; // keep snapping this frame
}

bool AExtendedControllerBase::TrySnapViaOverlap(AWorkArea* DraggedWorkArea, const FVector& MouseGround, const FHitResult& HitResult)
{
    if (!DraggedWorkArea || !DraggedWorkArea->Mesh)
    {
        return false;
    }

    const FBoxSphereBounds DraggedBounds = DraggedWorkArea->Mesh->CalcBounds(DraggedWorkArea->Mesh->GetComponentTransform());
    const FVector Extent = DraggedBounds.BoxExtent;

    const float DraggedBoxExtentSize = Extent.Size() * 2.f;

    const float Distance = FVector::Dist(MouseGround, DraggedWorkArea->GetActorLocation());
    if (Distance >= (SnapDistance + SnapGap + DraggedBoxExtentSize))
    {
        return false;
    }

    const FBoxSphereBounds MeshBounds = DraggedWorkArea->Mesh->CalcBounds(DraggedWorkArea->Mesh->GetComponentTransform());
    const float OverlapRadius = MeshBounds.SphereRadius;

    TArray<AActor*> OverlappedActors;
    const bool bAnyOverlap = UKismetSystemLibrary::SphereOverlapActors(
        this,
        HitResult.Location,
        OverlapRadius,
        TArray<TEnumAsByte<EObjectTypeQuery>>(),
        AActor::StaticClass(),
        TArray<AActor*>(),
        OverlappedActors);

    if (!bAnyOverlap || OverlappedActors.Num() == 0)
    {
        return false;
    }

    for (AActor* OverlappedActor : OverlappedActors)
    {
        if (!OverlappedActor || OverlappedActor == DraggedWorkArea)
        {
            continue;
        }

        AWorkArea* OverlappedWorkArea = Cast<AWorkArea>(OverlappedActor);
        ABuildingBase* OverlappedBuilding = Cast<ABuildingBase>(OverlappedActor);
        if (!(OverlappedWorkArea || OverlappedBuilding))
        {
            continue;
        }

        // resource/no-build checks
        if (OverlappedWorkArea)
        {
            if (OverlappedWorkArea->IsNoBuildZone)
            {
                DraggedWorkArea->TemporarilyChangeMaterial();
                break;
            }
            if (OverlappedWorkArea->Type == WorkAreaData::Primary ||
                OverlappedWorkArea->Type == WorkAreaData::Secondary ||
                OverlappedWorkArea->Type == WorkAreaData::Tertiary ||
                OverlappedWorkArea->Type == WorkAreaData::Rare ||
                OverlappedWorkArea->Type == WorkAreaData::Epic ||
                OverlappedWorkArea->Type == WorkAreaData::Legendary)
            {
                break;
            }
        }

        FVector OtherCenter, OtherExtent;
        if (!GetActorBoundsForSnap(OverlappedActor, OtherCenter, OtherExtent))
        {
            continue;
        }

        const float DragXY = FMath::Max(Extent.X, Extent.Y);
        const float OtherXY = FMath::Max(OtherExtent.X, OtherExtent.Y);
        const float AcquireThreshold = (DragXY + OtherXY + 150.f) * FMath::Clamp(AcquireHysteresisFactor, 0.1f, 1.0f);
        const float MouseToCandidate = FVector::Dist2D(MouseGround, OtherCenter);
        if (MouseToCandidate > AcquireThreshold)
        {
            continue;
        }

        const float XYDistance = FVector::Dist2D(DraggedWorkArea->GetActorLocation(), OverlappedActor->GetActorLocation());
        const float CombinedXYExtent = (Extent.X + OtherExtent.X + Extent.Y + OtherExtent.Y) * 0.5f;
        const float SnapThreshold = SnapDistance + SnapGap + CombinedXYExtent;
        if (XYDistance < SnapThreshold)
        {
            UStaticMeshComponent* OverlappedMesh = OverlappedWorkArea ? OverlappedWorkArea->Mesh : nullptr;
            SnapToActor(DraggedWorkArea, OverlappedActor, OverlappedMesh);
            return true;
        }
    }

    return false;
}

bool AExtendedControllerBase::TrySnapViaProximity(AWorkArea* DraggedWorkArea, const FVector& MouseGround)
{
    if (!DraggedWorkArea || !DraggedWorkArea->Mesh)
    {
        return false;
    }

    const FBoxSphereBounds DraggedBounds = DraggedWorkArea->Mesh->CalcBounds(DraggedWorkArea->Mesh->GetComponentTransform());
    const FVector Extent = DraggedBounds.BoxExtent;

    AActor* BestCandidate = nullptr;
    UStaticMeshComponent* BestCandidateMesh = nullptr;
    float BestCandidateDist = TNumericLimits<float>::Max();

    // Scan WorkAreas
    for (TActorIterator<AWorkArea> It(GetWorld()); It; ++It)
    {
        AWorkArea* Candidate = *It;
        if (!Candidate || Candidate == DraggedWorkArea) continue;

        if (Candidate->IsNoBuildZone)
        {
            DraggedWorkArea->TemporarilyChangeMaterial();
            continue;
        }
        if (Candidate->Type == WorkAreaData::Primary ||
            Candidate->Type == WorkAreaData::Secondary ||
            Candidate->Type == WorkAreaData::Tertiary ||
            Candidate->Type == WorkAreaData::Rare ||
            Candidate->Type == WorkAreaData::Epic ||
            Candidate->Type == WorkAreaData::Legendary)
        {
            continue;
        }

        FVector OtherCenter, OtherExtent;
        if (!GetActorBoundsForSnap(Candidate, OtherCenter, OtherExtent))
            continue;

        const float XYDistance = FVector::Dist2D(DraggedWorkArea->GetActorLocation(), Candidate->GetActorLocation());
        const float CombinedXYExtent = (Extent.X + OtherExtent.X + Extent.Y + OtherExtent.Y) * 0.5f;
        const float SnapThreshold = SnapDistance + SnapGap + CombinedXYExtent;
        if (XYDistance < SnapThreshold && XYDistance < BestCandidateDist)
        {
            BestCandidate = Candidate;
            BestCandidateMesh = Candidate->Mesh;
            BestCandidateDist = XYDistance;
        }
    }

    // Scan Buildings
    for (TActorIterator<ABuildingBase> ItB(GetWorld()); ItB; ++ItB)
    {
        ABuildingBase* Candidate = *ItB;
        if (!Candidate) continue;

        FVector OtherCenter, OtherExtent;
        if (!GetActorBoundsForSnap(Candidate, OtherCenter, OtherExtent))
            continue;

        const float XYDistance = FVector::Dist2D(DraggedWorkArea->GetActorLocation(), Candidate->GetActorLocation());
        const float CombinedXYExtent = (Extent.X + OtherExtent.X + Extent.Y + OtherExtent.Y) * 0.5f;
        const float SnapThreshold = SnapDistance + SnapGap + CombinedXYExtent;
        if (XYDistance < SnapThreshold && XYDistance < BestCandidateDist)
        {
            BestCandidate = Candidate;
            BestCandidateMesh = nullptr;
            BestCandidateDist = XYDistance;
        }
    }

    if (BestCandidate)
    {
        SnapToActor(DraggedWorkArea, BestCandidate, BestCandidateMesh);
        return true;
    }

    return false;
}

void AExtendedControllerBase::MoveDraggedAreaFreely(AWorkArea* DraggedWorkArea, const FVector& MouseGround, const FHitResult& HitResult)
{
    if (!DraggedWorkArea)
    {
        return;
    }

    CurrentDraggedGround = HitResult.GetActor();

    FVector NewActorPosition = MouseGround;
    NewActorPosition.Z += DraggedAreaZOffset;

    FVector GroundedPos = ComputeGroundedLocation(DraggedWorkArea, NewActorPosition);

    if (UStaticMeshComponent* FreeMesh = DraggedWorkArea->Mesh)
    {
        const FBoxSphereBounds DragBounds = FreeMesh->CalcBounds(FreeMesh->GetComponentTransform());
        const FVector DragExtent = DragBounds.BoxExtent;
        const FVector DragCenter = DragBounds.Origin;
        const FVector CenterToActorOffset = DragCenter - DraggedWorkArea->GetActorLocation();

        TArray<AActor*> Ignored; Ignored.Add(DraggedWorkArea);
        TArray<AActor*> Hits;

        const FVector InflatedExtent(DragExtent.X + SnapGap, DragExtent.Y + SnapGap, DragExtent.Z);

        TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
        ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_WorldStatic));
        ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_WorldDynamic));
        ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_PhysicsBody));
        ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_Pawn));

        const FVector TestCenter = FVector(GroundedPos.X + CenterToActorOffset.X, GroundedPos.Y + CenterToActorOffset.Y, GroundedPos.Z + CenterToActorOffset.Z);

        const bool bOverlap = UKismetSystemLibrary::BoxOverlapActors(
            GetWorld(), TestCenter, InflatedExtent, ObjectTypes, AActor::StaticClass(), Ignored, Hits);

        if (bOverlap)
        {
            for (AActor* A : Hits)
            {
                if (!A || A == DraggedWorkArea) continue;
                if (A->IsA(ALandscape::StaticClass())) continue;
                if (Cast<AWorkArea>(A) || Cast<ABuildingBase>(A))
                {
                    GroundedPos = DraggedWorkArea->GetActorLocation();
                    break;
                }
            }
        }
    }

    DraggedWorkArea->SetActorLocation(GroundedPos);
    WorkAreaIsSnapped = false;
}

bool AExtendedControllerBase::MoveWorkArea_Local_Simplified(float DeltaSeconds)
{
    // Distance-only implementation for dragging WorkArea; no overlap checks
    if (SelectedUnits.Num() == 0 || !SelectedUnits[0]) return true; // treat as handled
    AWorkArea* DraggedWorkArea = SelectedUnits[0]->CurrentDraggedWorkArea;
    if (!DraggedWorkArea) return true;

    SelectedUnits[0]->ShowWorkAreaIfNoFog_Implementation(DraggedWorkArea);

    FVector MouseGround; FHitResult HitResult;
    if (!TraceMouseToGround(MouseGround, HitResult)) return true;

    // Compute desired grounded location for the dragged area
    const FVector DesiredGrounded = ComputeGroundedLocation(DraggedWorkArea, MouseGround);

    // Beacon visual feedback: if this WorkArea needs a beacon and we are outside any beacon range, flash material
    if (DraggedWorkArea->NeedsBeacon)
    {
        UWorld* WorldCtx = GetWorld();
        if (WorldCtx && !ABuildingBase::IsLocationInBeaconRange(WorldCtx, DesiredGrounded))
        {
            DraggedWorkArea->TemporarilyChangeMaterial();
        }
    }

    // Persist last safe location per dragged area (function-local static state to avoid header changes)
    static TWeakObjectPtr<AWorkArea> LastAreaRef = nullptr;
    static FVector LastSafeLoc = FVector::ZeroVector;
    static bool bHasLastSafe = false;
    if (LastAreaRef.Get() != DraggedWorkArea)
    {
        LastAreaRef = DraggedWorkArea;
        bHasLastSafe = false;
    }

    // Get dragged extents (XY) using its mesh or bounds
    FVector DragCenter, DragExtent;
    if (!GetActorBoundsForSnap(DraggedWorkArea, DragCenter, DragExtent))
    {
        // Fallback: just place at desired grounded
        DraggedWorkArea->SetActorLocation(DesiredGrounded);
        WorkAreaIsSnapped = false;
        CurrentSnapActor = nullptr;
        LastSafeLoc = DesiredGrounded; bHasLastSafe = true;
        return true;
    }
    const float DragXY = FMath::Max(DragExtent.X, DragExtent.Y);

    // Scan close candidates: other WorkAreas and Buildings
    AActor* BestCandidate = nullptr;
    float BestDist = TNumericLimits<float>::Max();
    FVector BestOtherCenter(0,0,0); FVector BestOtherExtent(0,0,0);

    UWorld* World = GetWorld();
    if (!World)
    {
        DraggedWorkArea->SetActorLocation(DesiredGrounded);
        WorkAreaIsSnapped = false; CurrentSnapActor = nullptr;
        LastSafeLoc = DesiredGrounded; bHasLastSafe = true;
        return true;
    }

    auto ConsiderActor = [&](AActor* Candidate)
    {
        if (!Candidate || Candidate == DraggedWorkArea) return;
        FVector OtherCenter, OtherExtent;
        if (!GetActorBoundsForSnap(Candidate, OtherCenter, OtherExtent)) return;
        const float OtherXY = FMath::Max(OtherExtent.X, OtherExtent.Y);
        const float Threshold = DragXY + OtherXY; // no extra overlap margin here
        const float Dist2D = FVector::Dist2D(DesiredGrounded, OtherCenter);
        if (Dist2D < Threshold && Dist2D < BestDist)
        {
            BestDist = Dist2D;
            BestCandidate = Candidate;
            BestOtherCenter = OtherCenter; BestOtherExtent = OtherExtent;
        }
    };

    for (TActorIterator<AWorkArea> It(World); It; ++It)
    {
        ConsiderActor(*It);
    }
    for (TActorIterator<ABuildingBase> ItB(World); ItB; ++ItB)
    {
        ConsiderActor(*ItB);
    }

    // Helper: iterative push-away to satisfy distances vs all neighbors
    auto ResolveDistances = [&](FVector& InOutLocation, bool& bOutAllGood)
    {
        const int32 MaxIterations = 12;
        const float Padding = 1.0f; // small gap to avoid re-trigger
        const float MinStep = 0.1f;
        bOutAllGood = false;

        // Build neighbor list once
        TArray<AActor*> Neighbors;
        for (TActorIterator<AWorkArea> ItWA(World); ItWA; ++ItWA)
        {
            if (*ItWA && *ItWA != DraggedWorkArea) Neighbors.Add(*ItWA);
        }
        for (TActorIterator<ABuildingBase> ItBD(World); ItBD; ++ItBD)
        {
            if (*ItBD) Neighbors.Add(*ItBD);
        }

        FVector WorkingLoc = InOutLocation;
        bool bSolved = false;
        for (int32 Iter = 0; Iter < MaxIterations; ++Iter)
        {
            // Place tentatively at working location
            DraggedWorkArea->SetActorLocation(WorkingLoc);

            FVector DragC, DragE;
            if (!GetActorBoundsForSnap(DraggedWorkArea, DragC, DragE))
            {
                break; // cannot evaluate; treat as failure
            }
            const float DragR = FMath::Max(DragE.X, DragE.Y);

            bool bAnyViolation = false;
            FVector AccumulatedPush = FVector::ZeroVector;

            for (AActor* N : Neighbors)
            {
                if (!N) continue;
                FVector NC, NE;
                if (!GetActorBoundsForSnap(N, NC, NE)) continue;
                const float NR = FMath::Max(NE.X, NE.Y);
                float Required = DragR + NR + SnapGap;
                // Enforce resource distance if enabled on dragged WorkArea and neighbor is a resource WorkArea
                if (DraggedWorkArea->DenyPlacementCloseToResources)
                {
                    if (AWorkArea* NeighborWA = Cast<AWorkArea>(N))
                    {
                        const WorkAreaData::WorkAreaType T = NeighborWA->Type;
                        const bool bIsResourceType = (T == WorkAreaData::Primary || T == WorkAreaData::Secondary || T == WorkAreaData::Tertiary || T == WorkAreaData::Rare || T == WorkAreaData::Epic || T == WorkAreaData::Legendary);
                        if (bIsResourceType)
                        {
                            Required = FMath::Max(Required, DraggedWorkArea->ResourcePlacementDistance);
                        }
                    }
                }
                const float D = FVector::Dist2D(DragC, NC);
                if (D < Required)
                {
                    bAnyViolation = true;
                    FVector Dir = (DragC - NC);
                    Dir.Z = 0.f;
                    if (Dir.IsNearlyZero(1e-3f))
                    {
                        Dir = FVector(1.f, 0.f, 0.f);
                    }
                    Dir.Normalize();
                    const float Push = (Required - D) + Padding;
                    AccumulatedPush += Dir * Push;
                }
            }

            if (!bAnyViolation)
            {
                bSolved = true;
                break;
            }

            if (AccumulatedPush.Size2D() < MinStep)
            {
                // no meaningful progress possible
                break;
            }

            WorkingLoc += FVector(AccumulatedPush.X, AccumulatedPush.Y, 0.f);
        }

        // Final placement for evaluation result
        DraggedWorkArea->SetActorLocation(WorkingLoc);
        // Confirm no violations remain
        if (!bSolved)
        {
            // one last check
            FVector DragC2, DragE2;
            if (GetActorBoundsForSnap(DraggedWorkArea, DragC2, DragE2))
            {
                const float DragR2 = FMath::Max(DragE2.X, DragE2.Y);
                bool bStillBad = false;
                for (AActor* N : Neighbors)
                {
                    if (!N) continue;
                    FVector NC, NE;
                    if (!GetActorBoundsForSnap(N, NC, NE)) continue;
                    const float NR = FMath::Max(NE.X, NE.Y);
                    float Required = DragR2 + NR + SnapGap;
                    if (DraggedWorkArea->DenyPlacementCloseToResources)
                    {
                        if (AWorkArea* NeighborWA = Cast<AWorkArea>(N))
                        {
                            const WorkAreaData::WorkAreaType T = NeighborWA->Type;
                            const bool bIsResourceType = (T == WorkAreaData::Primary || T == WorkAreaData::Secondary || T == WorkAreaData::Tertiary || T == WorkAreaData::Rare || T == WorkAreaData::Epic || T == WorkAreaData::Legendary);
                            if (bIsResourceType)
                            {
                                Required = FMath::Max(Required, DraggedWorkArea->ResourcePlacementDistance);
                            }
                        }
                    }
                    const float D = FVector::Dist2D(DragC2, NC);
                    if (D < Required)
                    {
                        bStillBad = true; break;
                    }
                }
                if (!bStillBad)
                {
                    bSolved = true;
                }
            }
        }

        bOutAllGood = bSolved;
        InOutLocation = WorkingLoc;
    };

    if (BestCandidate)
    {
        // Try to snap to the best candidate
        AActor* PrevSnap = CurrentSnapActor;
        CurrentSnapActor = BestCandidate;
        SnapToActor(DraggedWorkArea, BestCandidate, nullptr);
        WorkAreaIsSnapped = true;

        // After snapping, iteratively resolve distances to all neighbors
        bool bAllGood = false;
        FVector StartLoc = DraggedWorkArea->GetActorLocation();
        ResolveDistances(StartLoc, bAllGood);
        if (!bAllGood)
        {
            // Revert if we can
            if (bHasLastSafe)
            {
                DraggedWorkArea->SetActorLocation(LastSafeLoc);
            }
            WorkAreaIsSnapped = false;
            CurrentSnapActor = nullptr;
        }
        else
        {
            // Accept final resolved position and update last safe
            DraggedWorkArea->SetActorLocation(StartLoc);
            LastSafeLoc = StartLoc;
            bHasLastSafe = true;
        }

        return true;
    }

    // No candidate close -> free move, then resolve against neighbors
    {
        FVector Proposed = DesiredGrounded;
        DraggedWorkArea->SetActorLocation(Proposed);
        bool bAllGood = false;
        ResolveDistances(Proposed, bAllGood);
        if (!bAllGood)
        {
            if (bHasLastSafe)
            {
                DraggedWorkArea->SetActorLocation(LastSafeLoc);
            }
        }
        else
        {
            DraggedWorkArea->SetActorLocation(Proposed);
            LastSafeLoc = Proposed; bHasLastSafe = true;
        }
    }

    WorkAreaIsSnapped = false;
    CurrentSnapActor = nullptr;
    return true;
}

// ------------------------------------------------------------------------------------------

void AExtendedControllerBase::MoveWorkArea_Local(float DeltaSeconds)
{
    // Ensure we have a unit and a dragged work area
    if (SelectedUnits.Num() == 0 || !SelectedUnits[0])
    {
        return;
    }
    AWorkArea* DraggedWorkArea = SelectedUnits[0]->CurrentDraggedWorkArea;
    if (!DraggedWorkArea)
    {
        return;
    }

    // Special handling for Extension Areas: mimic UExtensionAbility::UpdatePreviewFollow
    if (DraggedWorkArea->IsExtensionArea)
    {
        ABuildingBase* Unit = Cast<ABuildingBase>(SelectedUnits[0]);
    	Unit->ShowWorkAreaIfNoFog_Implementation(DraggedWorkArea);
        // Determine mouse world hit
        FHitResult Hit;
        if (!GetHitResultUnderCursor(ECC_Visibility, false, Hit))
        {
            return;
        }

        const FVector UnitLoc = Unit ? Unit->GetMassActorLocation() : FVector::ZeroVector;
        FVector TargetLoc = UnitLoc;
        // Compute building bounds extents to derive offset from actual size (ISM or components)
        FVector UnitCenterBounds(0.f, 0.f, 0.f), UnitExtentBounds(100.f, 100.f, 100.f);
        GetActorBoundsForSnap(Unit, UnitCenterBounds, UnitExtentBounds);

   		if (Unit->ExtensionDominantSideSelection)
   		{
   			// Axis-dominant side selection relative to mouse position
   			const FVector2D Delta2D(Hit.Location.X - UnitLoc.X, Hit.Location.Y - UnitLoc.Y);

   			
   			const float AbsX = Unit->ExtensionOffset.X + UnitExtentBounds.X; // half-size X from actor bounds
   			const float AbsY = Unit->ExtensionOffset.Y + UnitExtentBounds.Y;  // half-size Y from actor bounds

   			float DesiredYaw = DraggedWorkArea->GetActorRotation().Yaw;
   			if (FMath::Abs(Delta2D.X) >= FMath::Abs(Delta2D.Y))
   			{
   				const float SignX = (Delta2D.X >= 0.f) ? 1.f : -1.f;
   				TargetLoc.X += SignX * AbsX;
   				DesiredYaw = (SignX > 0.f) ? 0.f : 180.f; // Face +X or -X
   			}
   			else
   			{
   				const float SignY = (Delta2D.Y >= 0.f) ? 1.f : -1.f;
   				TargetLoc.Y += SignY * AbsY;
   				DesiredYaw = (SignY > 0.f) ? 90.f : 270.f; // Face +Y or -Y
   			}

   			// Apply discrete rotation to the preview and cache it on the WorkArea for server-side spawns
   			const FRotator NewRot(0.f, DesiredYaw, 0.f);
   			DraggedWorkArea->SetActorRotation(NewRot);
   			DraggedWorkArea->ServerMeshRotationBuilding = NewRot;
   		}else
   		{
   			TargetLoc.X += Unit->ExtensionOffset.X;
   			TargetLoc.Y += Unit->ExtensionOffset.Y;
   		}

   		if (Unit->ExtensionGroundTrace)
   		{
   			// Ground align: trace down while ignoring the unit, the dragged area, and any WorkAreas/Buildings under the cursor
   			const FVector TraceStart = TargetLoc + FVector(0, 0, 2000.f);
   			const FVector TraceEnd   = TargetLoc - FVector(0, 0, 2000.f);
   			FCollisionQueryParams Params(SCENE_QUERY_STAT(MoveExtensionAreaGround), /*bTraceComplex*/ true);
   			if (Unit) Params.AddIgnoredActor(Unit);
   			Params.AddIgnoredActor(DraggedWorkArea);
   			bool bFoundValidGround = false;
   			FHitResult GroundHit;
   			// Try multiple times, each time ignoring AWorkArea/ABuildingBase hits so we only align to real ground
   			const int32 MaxTries = 8;
   			for (int32 Try = 0; Try < MaxTries; ++Try)
   			{
   				if (!GetWorld()->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility, Params))
   				{
   					break; // nothing hit
   				}
   				AActor* HitActor = GroundHit.GetActor();
   				if (HitActor && (HitActor->IsA(AWorkArea::StaticClass()) || HitActor->IsA(ABuildingBase::StaticClass())))
   				{
   					Params.AddIgnoredActor(HitActor);
   					continue; // ignore and try again
   				}
   				bFoundValidGround = true;
   				break;
   			}
   			if (bFoundValidGround)
   			{
   				TargetLoc.Z = GroundHit.Location.Z;
   			}
   		}else
    	{
   			TargetLoc.Z += Unit->ExtensionOffset.Z + UnitExtentBounds.Z;
    	}

        // Adjust Z so the mesh bottom sits exactly on the ground plus a small clearance
        if (UStaticMeshComponent* MeshComp = DraggedWorkArea->FindComponentByClass<UStaticMeshComponent>())
        {
            const FBoxSphereBounds Bounds = MeshComp->CalcBounds(MeshComp->GetComponentTransform());
            const float BottomZ = Bounds.Origin.Z - Bounds.BoxExtent.Z;
            const float CurrentActorZ = DraggedWorkArea->GetActorLocation().Z;
            const float Clearance = 2.f;
            const float NewZ = CurrentActorZ + ((TargetLoc.Z + Clearance) - BottomZ);
            TargetLoc.Z = NewZ;
        }

        // Overlap check at the prospective TargetLoc: if overlapping other WorkAreas or Buildings (excluding the initiating Unit), flash material
        {
            UStaticMeshComponent* MeshComp2 = DraggedWorkArea->FindComponentByClass<UStaticMeshComponent>();
            if (MeshComp2)
            {
                const FTransform PreviewTM(DraggedWorkArea->GetActorRotation(), TargetLoc, DraggedWorkArea->GetActorScale3D());
                const FBoxSphereBounds PreviewBounds = MeshComp2->CalcBounds(PreviewTM);
                const FVector OverlapCenter = PreviewBounds.Origin;
                const float OverlapRadius = PreviewBounds.SphereRadius;

                TArray<AActor*> ActorsToIgnore;
                ActorsToIgnore.Add(DraggedWorkArea);
                if (Unit) { ActorsToIgnore.Add(Unit); }

                // Limit to relevant object types: WorldStatic (WorkAreas), WorldDynamic, and Pawn (Buildings)
                TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
                ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));
                ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));
                ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));

                TArray<AActor*> OverlappedActors;
                const bool bAnyOverlap = UKismetSystemLibrary::SphereOverlapActors(
                    this,
                    OverlapCenter,
                    OverlapRadius,
                    ObjectTypes,
                    AActor::StaticClass(),
                    ActorsToIgnore,
                    OverlappedActors
                );

                if (bAnyOverlap)
                {
                    for (AActor* Overlapped : OverlappedActors)
                    {
                        if (!Overlapped) { continue; }
                        if (Overlapped == DraggedWorkArea) { continue; }
                        if (Unit && Overlapped == Unit) { continue; }

                        if (Overlapped->IsA(AWorkArea::StaticClass()) || Overlapped->IsA(ABuildingBase::StaticClass()))
                        {
                            DraggedWorkArea->TemporarilyChangeMaterial();
                            break;
                        }
                    }
                }
            }
        }

        DraggedWorkArea->SetActorLocation(TargetLoc);

		if (DraggedWorkArea->InstantDrop)
		{
			Server_DropWorkAreaForUnit(Unit, true, DropWorkAreaFailedSound, DraggedWorkArea->GetActorTransform());
		}
    	
        return;
    }


    // Non-extension areas: use simplified path first
    if (MoveWorkArea_Local_Simplified(DeltaSeconds))
    {
        return;
    }

    SelectedUnits[0]->ShowWorkAreaIfNoFog_Implementation(DraggedWorkArea);
	
    FVector MousePosition, MouseDirection;
    DeprojectMousePositionToWorld(MousePosition, MouseDirection);

    // Raycast from the mouse into the scene
    FVector Start = MousePosition;
    FVector End   = Start + MouseDirection * 1000000.f;

    FHitResult HitResult;
    FCollisionQueryParams CollisionParams;
    CollisionParams.bTraceComplex = true;
    //CollisionParams.AddIgnoredActor(DraggedWorkArea);

	// Ignore all actors of class AWorkArea
	for (TActorIterator<AWorkArea> It(GetWorld()); It; ++It)
	{
		CollisionParams.AddIgnoredActor(*It);
	}
	
	bool bHit = GetWorld()->LineTraceSingleByChannel(
		HitResult,
		Start,
		End,
		ECC_Visibility,
		CollisionParams
	);

	// Compute distance from the drag’s current location to the raycast hit, constrained to NavMesh
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
	if (!bHit || !NavSys)
	{
		return; // no valid ground hit or nav system; freeze movement
	}
	FNavLocation NavLoc;
	const bool bOnNav = NavSys->ProjectPointToNavigation(HitResult.Location, NavLoc, FVector(100.f,100.f,300.f));
	if (!bOnNav)
	{
		return; // Mouse not over NavMesh: stop moving until it is
	}
	const FVector MouseGround = NavLoc.Location;
	// Beacon visual feedback in legacy path
	if (DraggedWorkArea->NeedsBeacon)
	{
		UWorld* WorldCtx = GetWorld();
		if (WorldCtx)
		{
			const FVector DesiredGrounded = ComputeGroundedLocation(DraggedWorkArea, MouseGround);
			if (!ABuildingBase::IsLocationInBeaconRange(WorldCtx, DesiredGrounded))
			{
				DraggedWorkArea->TemporarilyChangeMaterial();
			}
		}
	}
	float Distance = FVector::Dist(
		MouseGround,
		DraggedWorkArea->GetActorLocation()
	);

	// If currently snapped, keep updating snap while mouse stays within release distance; otherwise release
	if (CurrentSnapActor)
	{
		if (!IsValid(CurrentSnapActor))
		{
			CurrentSnapActor = nullptr;
			WorkAreaIsSnapped = false;
		}
  else if (bHit)
  {
      // Dynamic release threshold: half-extent(Dragged) + half-extent(Target) + 150, using MOUSE->TARGET distance
      FVector OtherCenter, OtherExtent;
      if (GetActorBoundsForSnap(CurrentSnapActor, OtherCenter, OtherExtent))
      {
          UStaticMeshComponent* DragMeshLocal = DraggedWorkArea->Mesh;
          const FBoxSphereBounds DragBoundsLocal = DragMeshLocal ? DragMeshLocal->CalcBounds(DragMeshLocal->GetComponentTransform()) : FBoxSphereBounds();
          const FVector DragExtentLocal = DragBoundsLocal.BoxExtent;

          const float DragXY = FMath::Max(DragExtentLocal.X, DragExtentLocal.Y);
          const float OtherXY = FMath::Max(OtherExtent.X, OtherExtent.Y);
          const float ReleaseThreshold = DragXY + OtherXY + 150.f;

          const float MouseToSnapTarget = FVector::Dist2D(MouseGround, OtherCenter);
          const float NowTime = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f);
          if (MouseToSnapTarget > ReleaseThreshold)
          {
              if (LastBeyondReleaseTime < 0.f)
              {
                  LastBeyondReleaseTime = NowTime;
              }
              if ((NowTime - LastBeyondReleaseTime) >= UnsnapGraceSeconds)
              {
                  CurrentSnapActor = nullptr;
                  WorkAreaIsSnapped = false;
                  LastBeyondReleaseTime = -1.f;
              }
          }
          else
          {
              LastBeyondReleaseTime = -1.f;
              SnapToActor(DraggedWorkArea, CurrentSnapActor, nullptr);
              return;
          }
      }
      else
      {
          CurrentSnapActor = nullptr;
          WorkAreaIsSnapped = false;
      }
  }
	}

    //---------------------------------------
    // Get DraggedWorkArea bounding box info
    //---------------------------------------
	UStaticMeshComponent* DraggedMesh = DraggedWorkArea->Mesh;

	if (!DraggedMesh)
	{
		// If there's no mesh on the dragged work area, bail out or handle gracefully
		return;
	}
	FBoxSphereBounds DraggedBounds = DraggedMesh->CalcBounds(DraggedMesh->GetComponentTransform());
	FVector Extent = DraggedBounds.BoxExtent; // half-size

    // Decide how you want to incorporate the bounding box size into your 
    // check. For example, add the length of the bounding box’s diagonal 
    // to your snap threshold, or just the largest dimension, or X/Y extents, etc.
    // Here, we do a quick example adding the entire box extent size.
    float DraggedBoxExtentSize = Extent.Size() * 2.f; 
    // e.g. can consider just .X/.Y if ignoring Z

    // If the distance < SnapDistance + SnapGap + "some factor of the box size"
	//  + SnapGap + DraggedBoxExtentSize
    if (Distance < (SnapDistance + SnapGap + DraggedBoxExtentSize))
    {
        // Try to overlap to see if we’re near something we can snap to
        if (UStaticMeshComponent* Mesh = DraggedWorkArea->Mesh)
        {
            // Use the sphere radius as an overlap check
            FBoxSphereBounds MeshBounds = Mesh->CalcBounds(Mesh->GetComponentTransform());
            float OverlapRadius = MeshBounds.SphereRadius;

            TArray<AActor*> OverlappedActors;
            bool bAnyOverlap = UKismetSystemLibrary::SphereOverlapActors(
                this,
                HitResult.Location,
                OverlapRadius,
                TArray<TEnumAsByte<EObjectTypeQuery>>(),
                AActor::StaticClass(),
                TArray<AActor*>(),  // Actors to ignore
                OverlappedActors
            );

            if (bAnyOverlap && OverlappedActors.Num() > 0)
            {
                for (AActor* OverlappedActor : OverlappedActors)
                {
                    // Ignore if the overlapped actor is invalid 
                    // or the same as the dragged one
                    if (!OverlappedActor || OverlappedActor == DraggedWorkArea)
                    {
                        continue;
                    }

                    AWorkArea* OverlappedWorkArea = Cast<AWorkArea>(OverlappedActor);
                    ABuildingBase* OverlappedBuilding = Cast<ABuildingBase>(OverlappedActor);
                    if (OverlappedWorkArea || OverlappedBuilding)
                    {
                        // Gate by mouse distance using dynamic release threshold (sum half-XY extents + 150)
                        {
                            FVector TmpCenter, TmpExtent;
                            if (GetActorBoundsForSnap(OverlappedActor, TmpCenter, TmpExtent))
                            {
                                const float DragXY = FMath::Max(Extent.X, Extent.Y);
                                const float OtherXY = FMath::Max(TmpExtent.X, TmpExtent.Y);
                                const float ReleaseThreshold = DragXY + OtherXY + 150.f;
                                const float AcquireThreshold = ReleaseThreshold * FMath::Clamp(AcquireHysteresisFactor, 0.1f, 1.0f);
                                const float MouseToCandidate = FVector::Dist2D(MouseGround, TmpCenter);
                                if (MouseToCandidate > AcquireThreshold)
                                {
                                    continue;
                                }
                            }
                        }
						bool StopLoop = false;
                        UStaticMeshComponent* OverlappedMesh = nullptr;
                        if (OverlappedWorkArea)
                        {
                            OverlappedMesh = OverlappedWorkArea->Mesh;
                            if (OverlappedWorkArea->IsNoBuildZone)
                            {
                                DraggedWorkArea->TemporarilyChangeMaterial();
                                break;
                            }
                            else if (OverlappedWorkArea->Type == WorkAreaData::Primary ||
                                     OverlappedWorkArea->Type == WorkAreaData::Secondary ||
                                     OverlappedWorkArea->Type == WorkAreaData::Tertiary ||
                                     OverlappedWorkArea->Type == WorkAreaData::Rare ||
                                     OverlappedWorkArea->Type == WorkAreaData::Epic ||
                                     OverlappedWorkArea->Type == WorkAreaData::Legendary)
                            {
                                break;
                            }
                        }

                        FVector OtherCenter, OtherExtent;
                        if (!GetActorBoundsForSnap(OverlappedActor, OtherCenter, OtherExtent))
                        {
                            continue;
                        }

                        float XYDistance = FVector::Dist2D(
                            DraggedWorkArea->GetActorLocation(),
                            OverlappedActor->GetActorLocation()
                        );

                        float CombinedXYExtent = (Extent.X + OtherExtent.X +
                                                  Extent.Y + OtherExtent.Y) * 0.5f;

                        float SnapThreshold = SnapDistance + SnapGap + CombinedXYExtent;

                        if (XYDistance < SnapThreshold)
                        {
                            SnapToActor(DraggedWorkArea, OverlappedActor, OverlappedMesh);
                            return;
                        }
                    }
                }
            }
        }
    }
    //---------------------------------
    // If no snap from overlap, try a fallback proximity scan (works with ISMs without overlaps)
    //---------------------------------
    {
        AActor* BestCandidate = nullptr;
        UStaticMeshComponent* BestCandidateMesh = nullptr;
        float BestCandidateDist = TNumericLimits<float>::Max();

        // Scan WorkAreas
        for (TActorIterator<AWorkArea> It(GetWorld()); It; ++It)
        {
            AWorkArea* Candidate = *It;
            if (!Candidate || Candidate == DraggedWorkArea) continue;

            if (Candidate->IsNoBuildZone)
            {
                // Visual feedback only; don't snap to no-build zones
                DraggedWorkArea->TemporarilyChangeMaterial();
                continue;
            }
            if (Candidate->Type == WorkAreaData::Primary ||
                Candidate->Type == WorkAreaData::Secondary ||
                Candidate->Type == WorkAreaData::Tertiary ||
                Candidate->Type == WorkAreaData::Rare ||
                Candidate->Type == WorkAreaData::Epic ||
                Candidate->Type == WorkAreaData::Legendary)
            {
                continue; // resources are not snap targets
            }

            FVector OtherCenter, OtherExtent;
            if (!GetActorBoundsForSnap(Candidate, OtherCenter, OtherExtent))
                continue;

            const float XYDistance = FVector::Dist2D(DraggedWorkArea->GetActorLocation(), Candidate->GetActorLocation());
            const float CombinedXYExtent = (Extent.X + OtherExtent.X + Extent.Y + OtherExtent.Y) * 0.5f;
            const float SnapThreshold = SnapDistance + SnapGap + CombinedXYExtent;
            if (XYDistance < SnapThreshold && XYDistance < BestCandidateDist)
            {
                BestCandidate = Candidate;
                BestCandidateMesh = Candidate->Mesh; // may be null; SnapToActor can handle
                BestCandidateDist = XYDistance;
            }
        }

        // Scan Buildings
        for (TActorIterator<ABuildingBase> ItB(GetWorld()); ItB; ++ItB)
        {
            ABuildingBase* Candidate = *ItB;
            if (!Candidate) continue;

            FVector OtherCenter, OtherExtent;
            if (!GetActorBoundsForSnap(Candidate, OtherCenter, OtherExtent))
                continue;

            const float XYDistance = FVector::Dist2D(DraggedWorkArea->GetActorLocation(), Candidate->GetActorLocation());
            const float CombinedXYExtent = (Extent.X + OtherExtent.X + Extent.Y + OtherExtent.Y) * 0.5f;
            const float SnapThreshold = SnapDistance + SnapGap + CombinedXYExtent;
            if (XYDistance < SnapThreshold && XYDistance < BestCandidateDist)
            {
                BestCandidate = Candidate;
                BestCandidateMesh = nullptr; // buildings may be ISM; let SnapToActor resolve bounds
                BestCandidateDist = XYDistance;
            }
        }

        if (BestCandidate)
        {
            SnapToActor(DraggedWorkArea, BestCandidate, BestCandidateMesh);
            return;
        }
    }

    //---------------------------------
    // If no snap, move the WorkArea
    //---------------------------------
    if (bHit && HitResult.GetActor() != nullptr) // && !bAnyOverlap
    {
        CurrentDraggedGround = HitResult.GetActor();

        FVector NewActorPosition = MouseGround;
        // Adjust Z if needed
        NewActorPosition.Z += DraggedAreaZOffset; 
        // Client-only preview move during drag with grounded Z
        FVector GroundedPos = ComputeGroundedLocation(DraggedWorkArea, NewActorPosition);

        // Enforce no-overlap during free movement, honoring SnapGap to all WorkAreas/Buildings
        if (UStaticMeshComponent* FreeMesh = DraggedWorkArea->Mesh)
        {
            const FBoxSphereBounds DragBounds = FreeMesh->CalcBounds(FreeMesh->GetComponentTransform());
            const FVector DragExtent = DragBounds.BoxExtent;
            const FVector DragCenter = DragBounds.Origin;
            const FVector CenterToActorOffset = DragCenter - DraggedWorkArea->GetActorLocation();

            // Strict no-overlap policy: if proposed position would overlap, do not move (freeze at previous location)
            {
                TArray<AActor*> Ignored;
                Ignored.Add(DraggedWorkArea);
                TArray<AActor*> Hits;

                const FVector InflatedExtent(DragExtent.X + SnapGap, DragExtent.Y + SnapGap, DragExtent.Z);
                // Build explicit object types for reliable overlap detection
                TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
                ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_WorldStatic));
                ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_WorldDynamic));
                ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_PhysicsBody));
                ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_Pawn));

                // Use the prospective mesh center as overlap center
                const FVector TestCenter = FVector(GroundedPos.X + CenterToActorOffset.X, GroundedPos.Y + CenterToActorOffset.Y, GroundedPos.Z + CenterToActorOffset.Z);

                const bool bOverlap = UKismetSystemLibrary::BoxOverlapActors(
                    GetWorld(),
                    TestCenter,               // test around mesh center
                    InflatedExtent,
                    ObjectTypes,
                    AActor::StaticClass(),
                    Ignored,
                    Hits
                );

                if (bOverlap)
                {
                    for (AActor* A : Hits)
                    {
                        if (!A || A == DraggedWorkArea) continue;
                        if (A->IsA(ALandscape::StaticClass())) continue; // ignore ground
                        if (Cast<AWorkArea>(A) || Cast<ABuildingBase>(A))
                        {
                            // stop movement immediately; keep last safe position
                            GroundedPos = DraggedWorkArea->GetActorLocation();
                            break;
                        }
                    }
                }
            }
        }

        DraggedWorkArea->SetActorLocation(GroundedPos);

        WorkAreaIsSnapped = false;
    }
}


void AExtendedControllerBase::SetWorkArea(FTransform AreaTransform)
{
    // Sanity check that we have at least one "SelectedUnit"
    if (SelectedUnits.Num() == 0 || !SelectedUnits[0])
    {
        return;
    }

    AWorkArea* DraggedWorkArea = SelectedUnits[0]->CurrentDraggedWorkArea;
    if (!DraggedWorkArea)
    {
        return;
    }
	
	SelectedUnits[0]->ShowWorkAreaIfNoFog_Implementation(DraggedWorkArea);

    const FVector AreaLocation = AreaTransform.GetLocation();
    // Raycast from the mouse into the scene
    FVector Start = AreaLocation+FVector(0.f, 0.f, 1000.f);
    FVector End   = AreaLocation-FVector(0.f, 0.f, 1000.f);

    FHitResult HitResult;
    FCollisionQueryParams CollisionParams;
    CollisionParams.bTraceComplex = true;
    CollisionParams.AddIgnoredActor(DraggedWorkArea);

    bool bHit = GetWorld()->LineTraceSingleByChannel(
        HitResult,
        Start,
        End,
        ECC_Visibility,
        CollisionParams
    );

    // Compute distance from the drag’s current location to the raycast hit, constrained to NavMesh
    UNavigationSystemV1* NavSys2 = UNavigationSystemV1::GetCurrent(GetWorld());
    if (!bHit || !NavSys2)
    {
        return; // freeze when no valid ground hit or nav system not available
    }
    FNavLocation NavLoc2;
    const bool bOnNav2 = NavSys2->ProjectPointToNavigation(HitResult.Location, NavLoc2, FVector(100.f,100.f,300.f));
    if (!bOnNav2)
    {
        return; // mouse not over NavMesh: stop moving until it is
    }
    const FVector MouseGround = NavLoc2.Location;
    float Distance = FVector::Dist(
        MouseGround,
        DraggedWorkArea->GetActorLocation()
    );

    // If currently snapped, keep updating snap while mouse stays within release distance; otherwise release
    if (CurrentSnapActor)
    {
        if (!IsValid(CurrentSnapActor))
        {
            CurrentSnapActor = nullptr;
            WorkAreaIsSnapped = false;
        }
        else if (bHit)
        {
            // Dynamic release threshold: half-extent(Dragged) + half-extent(Target) + 150, using MOUSE->TARGET distance
            FVector OtherCenter, OtherExtent;
            if (GetActorBoundsForSnap(CurrentSnapActor, OtherCenter, OtherExtent))
            {
                UStaticMeshComponent* DragMeshLocal = DraggedWorkArea->Mesh;
                const FBoxSphereBounds DragBoundsLocal = DragMeshLocal ? DragMeshLocal->CalcBounds(DragMeshLocal->GetComponentTransform()) : FBoxSphereBounds();
                const FVector DragExtentLocal = DragBoundsLocal.BoxExtent;

                const float DragXY = FMath::Max(DragExtentLocal.X, DragExtentLocal.Y);
                const float OtherXY = FMath::Max(OtherExtent.X, OtherExtent.Y);
                const float ReleaseThreshold = DragXY + OtherXY + 150.f;

                const float MouseToSnapTarget = FVector::Dist2D(MouseGround, OtherCenter);
                if (MouseToSnapTarget > ReleaseThreshold)
                {
                    UE_LOG(LogTemp, Warning, TEXT("[SetWorkArea] Release: MouseDist=%.1f > Threshold=%.1f. DragXY=%.1f OtherXY=%.1f Target=%s"),
                        MouseToSnapTarget, ReleaseThreshold, DragXY, OtherXY, *CurrentSnapActor->GetName());
                    CurrentSnapActor = nullptr;
                    WorkAreaIsSnapped = false;
                }
                else
                {
                    SnapToActor(DraggedWorkArea, CurrentSnapActor, nullptr);
                    return;
                }
            }
            else
            {
                CurrentSnapActor = nullptr;
                WorkAreaIsSnapped = false;
            }
        }
    }

    //---------------------------------------
    // Get DraggedWorkArea bounding box info
    //---------------------------------------
	UStaticMeshComponent* DraggedMesh = DraggedWorkArea->Mesh;

	if (!DraggedMesh)
	{
		// If there's no mesh on the dragged work area, bail out or handle gracefully
		return;
	}
	FBoxSphereBounds DraggedBounds = DraggedMesh->CalcBounds(DraggedMesh->GetComponentTransform());
	FVector Extent = DraggedBounds.BoxExtent; // half-size

    // Decide how you want to incorporate the bounding box size into your 
    // check. For example, add the length of the bounding box’s diagonal 
    // to your snap threshold, or just the largest dimension, or X/Y extents, etc.
    // Here, we do a quick example adding the entire box extent size.
    float DraggedBoxExtentSize = Extent.Size() * 2.f; 
    // e.g. can consider just .X/.Y if ignoring Z

    // If the distance < SnapDistance + SnapGap + "some factor of the box size"
	//  + SnapGap + DraggedBoxExtentSize
    if (Distance < (SnapDistance + SnapGap + DraggedBoxExtentSize))
    {
        // Try to overlap to see if we’re near something we can snap to
        if (UStaticMeshComponent* Mesh = DraggedWorkArea->Mesh)
        {
            // Use the sphere radius as an overlap check
            FBoxSphereBounds MeshBounds = Mesh->CalcBounds(Mesh->GetComponentTransform());
            float OverlapRadius = MeshBounds.SphereRadius;

            TArray<AActor*> OverlappedActors;
            bool bAnyOverlap = UKismetSystemLibrary::SphereOverlapActors(
                this,
                HitResult.Location,
                OverlapRadius,
                TArray<TEnumAsByte<EObjectTypeQuery>>(),
                AActor::StaticClass(),
                TArray<AActor*>(),  // Actors to ignore
                OverlappedActors
            );

            if (bAnyOverlap && OverlappedActors.Num() > 0)
            {
                for (AActor* OverlappedActor : OverlappedActors)
                {
                    // Ignore if the overlapped actor is invalid 
                    // or the same as the dragged one
                    if (!OverlappedActor || OverlappedActor == DraggedWorkArea)
                    {
                        continue;
                    }

                    AWorkArea* OverlappedWorkArea = Cast<AWorkArea>(OverlappedActor);
                    ABuildingBase* OverlappedBuilding = Cast<ABuildingBase>(OverlappedActor);
                    if (OverlappedWorkArea || OverlappedBuilding)
                    {
                        // Gate by mouse distance using dynamic release threshold (sum half-XY extents + 150)
                        {
                            FVector TmpCenter, TmpExtent;
                            if (GetActorBoundsForSnap(OverlappedActor, TmpCenter, TmpExtent))
                            {
                                const FVector DraggedExtentLocal2 = DraggedBounds.BoxExtent;
                                const float DragXY = FMath::Max(Extent.X, Extent.Y);
                                const float OtherXY = FMath::Max(TmpExtent.X, TmpExtent.Y);
                                const float ReleaseThreshold = DragXY + OtherXY + 150.f;
                                const float MouseToCandidate = FVector::Dist2D(MouseGround, TmpCenter);
                                if (MouseToCandidate > ReleaseThreshold)
                                {
                                    continue;
                                }
                            }
                        }
                        UStaticMeshComponent* OverlappedMesh = nullptr;
                        if (OverlappedWorkArea)
                        {
                            OverlappedMesh = OverlappedWorkArea->Mesh;
                            if (OverlappedWorkArea->IsNoBuildZone)
                            {
                                DraggedWorkArea->TemporarilyChangeMaterial();
                                break;
                            }
                            else if (OverlappedWorkArea->Type == WorkAreaData::Primary ||
                                     OverlappedWorkArea->Type == WorkAreaData::Secondary ||
                                     OverlappedWorkArea->Type == WorkAreaData::Tertiary ||
                                     OverlappedWorkArea->Type == WorkAreaData::Rare ||
                                     OverlappedWorkArea->Type == WorkAreaData::Epic ||
                                     OverlappedWorkArea->Type == WorkAreaData::Legendary)
                            {
                                break;
                            }
                        }

                        FVector OtherCenter, OtherExtent;
                        if (!GetActorBoundsForSnap(OverlappedActor, OtherCenter, OtherExtent))
                        {
                            continue;
                        }

                        float XYDistance = FVector::Dist2D(
                            DraggedWorkArea->GetActorLocation(),
                            OverlappedActor->GetActorLocation()
                        );

                        float CombinedXYExtent = (Extent.X + OtherExtent.X +
                                                  Extent.Y + OtherExtent.Y) * 0.5f;

                        float SnapThreshold = SnapDistance + SnapGap + CombinedXYExtent;

                        if (XYDistance < SnapThreshold)
                        {
                            SnapToActor(DraggedWorkArea, OverlappedActor, OverlappedMesh);
                            return;
                        }
                    }
                }
            }
        }
    }
    //---------------------------------
    // If no snap from overlap, try a fallback proximity scan (works with ISMs without overlaps)
    //---------------------------------
    {
        AActor* BestCandidate = nullptr;
        UStaticMeshComponent* BestCandidateMesh = nullptr;
        float BestCandidateDist = TNumericLimits<float>::Max();

        // Scan WorkAreas
        for (TActorIterator<AWorkArea> It(GetWorld()); It; ++It)
        {
            AWorkArea* Candidate = *It;
            if (!Candidate || Candidate == DraggedWorkArea) continue;

            if (Candidate->IsNoBuildZone)
            {
                // Visual feedback only; don't snap to no-build zones
                DraggedWorkArea->TemporarilyChangeMaterial();
                continue;
            }
            if (Candidate->Type == WorkAreaData::Primary ||
                Candidate->Type == WorkAreaData::Secondary ||
                Candidate->Type == WorkAreaData::Tertiary ||
                Candidate->Type == WorkAreaData::Rare ||
                Candidate->Type == WorkAreaData::Epic ||
                Candidate->Type == WorkAreaData::Legendary)
            {
                continue; // resources are not snap targets
            }

            FVector OtherCenter, OtherExtent;
            if (!GetActorBoundsForSnap(Candidate, OtherCenter, OtherExtent))
                continue;

            const float XYDistance = FVector::Dist2D(DraggedWorkArea->GetActorLocation(), Candidate->GetActorLocation());
            const float CombinedXYExtent = (Extent.X + OtherExtent.X + Extent.Y + OtherExtent.Y) * 0.5f;
            const float SnapThreshold = SnapDistance + SnapGap + CombinedXYExtent;
            if (XYDistance < SnapThreshold && XYDistance < BestCandidateDist)
            {
                BestCandidate = Candidate;
                BestCandidateMesh = Candidate->Mesh; // may be null; SnapToActor can handle
                BestCandidateDist = XYDistance;
            }
        }

        // Scan Buildings
        for (TActorIterator<ABuildingBase> ItB(GetWorld()); ItB; ++ItB)
        {
            ABuildingBase* Candidate = *ItB;
            if (!Candidate) continue;

            FVector OtherCenter, OtherExtent;
            if (!GetActorBoundsForSnap(Candidate, OtherCenter, OtherExtent))
                continue;

            const float XYDistance = FVector::Dist2D(DraggedWorkArea->GetActorLocation(), Candidate->GetActorLocation());
            const float CombinedXYExtent = (Extent.X + OtherExtent.X + Extent.Y + OtherExtent.Y) * 0.5f;
            const float SnapThreshold = SnapDistance + SnapGap + CombinedXYExtent;
            if (XYDistance < SnapThreshold && XYDistance < BestCandidateDist)
            {
                BestCandidate = Candidate;
                BestCandidateMesh = nullptr; // buildings may be ISM; let SnapToActor resolve bounds
                BestCandidateDist = XYDistance;
            }
        }

        if (BestCandidate)
        {
            SnapToActor(DraggedWorkArea, BestCandidate, BestCandidateMesh);
            return;
        }
    }

    //---------------------------------
    // If no snap, move the WorkArea
    //---------------------------------
    if (bHit && HitResult.GetActor() != nullptr) // && !bAnyOverlap
    {
        CurrentDraggedGround = HitResult.GetActor();

        FVector NewActorPosition = MouseGround;
        // Adjust Z if needed
        NewActorPosition.Z += DraggedAreaZOffset; 
        // Client-only preview move during drag with grounded Z
        const FVector GroundedPos2 = ComputeGroundedLocation(DraggedWorkArea, NewActorPosition);
        DraggedWorkArea->SetActorLocation(GroundedPos2);

        WorkAreaIsSnapped = false;
    }
}

void AExtendedControllerBase::MoveAbilityIndicator_Local(float DeltaSeconds)
{
    // Follow-mouse and snap/pushback like WorkArea, but only when DetectOverlapWithWorkArea is enabled
    if (SelectedUnits.Num() == 0)
    {
        return;
    }

    FVector MouseGround; FHitResult HitResult;
    const bool bHit = TraceMouseToGround(MouseGround, HitResult);
    if (!bHit)
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    for (AUnitBase* Unit : SelectedUnits)
    {
        if (!Unit) continue;
        AAbilityIndicator* CurrentIndicator = Unit->CurrentDraggedAbilityIndicator;
        if (!CurrentIndicator) continue;

        // Helper: apply optional rotation to face from owner unit to indicator location (yaw only)
        auto ApplyRotationIfNeeded = [&](AAbilityIndicator* Indicator)
        {
            if (!Indicator || !Indicator->RotatesToDirection) return;
            const FVector From = Unit->GetActorLocation();
            FVector To = Indicator->GetActorLocation();
            FVector Dir = To - From; Dir.Z = 0.f;
            if (!Dir.IsNearlyZero(1e-3f))
            {
                FRotator Rot = Dir.Rotation();
                Rot.Pitch = 0.f; Rot.Roll = 0.f;
                Indicator->SetActorRotation(Rot);
            }
        };

        // Handle range blinking regardless of placement mode
        {
            const FVector ALocation = Unit->GetMassActorLocation();
            const float Distance = FVector::Dist(MouseGround, ALocation);
            if (Unit->CurrentSnapshot.AbilityClass)
            {
                if (UGameplayAbilityBase* AbilityCDO = Unit->CurrentSnapshot.AbilityClass->GetDefaultObject<UGameplayAbilityBase>())
                {
                    if (AbilityCDO->Range != 0.f && Distance > AbilityCDO->Range)
                    {
                        AbilityIndicatorBlinkTimer += DeltaSeconds;
                        if (AbilityIndicatorBlinkTimer > 0.25f)
                        {
                            AbilityIndicatorBlinkTimer = 0.0f;
                            if (Unit->AbilityIndicatorVisibility) { Unit->HideAbilityIndicator(CurrentIndicator); }
                            else { Unit->ShowAbilityIndicator(CurrentIndicator); }
                        }
                    }
                    else
                    {
                        Unit->ShowAbilityIndicator(CurrentIndicator);
                    }
                }
            }
        }

        // If DetectOverlapWithWorkArea is disabled OR mesh is missing, just follow mouse (grounded)
        if (!CurrentIndicator->DetectOverlapWithWorkArea || !CurrentIndicator->IndicatorMesh)
        {
            // Ground to landscape keeping mesh bottom on ground
            const FBoxSphereBounds MeshBounds = CurrentIndicator->IndicatorMesh ? CurrentIndicator->IndicatorMesh->CalcBounds(CurrentIndicator->IndicatorMesh->GetComponentTransform()) : FBoxSphereBounds(CurrentIndicator->GetActorLocation(), FVector::ZeroVector, 0.f);
            const float HalfH = MeshBounds.BoxExtent.Z;
            const float BottomZ = MeshBounds.Origin.Z - HalfH;
            const float OffsetActorToBottom = BottomZ - CurrentIndicator->GetActorLocation().Z;
            FHitResult GroundHit;
            FCollisionQueryParams Params(FName(TEXT("AbilityIndicator_MouseFollowGround")), true, CurrentIndicator);
            Params.AddIgnoredActor(CurrentIndicator);
            for (AUnitBase* U : SelectedUnits) { if (U && U->CurrentDraggedAbilityIndicator) { Params.AddIgnoredActor(U->CurrentDraggedAbilityIndicator); } }
            const FVector TraceStart(MouseGround.X, MouseGround.Y, MouseGround.Z + 2000.f);
            const FVector TraceEnd  (MouseGround.X, MouseGround.Y, MouseGround.Z - 10000.f);
            FVector FinalLoc = MouseGround;
            if (World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility, Params))
            {
                FinalLoc.Z = GroundHit.ImpactPoint.Z - OffsetActorToBottom;
            }
            CurrentIndicator->SetActorLocation(FinalLoc);
            ApplyRotationIfNeeded(CurrentIndicator);
            continue;
        }

        // Distance-only logic with recursive pushback (including SnapGap), same as WorkArea
        auto GroundToLandscape = [&](const FVector& InLoc)
        {
            FVector Grounded = InLoc;
            const FBoxSphereBounds MeshBoundsNow = CurrentIndicator->IndicatorMesh->CalcBounds(CurrentIndicator->IndicatorMesh->GetComponentTransform());
            const float HalfH = MeshBoundsNow.BoxExtent.Z;
            const float BottomZ = MeshBoundsNow.Origin.Z - HalfH;
            const float OffsetActorToBottom = BottomZ - CurrentIndicator->GetActorLocation().Z;
            FHitResult GroundHit;
            FCollisionQueryParams Params(FName(TEXT("AbilityIndicator_GroundTrace")), true, CurrentIndicator);
            Params.AddIgnoredActor(CurrentIndicator);
            for (AUnitBase* U : SelectedUnits) { if (U && U->CurrentDraggedAbilityIndicator) { Params.AddIgnoredActor(U->CurrentDraggedAbilityIndicator); } }
            const FVector TraceStart(InLoc.X, InLoc.Y, InLoc.Z + 2000.f);
            const FVector TraceEnd  (InLoc.X, InLoc.Y, InLoc.Z - 10000.f);
            if (World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility, Params))
            {
                Grounded.Z = GroundHit.ImpactPoint.Z - OffsetActorToBottom;
            }
            return Grounded;
        };

        FVector DesiredGrounded = GroundToLandscape(MouseGround);

        // Last safe position per-indicator
        static TWeakObjectPtr<AAbilityIndicator> LastIndRef = nullptr;
        static FVector LastSafeLoc = FVector::ZeroVector;
        static bool bHasLastSafe = false;
        if (LastIndRef.Get() != CurrentIndicator)
        {
            LastIndRef = CurrentIndicator;
            bHasLastSafe = false;
        }

        // Drag extents
        const FBoxSphereBounds DragBounds = CurrentIndicator->IndicatorMesh->CalcBounds(CurrentIndicator->IndicatorMesh->GetComponentTransform());
        const FVector DragExtent = DragBounds.BoxExtent;
        const float DragXY = FMath::Max(DragExtent.X, DragExtent.Y);

        // Pick best candidate
        AActor* BestCandidate = nullptr;
        float BestDist = TNumericLimits<float>::Max();
        auto ConsiderActor = [&](AActor* Candidate)
        {
            if (!Candidate || Candidate == CurrentIndicator) return;
            // Do not consider the owner unit/building as a blocking/snap target
            if (Candidate == Unit) return;
            FVector OtherCenter, OtherExtent;
            if (!GetActorBoundsForSnap(Candidate, OtherCenter, OtherExtent)) return;
            const float OtherXY = FMath::Max(OtherExtent.X, OtherExtent.Y);
            const float Threshold = DragXY + OtherXY;
            const float D = FVector::Dist2D(DesiredGrounded, OtherCenter);
            if (D < Threshold && D < BestDist)
            {
                BestDist = D; BestCandidate = Candidate;
            }
        };
        for (TActorIterator<AWorkArea> ItWA(World); ItWA; ++ItWA) { ConsiderActor(*ItWA); }
        for (TActorIterator<ABuildingBase> ItBD(World); ItBD; ++ItBD) { ConsiderActor(*ItBD); }

        auto ComputeSnapForIndicator = [&](AActor* OtherActor, FVector& OutLoc)
        {
            if (!OtherActor) return false;
            const FBoxSphereBounds DragNow = CurrentIndicator->IndicatorMesh->CalcBounds(CurrentIndicator->IndicatorMesh->GetComponentTransform());
            const FVector DragCenter = DragNow.Origin;
            const FVector DragExt = DragNow.BoxExtent;
            const FVector CenterToActorOffset = DragCenter - CurrentIndicator->GetActorLocation();
            FVector OtherCenter, OtherExt;
            if (!GetActorBoundsForSnap(OtherActor, OtherCenter, OtherExt)) return false;
            float EffectiveGap = SnapGap;
            if (ABuildingBase* TargetBuilding = Cast<ABuildingBase>(OtherActor))
            {
                EffectiveGap = FMath::Max(0.f, SnapGap + TargetBuilding->SnapGapAdjustment);
                if (UCapsuleComponent* Capsule = TargetBuilding->FindComponentByClass<UCapsuleComponent>())
                {
                    const float R = Capsule->GetScaledCapsuleRadius();
                    OtherExt.X = R; OtherExt.Y = R;
                    OtherCenter = TargetBuilding->GetActorLocation();
                }
            }
            const FVector2D ToTarget(MouseGround.X - OtherCenter.X, MouseGround.Y - OtherCenter.Y);
            FVector NewCenter = DragCenter;
            if (FMath::Abs(ToTarget.X) >= FMath::Abs(ToTarget.Y))
            {
                const float Sign = (ToTarget.X >= 0.f) ? 1.f : -1.f;
                NewCenter.X = OtherCenter.X + Sign * (OtherExt.X + DragExt.X + EffectiveGap);
                NewCenter.Y = OtherCenter.Y;
            }
            else
            {
                const float Sign = (ToTarget.Y >= 0.f) ? 1.f : -1.f;
                NewCenter.Y = OtherCenter.Y + Sign * (OtherExt.Y + DragExt.Y + EffectiveGap);
                NewCenter.X = OtherCenter.X;
            }
            OutLoc = GroundToLandscape(NewCenter - CenterToActorOffset);
            return true;
        };

        auto ResolveDistances = [&](FVector& InOutLoc, bool& bOutAllGood)
        {
            const int32 MaxIterations = 12;
            const float Padding = 1.0f;
            const float MinStep = 0.1f;
            bOutAllGood = false;
            TArray<AActor*> Neighbors;
            for (TActorIterator<AWorkArea> ItWA2(World); ItWA2; ++ItWA2) { if (*ItWA2) Neighbors.Add(*ItWA2); }
            for (TActorIterator<ABuildingBase> ItBD2(World); ItBD2; ++ItBD2) { if (*ItBD2) Neighbors.Add(*ItBD2); }
            FVector Working = InOutLoc;
            bool bSolved = false;
            for (int32 Iter=0; Iter<MaxIterations; ++Iter)
            {
                CurrentIndicator->SetActorLocation(Working);
                const FBoxSphereBounds DragNow = CurrentIndicator->IndicatorMesh->CalcBounds(CurrentIndicator->IndicatorMesh->GetComponentTransform());
                const FVector DragExtNow = DragNow.BoxExtent;
                const float DragR = FMath::Max(DragExtNow.X, DragExtNow.Y);
                bool bAnyViolation = false;
                FVector Accum = FVector::ZeroVector;
                for (AActor* N : Neighbors)
                {
                    if (!N) continue;
                    // Do not be blocked by the owning unit/building
                    if (N == Unit) continue;
                    if (ABuildingBase* OwnerBuilding = Cast<ABuildingBase>(Unit)) { if (N == OwnerBuilding) continue; }
                    FVector NC, NE; if (!GetActorBoundsForSnap(N, NC, NE)) continue;
                    const float NR = FMath::Max(NE.X, NE.Y);
                    float Required = DragR + NR + SnapGap;
                    if (CurrentIndicator->DetectOverlapWithWorkArea && CurrentIndicator->DenyPlacementCloseToResources)
                    {
                        if (AWorkArea* NeighborWA = Cast<AWorkArea>(N))
                        {
                            const WorkAreaData::WorkAreaType T = NeighborWA->Type;
                            const bool bIsResourceType = (T == WorkAreaData::Primary || T == WorkAreaData::Secondary || T == WorkAreaData::Tertiary || T == WorkAreaData::Rare || T == WorkAreaData::Epic || T == WorkAreaData::Legendary);
                            if (bIsResourceType)
                            {
                                Required = FMath::Max(Required, CurrentIndicator->ResourcePlacementDistance);
                            }
                        }
                    }
                    const float D = FVector::Dist2D(DragNow.Origin, NC);
                    if (D < Required)
                    {
                        bAnyViolation = true;
                        FVector Dir = (DragNow.Origin - NC); Dir.Z = 0.f;
                        if (Dir.IsNearlyZero(1e-3f)) Dir = FVector(1,0,0);
                        Dir.Normalize();
                        const float Push = (Required - D) + Padding;
                        Accum += Dir * Push;
                    }
                }
                if (!bAnyViolation) { bSolved = true; break; }
                if (Accum.Size2D() < MinStep) break;
                Working += FVector(Accum.X, Accum.Y, 0.f);
            }
            CurrentIndicator->SetActorLocation(Working);
            if (!bSolved)
            {
                const FBoxSphereBounds DragNow = CurrentIndicator->IndicatorMesh->CalcBounds(CurrentIndicator->IndicatorMesh->GetComponentTransform());
                const FVector DragExtNow = DragNow.BoxExtent;
                const float DragR = FMath::Max(DragExtNow.X, DragExtNow.Y);
                bool bStillBad = false;
                for (AActor* N : Neighbors)
                {
                    if (!N) continue;
                    FVector NC, NE; if (!GetActorBoundsForSnap(N, NC, NE)) continue;
                    const float NR = FMath::Max(NE.X, NE.Y);
                    const float Required = DragR + NR + SnapGap;
                    const float D = FVector::Dist2D(DragNow.Origin, NC);
                    if (D < Required) { bStillBad = true; break; }
                }
                if (!bStillBad) bSolved = true;
            }
            bOutAllGood = bSolved;
            InOutLoc = Working;
        };

        if (BestCandidate)
        {
            FVector SnapLoc;
            if (ComputeSnapForIndicator(BestCandidate, SnapLoc))
            {
                CurrentIndicator->SetActorLocation(SnapLoc);
                bool bAllGood = false; FVector Corrected = SnapLoc; ResolveDistances(Corrected, bAllGood);
                if (!bAllGood)
                {
                    if (bHasLastSafe) { CurrentIndicator->SetActorLocation(LastSafeLoc); ApplyRotationIfNeeded(CurrentIndicator); }
                    WorkAreaIsSnapped = false; CurrentSnapActor = nullptr;
                }
                else
                {
                    CurrentIndicator->SetActorLocation(Corrected);
                    ApplyRotationIfNeeded(CurrentIndicator);
                    LastSafeLoc = Corrected; bHasLastSafe = true; WorkAreaIsSnapped = true; CurrentSnapActor = BestCandidate;
                }
                continue;
            }
        }

        // No snap -> free move then resolve
        {
            FVector Proposed = DesiredGrounded;
            CurrentIndicator->SetActorLocation(Proposed);
            bool bAllGood = false; ResolveDistances(Proposed, bAllGood);
            if (!bAllGood)
            {
                if (bHasLastSafe) { CurrentIndicator->SetActorLocation(LastSafeLoc); ApplyRotationIfNeeded(CurrentIndicator); }
                WorkAreaIsSnapped = false; CurrentSnapActor = nullptr;
            }
            else
            {
                CurrentIndicator->SetActorLocation(Proposed);
                ApplyRotationIfNeeded(CurrentIndicator);
                LastSafeLoc = Proposed; bHasLastSafe = true; WorkAreaIsSnapped = false; CurrentSnapActor = nullptr;
            }
        }
    }
}

void AExtendedControllerBase::Server_SpawnExtensionConstructionUnit_Implementation(AUnitBase* Unit, AWorkArea* WA)
{
	if (!WA || !Unit) return;
	if (!WA->IsExtensionArea) return;
	
	UWorld* World = GetWorld();
	if (!World) return;

	if (WA->ConstructionUnitClass)
	{
 	// Determine desired yaw for extension based on WA position relative to the initiating Unit
	FRotator DesiredRot = WA->ServerMeshRotationBuilding;
	if (Unit)
	{
		const FVector UnitLoc = Unit->GetActorLocation();
		const FVector WLoc = WA->GetActorLocation();
		const FVector2D Delta2D(WLoc.X - UnitLoc.X, WLoc.Y - UnitLoc.Y);
		if (FMath::Abs(Delta2D.X) >= FMath::Abs(Delta2D.Y))
		{
			DesiredRot = FRotator(0.f, (Delta2D.X >= 0.f) ? 0.f : 180.f, 0.f);
		}
		else
		{
			DesiredRot = FRotator(0.f, (Delta2D.Y >= 0.f) ? 90.f : 270.f, 0.f);
		}
	}
	WA->ServerMeshRotationBuilding = DesiredRot;

	const FTransform SpawnTM(DesiredRot, WA->GetActorLocation());
	AUnitBase* NewConstruction = World->SpawnActorDeferred<AUnitBase>(WA->ConstructionUnitClass, SpawnTM, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (NewConstruction)
	{
		// Ground trace at the area center to find floor Z
		FBox AreaBox = WA->Mesh ? WA->Mesh->Bounds.GetBox() : WA->GetComponentsBoundingBox(true);
		const FVector AreaCenter = AreaBox.GetCenter();
		const FVector AreaSize = AreaBox.GetSize();
		FHitResult Hit;
		FVector Start = AreaCenter + FVector(0, 0, 10000.f);
		FVector End = AreaCenter - FVector(0, 0, 10000.f);
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(WA);
		Params.AddIgnoredActor(NewConstruction);
		Params.AddIgnoredActor(Unit);
		bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
		float GroundZ = bHit ? Hit.Location.Z : WA->GetActorLocation().Z;

		NewConstruction->FlyHeight = WA->GetActorLocation().Z - GroundZ;
		NewConstruction->TeamId = Unit->TeamId;

		// Ensure every runtime-spawned construction unit gets a unique UnitIndex on the server
		// (level-placed units are assigned in ARTSGameModeBase::FillUnitArrays).
		if (ARTSGameModeBase* GM = World->GetAuthGameMode<ARTSGameModeBase>())
		{
			if (NewConstruction->UnitIndex <= 0)
			{
				GM->AddUnitIndexAndAssignToAllUnitsArrayWithIndex(NewConstruction, INDEX_NONE, FUnitSpawnParameter());
			}
			else
			{
				// Make sure the unit is tracked by the authoritative unit list as well
				if (!GM->AllUnits.Contains(NewConstruction))
				{
					GM->AllUnits.Add(NewConstruction);
				}
			}
		}

		if (WA->BuildingClass)
		{
			if (ABuildingBase* BuildingCDO = WA->BuildingClass->GetDefaultObject<ABuildingBase>())
			{
				NewConstruction->DefaultAttributeEffect = BuildingCDO->DefaultAttributeEffect;
			}
		}
		if (AConstructionUnit* CU = Cast<AConstructionUnit>(NewConstruction))
		{
			CU->Worker = Unit;
			CU->WorkArea = WA;
			CU->CastTime = WA->BuildTime;
		}
		NewConstruction->ServerMeshRotation = DesiredRot;
		NewConstruction->FinishSpawning(SpawnTM);
		if (NewConstruction->AbilitySystemComponent)
		{
			NewConstruction->AbilitySystemComponent->InitAbilityActorInfo(NewConstruction, NewConstruction);
			NewConstruction->InitializeAttributes();
		}
		NewConstruction->SetMeshRotationServer();
		// Fit/ground-align and visuals same as before
			{
				FBox PreBox = NewConstruction->GetComponentsBoundingBox(true);
				FVector UnitSize = PreBox.GetSize();
				if (!UnitSize.IsNearlyZero(1e-3f) && AreaSize.X > KINDA_SMALL_NUMBER && AreaSize.Y > KINDA_SMALL_NUMBER)
				{
					const float Margin = 0.98f;
					const float ScaleX = (AreaSize.X * Margin) / FMath::Max(KINDA_SMALL_NUMBER, UnitSize.X);
					const float ScaleY = (AreaSize.Y * Margin) / FMath::Max(KINDA_SMALL_NUMBER, UnitSize.Y);
					const float ScaleZComp = (AreaSize.Z * Margin) / FMath::Max(KINDA_SMALL_NUMBER, UnitSize.Z);
					const float Uniform = FMath::Max(FMath::Min(ScaleX, ScaleY), 0.1f);
					FVector NewScale = NewConstruction->GetActorScale3D();
					NewScale.X = Uniform;
					NewScale.Y = Uniform;
					NewScale.Z = Uniform;
					if (AConstructionUnit* CU_ScaleCheck = Cast<AConstructionUnit>(NewConstruction))
					{
						if (CU_ScaleCheck->ScaleZ)
						{
							NewScale.Z = ScaleZComp;
						}
					}
					NewConstruction->SetActorScale3D(NewScale * 2.f);
				}
				
				FBox ScaledBox = NewConstruction->GetComponentsBoundingBox(true);
				const FVector UnitCenter = ScaledBox.GetCenter();
				const float BottomZ = ScaledBox.Min.Z;
				FVector FinalLoc = NewConstruction->GetActorLocation();
				FinalLoc.X += (AreaCenter.X - UnitCenter.X);
				FinalLoc.Y += (AreaCenter.Y - UnitCenter.Y);
				if (!NewConstruction->IsFlying) FinalLoc.Z += (GroundZ - BottomZ);
				NewConstruction->SetActorLocation(FinalLoc);
			}
			if (AConstructionUnit* CU_Anim = Cast<AConstructionUnit>(NewConstruction))
			{
				const float AnimDuration = WA->BuildTime * 0.95f;
				CU_Anim->MulticastStartRotateVisual(CU_Anim->DefaultRotateAxis, CU_Anim->DefaultRotateDegreesPerSecond, AnimDuration);
				CU_Anim->MulticastStartOscillateVisual(CU_Anim->DefaultOscOffsetA, CU_Anim->DefaultOscOffsetB, CU_Anim->DefaultOscillationCyclesPerSecond, AnimDuration);
				if (CU_Anim->bPulsateScaleDuringBuild)
				{
					CU_Anim->MulticastPulsateScale(CU_Anim->PulsateMinMultiplier, CU_Anim->PulsateMaxMultiplier, CU_Anim->PulsateTimeMinToMax, true);
				}
			}
			WA->ConstructionUnit = NewConstruction;
			WA->bConstructionUnitSpawned = true;
			NewConstruction->BuildArea = WA;
			if (AConstructionUnit* CU = Cast<AConstructionUnit>(NewConstruction))
			{
				CU->SwitchEntityTag(FMassStateCastingTag::StaticStruct());
			}
			
				if (!Unit->CurrentDraggedWorkArea)
				{
					return;
				}
				
					Unit->BuildArea = Unit->CurrentDraggedWorkArea;
					Unit->BuildArea->TeamId = Unit->TeamId;
					Unit->BuildArea->PlannedBuilding = true;
					Unit->BuildArea->ControlTimer = 0.f;
					Unit->BuildArea->AddAreaToGroup();
					AResourceGameMode* ResourceGameMode = Cast<AResourceGameMode>(RTSGameMode);

					if(ResourceGameMode && Unit->BuildArea->IsPaid)
					{
						ResourceGameMode->ModifyResource(EResourceType::Primary, Unit->TeamId, -Unit->BuildArea->ConstructionCost.PrimaryCost);
						ResourceGameMode->ModifyResource(EResourceType::Secondary, Unit->TeamId, -Unit->BuildArea->ConstructionCost.SecondaryCost);
						ResourceGameMode->ModifyResource(EResourceType::Tertiary, Unit->TeamId, -Unit->BuildArea->ConstructionCost.TertiaryCost);
						ResourceGameMode->ModifyResource(EResourceType::Rare, Unit->TeamId, -Unit->BuildArea->ConstructionCost.RareCost);
						ResourceGameMode->ModifyResource(EResourceType::Epic, Unit->TeamId, -Unit->BuildArea->ConstructionCost.EpicCost);
						ResourceGameMode->ModifyResource(EResourceType::Legendary, Unit->TeamId, -Unit->BuildArea->ConstructionCost.LegendaryCost);
					}
				 Unit->CurrentDraggedWorkArea = nullptr;
		}
	}
}

void AExtendedControllerBase::SendWorkerToWork_Implementation(AUnitBase* Worker)
{

	UE_LOG(LogTemp, Warning, TEXT("!!!SendWorkerToWork!!!"));
	if (!Worker)
	{
		UE_LOG(LogTemp, Error, TEXT("Worker is null! Cannot proceed."));
		return;
	}

	if (!Worker->CurrentDraggedWorkArea)
	{
		UE_LOG(LogTemp, Error, TEXT("Worker->CurrentDraggedWorkArea is null! Cannot assign to Worker."));
		return;
	}
	
		Worker->BuildArea = Worker->CurrentDraggedWorkArea;
		Worker->BuildArea->TeamId = Worker->TeamId;
		Worker->BuildArea->PlannedBuilding = true;
		Worker->BuildArea->ControlTimer = 0.f;
		Worker->BuildArea->AddAreaToGroup();
		AResourceGameMode* ResourceGameMode = Cast<AResourceGameMode>(RTSGameMode);

		if(ResourceGameMode && Worker->BuildArea->IsPaid)
		{
			ResourceGameMode->ModifyResource(EResourceType::Primary, Worker->TeamId, -Worker->BuildArea->ConstructionCost.PrimaryCost);
			ResourceGameMode->ModifyResource(EResourceType::Secondary, Worker->TeamId, -Worker->BuildArea->ConstructionCost.SecondaryCost);
			ResourceGameMode->ModifyResource(EResourceType::Tertiary, Worker->TeamId, -Worker->BuildArea->ConstructionCost.TertiaryCost);
			ResourceGameMode->ModifyResource(EResourceType::Rare, Worker->TeamId, -Worker->BuildArea->ConstructionCost.RareCost);
			ResourceGameMode->ModifyResource(EResourceType::Epic, Worker->TeamId, -Worker->BuildArea->ConstructionCost.EpicCost);
			ResourceGameMode->ModifyResource(EResourceType::Legendary, Worker->TeamId, -Worker->BuildArea->ConstructionCost.LegendaryCost);
		}
	
		// Check if the worker is overlapping with the build area
		if (Worker->IsOverlappingActor(Worker->BuildArea))
		{
			// If they are overlapping, set the state to 'Build'
			Worker->SetUnitState(UnitData::Build);
			Worker->SwitchEntityTagByState(UnitData::Build, Worker->UnitStatePlaceholder);
			Worker->SetUEPathfinding = true;
		}
		else
		{
			// If they are not overlapping, set the state to 'GoToBuild'
			Worker->SetUnitState(UnitData::GoToBuild);
			Worker->SwitchEntityTagByState(UnitData::GoToBuild, Worker->UnitStatePlaceholder);
			Worker->SetUEPathfinding = true;
		}
	

	

	
	 Worker->CurrentDraggedWorkArea = nullptr;
}


void AExtendedControllerBase::SendWorkerToBase_Implementation(AUnitBase* Worker)
{
	if (!Worker)
	{
		UE_LOG(LogTemp, Error, TEXT("Worker is null! Cannot proceed."));
		return;
	}
	
	Worker->SetUnitState(UnitData::GoToBase);
	Worker->SwitchEntityTagByState(UnitData::GoToBase, Worker->UnitStatePlaceholder);
}

bool AExtendedControllerBase::DropWorkArea()
{
	if(SelectedUnits.Num() && SelectedUnits[0])
	if (SelectedUnits[0]->CurrentDraggedWorkArea && SelectedUnits[0]->CurrentDraggedWorkArea->PlannedBuilding == false)
	{
		// Get all actors overlapping with the CurrentDraggedWorkArea
		TArray<AActor*> OverlappingActors;
		SelectedUnits[0]->CurrentDraggedWorkArea->GetOverlappingActors(OverlappingActors);
	
		bool bIsOverlappingWithValidArea = false;
		bool IsNoBuildZone = false;
		// Determine initiating building to ignore for extension areas
		ABuildingBase* InitiatingBuilding = Cast<ABuildingBase>(SelectedUnits[0]);
		const bool bIsExtensionArea = SelectedUnits[0]->CurrentDraggedWorkArea->IsExtensionArea;
		// Loop through the overlapping actors to check if they are instances of AWorkArea or ABuildingBase
		for (AActor* OverlappedActor : OverlappingActors)
		{
			// Ignore the initiating building when placing an Extension Area
			if (bIsExtensionArea && InitiatingBuilding && OverlappedActor == InitiatingBuilding)
			{
				continue;
			}
			if (OverlappedActor->IsA(AWorkArea::StaticClass()) || OverlappedActor->IsA(ABuildingBase::StaticClass()))
			{
				AWorkArea* NoBuildZone = Cast<AWorkArea>(OverlappedActor);
				if (NoBuildZone && NoBuildZone->IsNoBuildZone == true) IsNoBuildZone = NoBuildZone->IsNoBuildZone;
				
				bIsOverlappingWithValidArea = true;
				break;
			}
		}

		// NeedsBeacon check: outside of any beacon range?
		bool bNeedsBeaconOutOfRange = false;
		if (SelectedUnits[0] && SelectedUnits[0]->CurrentDraggedWorkArea && SelectedUnits[0]->CurrentDraggedWorkArea->NeedsBeacon)
		{
			UWorld* WorldCtx = GetWorld();
			if (WorldCtx)
			{
				const FVector Pos = SelectedUnits[0]->CurrentDraggedWorkArea->GetActorLocation();
				bNeedsBeaconOutOfRange = !ABuildingBase::IsLocationInBeaconRange(WorldCtx, Pos);
			}
		}
		
		// If overlapping with AWorkArea or ABuildingBase, destroy the CurrentDraggedWorkArea
		if ((bIsOverlappingWithValidArea && !WorkAreaIsSnapped) || IsNoBuildZone || bNeedsBeaconOutOfRange) // bIsOverlappingWithValidArea &&
		{
			if (DropWorkAreaFailedSound)
			{
				Client_PlaySound2D(DropWorkAreaFailedSound);
			}
			
			SelectedUnits[0]->CurrentDraggedWorkArea->Destroy();
			SelectedUnits[0]->BuildArea = nullptr;
			SelectedUnits[0]->CurrentDraggedWorkArea = nullptr;
			CancelCurrentAbility(SelectedUnits[0]);
			SendWorkerToBase(SelectedUnits[0]);
			return true;
		}

		if(SelectedUnits.Num() && SelectedUnits[0] && SelectedUnits[0]->IsWorker)
		{
			if (DropWorkAreaSound)
			{
				Client_PlaySound2D(DropWorkAreaSound);
			}
	
 			// Finalize placement on the server now that the player dropped the area
				if (SelectedUnits[0]->CurrentDraggedWorkArea)
				{
					Server_FinalizeWorkAreaPosition(SelectedUnits[0]->CurrentDraggedWorkArea,
						SelectedUnits[0]->CurrentDraggedWorkArea->GetActorTransform(), SelectedUnits[0]);
				}
				// If this is an extension area, spawn its ConstructionUnit now on the server
				SendWorkerToWork(SelectedUnits[0]);
				return true;
		}

		if (SelectedUnits[0]->CurrentDraggedWorkArea && SelectedUnits[0]->CurrentDraggedWorkArea->IsExtensionArea)
		{
			Server_FinalizeWorkAreaPosition(SelectedUnits[0]->CurrentDraggedWorkArea,
			SelectedUnits[0]->CurrentDraggedWorkArea->GetActorTransform(), SelectedUnits[0]);
			Server_SpawnExtensionConstructionUnit(SelectedUnits[0], SelectedUnits[0]->CurrentDraggedWorkArea);
			SetAbilityEnabledByKey(SelectedUnits[0], "ExtensionAbility", false);
			return true;
		}
	}
 //SelectedUnits[0]->CurrentDraggedWorkArea = nullptr;
	return false;
}


void AExtendedControllerBase::SetAbilityEnabledByKey(AUnitBase* UnitBase, const FString& Key, bool bEnable)
{
	const FString NormalizedKey = NormalizeAbilityKey(Key);
	
	if (UnitBase && UnitBase->HasAuthority())
	{
		if (UWorld* World = GetWorld())
		{
			int32 SentCount = 0;
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				ACustomControllerBase* CustomPC = Cast<ACustomControllerBase>(It->Get());
				if (!CustomPC) continue;
				if (CustomPC->SelectableTeamId == UnitBase->TeamId)
				{
					CustomPC->Client_ApplyOwnerAbilityKeyToggle(UnitBase, NormalizedKey, bEnable);
					++SentCount;
				}
			}
		}
	}
}

bool AExtendedControllerBase::DropWorkAreaForUnit(AUnitBase* UnitBase, bool bWorkAreaIsSnapped, USoundBase* InDropWorkAreaFailedSound)
{
	if (!UnitBase) return false;

	if (UnitBase->CurrentDraggedWorkArea && UnitBase->CurrentDraggedWorkArea->PlannedBuilding == false)
	{
		TArray<AActor*> OverlappingActors;

		bool bIsOverlappingWithValidArea = false;
		bool bIsNoBuildZone = false;
		// Determine initiating building to ignore when placing an Extension Area
		ABuildingBase* InitiatingBuilding = Cast<ABuildingBase>(UnitBase);
		const bool bIsExtensionArea = UnitBase->CurrentDraggedWorkArea->IsExtensionArea;
		for (AActor* OverlappedActor : OverlappingActors)
		{
			if (bIsExtensionArea && InitiatingBuilding && OverlappedActor == InitiatingBuilding)
			{
				continue;
			}
			if (OverlappedActor->IsA(AWorkArea::StaticClass()) || OverlappedActor->IsA(ABuildingBase::StaticClass()))
			{
				AWorkArea* NoBuildZone = Cast<AWorkArea>(OverlappedActor);
				if (NoBuildZone && NoBuildZone->IsNoBuildZone == true)
				{
					bIsNoBuildZone = true;
				}
				bIsOverlappingWithValidArea = true;
				break;
			}
		}

	
		bool bNeedsBeaconOutOfRange = false;
		if (UnitBase && UnitBase->CurrentDraggedWorkArea && UnitBase->CurrentDraggedWorkArea->NeedsBeacon)
		{
			UWorld* WorldCtx = GetWorld();
			if (WorldCtx)
			{
				const FVector Pos = UnitBase->CurrentDraggedWorkArea->GetActorLocation();
				bNeedsBeaconOutOfRange = !ABuildingBase::IsLocationInBeaconRange(WorldCtx, Pos);
			}
		}

		// && !bWorkAreaIsSnapped
		if ((bIsOverlappingWithValidArea && !bWorkAreaIsSnapped) || bIsNoBuildZone || bNeedsBeaconOutOfRange)
		{
			if (InDropWorkAreaFailedSound)
			{
				Client_PlaySound2D(InDropWorkAreaFailedSound);
			}

			UnitBase->CurrentDraggedWorkArea->Destroy();
			UnitBase->BuildArea = nullptr;
			UnitBase->CurrentDraggedWorkArea = nullptr;
			CancelCurrentAbility(UnitBase);
			SendWorkerToBase(UnitBase);
			return true;
		}

		if (UnitBase->IsWorker)
		{
			if (DropWorkAreaSound)
			{
				Client_PlaySound2D(DropWorkAreaSound);
			}

 		if (UnitBase->CurrentDraggedWorkArea)
			{
				Server_FinalizeWorkAreaPosition(UnitBase->CurrentDraggedWorkArea,
					UnitBase->CurrentDraggedWorkArea->GetActorTransform(), UnitBase);
			}
			// If this is an extension area, spawn its ConstructionUnit now on the server
			SendWorkerToWork(UnitBase);
			return true;
		}

		if (UnitBase->CurrentDraggedWorkArea && UnitBase->CurrentDraggedWorkArea->IsExtensionArea)
		{
			Server_FinalizeWorkAreaPosition(UnitBase->CurrentDraggedWorkArea,
				UnitBase->CurrentDraggedWorkArea->GetActorTransform(), UnitBase);
			Server_SpawnExtensionConstructionUnit(UnitBase, UnitBase->CurrentDraggedWorkArea);
			SetAbilityEnabledByKey(UnitBase, "ExtensionAbility", false);
			return true;
		}
	}
	return false;
}

void AExtendedControllerBase::Server_DropWorkAreaForUnit_Implementation(AUnitBase* UnitBase, bool bWorkAreaIsSnapped, USoundBase* InDropWorkAreaFailedSound, FTransform ClientWorkAreaTransform)
{
	if (!UnitBase || !UnitBase->CurrentDraggedWorkArea || UnitBase->CurrentDraggedWorkArea->AreaDropped) return;
	
	UnitBase->CurrentDraggedWorkArea->AreaDropped = true;
	UnitBase->CurrentDraggedWorkArea->SetActorTransform(ClientWorkAreaTransform);
	DropWorkAreaForUnit(UnitBase, bWorkAreaIsSnapped, InDropWorkAreaFailedSound);
}

void AExtendedControllerBase::MoveDraggedUnit_Implementation(float DeltaSeconds)
{
	// Optionally, attach the actor to the cursor
	if(CurrentDraggedUnitBase)
	{
		FVector MousePosition, MouseDirection;
		DeprojectMousePositionToWorld(MousePosition, MouseDirection);

		// Raycast from the mouse position into the scene to find the ground
		FVector Start = MousePosition;
		FVector End = Start + MouseDirection * 1000000.f; // Extend to a maximum reasonable distance

		FHitResult HitResult;
		FCollisionQueryParams CollisionParams;
		CollisionParams.bTraceComplex = true; // Use complex collision for precise tracing
		CollisionParams.AddIgnoredActor(CurrentDraggedUnitBase); // Ignore the dragged actor in the raycast

		// Perform the raycast
		bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, CollisionParams);

		// Check if something was hit
		if (bHit && HitResult.GetActor() != nullptr)
		{
			CurrentDraggedGround = HitResult.GetActor();
			// Update the actor's position to the hit location
			FVector NewActorPosition = HitResult.Location;
			NewActorPosition.Z += 50.f;
			CurrentDraggedUnitBase->SetActorLocation(NewActorPosition);
		}

	}
}

void AExtendedControllerBase::DragUnitBase(AUnitBase* UnitToDrag)
{
	if(UnitToDrag->IsOnPlattform && SpawnPlatform)
	{
		if(SpawnPlatform->GetEnergy() >= UnitToDrag->EnergyCost)
		{
			CurrentDraggedUnitBase = UnitToDrag;
			CurrentDraggedUnitBase->IsDragged = true;
			SpawnPlatform->SetEnergy(SpawnPlatform->Energy-UnitToDrag->EnergyCost);
		}
	}
}

void AExtendedControllerBase::DropUnitBase()
{
	AUnitSpawnPlatform* CurrentSpawnPlatform = Cast<AUnitSpawnPlatform>(CurrentDraggedGround);
	if(CurrentDraggedUnitBase && CurrentDraggedUnitBase->IsOnPlattform && !CurrentSpawnPlatform && SpawnPlatform)
	{
		CurrentDraggedUnitBase->IsOnPlattform = false;
		CurrentDraggedUnitBase->IsDragged = false;
		CurrentDraggedUnitBase->SetUnitState(UnitData::PatrolRandom);
		CurrentDraggedUnitBase = nullptr;
	}else if(CurrentDraggedUnitBase && CurrentDraggedUnitBase->IsOnPlattform && SpawnPlatform)
	{
		SpawnPlatform->SetEnergy(SpawnPlatform->Energy+CurrentDraggedUnitBase->EnergyCost);
		CurrentDraggedUnitBase->IsDragged = false;
		CurrentDraggedUnitBase->SetUnitState(UnitData::Idle);
		CurrentDraggedUnitBase = nullptr;
	}
}

void AExtendedControllerBase::DestroyWorkAreaOnServer_Implementation(AWorkArea* WorkArea)
{
	if (WorkArea) WorkArea->Destroy();;
}

void AExtendedControllerBase::DestroyWorkArea()
{
	// Use Hit (FHitResult) and GetHitResultUnderCursor
	// If you hit AWorkArea with WorkArea->Type == BuildArea
	// Destory the WorkArea
	
	 FHitResult HitResult;

	// Get the hit result under the cursor
	if (GetHitResultUnderCursor(ECC_Visibility, false, HitResult))
	{
		// Check if the hit actor is of type AWorkArea
		AWorkArea* WorkArea = Cast<AWorkArea>(HitResult.GetActor());

		if (WorkArea && WorkArea->Type == WorkAreaData::BuildArea && WorkArea->TeamId == SelectableTeamId) // Assuming EWorkAreaType is an enum with 'BuildArea'
		{
			// Destroy the WorkArea
			//WorkArea->Destroy();
			DestroyWorkAreaOnServer(WorkArea);
		}
	}
	
}

void AExtendedControllerBase::CancelAbilitiesIfNoBuilding(AUnitBase* Unit)
{

		if (Unit)
		{
			if (Unit->CurrentSnapshot.AbilityClass)
			{
				UGameplayAbilityBase* AbilityCDO = Unit->CurrentSnapshot.AbilityClass->GetDefaultObject<UGameplayAbilityBase>();
				if (AbilityCDO && AbilityCDO->AbilityCanBeCanceled)
				{
					ABuildingBase* BuildingBase = Cast<ABuildingBase>(Unit);
					
					if (!BuildingBase || BuildingBase->CanMove)
						CancelCurrentAbility(Unit);
				}
			}
		}
}

void AExtendedControllerBase::DestroyDraggedArea_Implementation(AWorkingUnitBase* Worker)
{
	if(!Worker->CurrentDraggedWorkArea)
	{
		UE_LOG(LogTemp, Error, TEXT("DraggedArea is null! Cannot Destroy."));
		return;
	}

	Worker->CurrentDraggedWorkArea->PlannedBuilding = true;
	Worker->CurrentDraggedWorkArea->ControlTimer = 0.f;
	Worker->CurrentDraggedWorkArea->RemoveAreaFromGroup();
	Worker->CurrentDraggedWorkArea->Destroy();
	Worker->CurrentDraggedWorkArea = nullptr;
	AUnitBase* Unit = Cast<AUnitBase>(Worker);
	
	if (Unit && Unit->IsWorker)
	{
		CancelCurrentAbility(Unit);
	}
}

void AExtendedControllerBase::StopWork_Implementation(AWorkingUnitBase* Worker)
{
	if(Worker && Worker->GetUnitState() == UnitData::Build && Worker->BuildArea)
	{
		Worker->BuildArea->StartedBuilding = false;
		Worker->BuildArea->PlannedBuilding = false;
		Worker->BuildArea->RemoveWorkerFromArray(Worker);
		AResourceGameMode* ResourceGameMode = Cast<AResourceGameMode>(GetWorld()->GetAuthGameMode());

		if(ResourceGameMode)
		{
			ResourceGameMode->ModifyResource(EResourceType::Primary, Worker->TeamId, Worker->BuildArea->ConstructionCost.PrimaryCost);
			ResourceGameMode->ModifyResource(EResourceType::Secondary, Worker->TeamId, Worker->BuildArea->ConstructionCost.SecondaryCost);
			ResourceGameMode->ModifyResource(EResourceType::Tertiary, Worker->TeamId, Worker->BuildArea->ConstructionCost.TertiaryCost);
			ResourceGameMode->ModifyResource(EResourceType::Rare, Worker->TeamId, Worker->BuildArea->ConstructionCost.RareCost);
			ResourceGameMode->ModifyResource(EResourceType::Epic, Worker->TeamId, Worker->BuildArea->ConstructionCost.EpicCost);
			ResourceGameMode->ModifyResource(EResourceType::Legendary, Worker->TeamId, Worker->BuildArea->ConstructionCost.LegendaryCost);
		}
	}
}

void AExtendedControllerBase::StopWorkOnSelectedUnit()
{
	if(SelectedUnits.Num() && SelectedUnits[0])
	{
		AWorkingUnitBase* Worker = Cast<AWorkingUnitBase>(SelectedUnits[0]);
		StopWork(Worker);
	}
}


void AExtendedControllerBase::SelectUnitsWithTag_Implementation(FGameplayTag Tag, int TeamId)
{
	UE_LOG(LogTemp, Warning, TEXT("!!!SelectUnitsWithTag_Implementation!!!!"));
	if(!RTSGameMode || !RTSGameMode->AllUnits.Num()) return;

	AbilityArrayIndex = 0;
	
	TArray<AUnitBase*> NewSelection;
	for (int32 i = 0; i < RTSGameMode->AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(RTSGameMode->AllUnits[i]);
		if (Unit && Unit->CanBeSelected && Unit->GetUnitState() != UnitData::Dead && Unit->UnitTags.HasAnyExact(FGameplayTagContainer(Tag)) && Unit->TeamId == TeamId)
		{
			NewSelection.Add(Unit);
		}
	}

	// Check if there are units to sort
	if (NewSelection.Num() > 1)
	{
		// Sort the NewSelection array by tag
		NewSelection.Sort([Tag](const AUnitBase& A, const AUnitBase& B) -> bool
		{
			// Example Sorting Logic:
			// If both units have the tag, sort by unit name
			// Otherwise, prioritize units that have the tag
			bool AHasTag = A.UnitTags.HasTag(Tag);
			bool BHasTag = B.UnitTags.HasTag(Tag);

			if (AHasTag && BHasTag)
			{
				return A.GetName() < B.GetName();
			}
			if (AHasTag)
			{
				return true; // A comes before B
			}
			if (BHasTag)
			{
				return false; // B comes before A
			}
			return A.GetName() < B.GetName(); // Fallback sorting
		});
	}
	UE_LOG(LogTemp, Warning, TEXT("!!!NewSelection.Num(): %d!!!!"), NewSelection.Num());
	UE_LOG(LogTemp, Warning, TEXT("!!!TeamId: %d!!!!"), TeamId);
	// Update the HUD with the sorted selection

	// Call this on Server
	Client_UpdateHUDSelection(NewSelection, TeamId);
}

void AExtendedControllerBase::Client_UpdateHUDSelection_Implementation(const TArray<AUnitBase*>& NewSelection, int TeamId)
{
	if (SelectableTeamId != TeamId)
	{
		return;
	}
	
	if (!HUDBase)
	{
		return;
	}
	
	HUDBase->DeselectAllUnits();

	for (AUnitBase* NewUnit : NewSelection)
	{
		NewUnit->SetSelected();
		HUDBase->SelectedUnits.Emplace(NewUnit);
	}
	CurrentUnitWidgetIndex = 0;
	SelectedUnits = HUDBase->SelectedUnits;
}

void AExtendedControllerBase::Client_DeselectSingleUnit_Implementation(AUnitBase* UnitToDeselect)
{
	if (!HUDBase || !UnitToDeselect)
	{
		return;
	}

	// Deselect the unit visually
	UnitToDeselect->SetDeselected();

	// Remove the unit from the HUD's selected units array
	HUDBase->SelectedUnits.Remove(UnitToDeselect);

	// Update the controller's selected units
	SelectedUnits = HUDBase->SelectedUnits;
}

void AExtendedControllerBase::AddToCurrentUnitWidgetIndex(int Add)
{
	const int NumUnits = SelectedUnits.Num();
	if (NumUnits == 0 || !SelectedUnits.IsValidIndex(CurrentUnitWidgetIndex))
	{
		return;
	}

	const int StartIndex = CurrentUnitWidgetIndex;
	if (!SelectedUnits[StartIndex]) return;
	const FGameplayTag CurrentTag = SelectedUnits[StartIndex]->AbilitySelectionTag;

	int visited = 0;       // how many units (including empty‑ability ones) we've stepped past
	int defaultHits = 0;   // how many times we've landed on a unit with DefaultAbilities.Num() > 0

	// Single loop over at most 'NumUnits' steps:
	while (visited < NumUnits)
	{
		// advance & wrap
		CurrentUnitWidgetIndex = (CurrentUnitWidgetIndex + Add + NumUnits) % NumUnits;

		// if this unit has no default abilities, skip it
		if (!SelectedUnits[CurrentUnitWidgetIndex] || SelectedUnits[CurrentUnitWidgetIndex]->DefaultAbilities.Num() == 0)
		{
			visited++;
			continue;
		}

		// we found one with at least DefaultAbilities
		defaultHits++;

		// if the tag changed, or we looped all the way back to where we started, stop here
		if (SelectedUnits[CurrentUnitWidgetIndex] && (SelectedUnits[CurrentUnitWidgetIndex]->AbilitySelectionTag != CurrentTag ||
			CurrentUnitWidgetIndex == StartIndex))
		{
			break;
		}

		// otherwise it was the same-tag unit, count it and keep going
		visited++;
	}

	// if we never encountered *any* unit with DefaultAbilities, undo the move
	if (defaultHits == 0)
	{
		CurrentUnitWidgetIndex = StartIndex;
	}
}

void AExtendedControllerBase::SendWorkerToResource_Implementation(AWorkingUnitBase* Worker, AWorkArea* WorkArea)
{
	if (!Worker || !Worker->IsWorker) return;
	
	Worker->ResourcePlace = WorkArea;
	Worker->SetUnitState(UnitData::GoToResourceExtraction);
	Worker->SwitchEntityTagByState(UnitData::GoToResourceExtraction, Worker->UnitStatePlaceholder);

}

void AExtendedControllerBase::SendWorkerToWorkArea_Implementation(AWorkingUnitBase* Worker, AWorkArea* WorkArea)
{

	if (!Worker || !Worker->IsWorker) return;
	
	Worker->BuildArea = WorkArea;
	// Check if the worker is overlapping with the build area
	if (Worker->IsOverlappingActor(Worker->BuildArea))
	{
		// If they are overlapping, set the state to 'Build'
		Worker->SetUnitState(UnitData::Build);
		Worker->SwitchEntityTagByState(UnitData::Build, Worker->UnitStatePlaceholder);
	}
	else
	{
		// If they are not overlapping, set the state to 'GoToBuild'
		Worker->SetUnitState(UnitData::GoToBuild);
		Worker->SwitchEntityTagByState(UnitData::GoToBuild, Worker->UnitStatePlaceholder);
	}
}

void AExtendedControllerBase::LoadUnits_Implementation(const TArray<AUnitBase*>& UnitsToLoad, AUnitBase* Transporter)
{
		if (Transporter && Transporter->IsATransporter) // Transporter->IsATransporter
		{
			// Assign a non-zero TransportId to the clicked transporter (stable identifier)
			if (Transporter->TransportId == 0)
			{
				Transporter->TransportId = Transporter->GetUniqueID();
			}

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
					// Bind this unit to the clicked transporter so it won't load into others en route
					UnitsToLoad[i]->TransportId = Transporter->TransportId;
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

				
					RightClickRunUEPF(UnitsToLoad[i], UnitsToLoad[i]->RunLocation, true);
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

bool AExtendedControllerBase::CheckClickOnTransportUnit(FHitResult Hit_Pawn)
{
	if (!Hit_Pawn.bBlockingHit) return false;


		AActor* HitActor = Hit_Pawn.GetActor();
		
		AUnitBase* UnitBase = Cast<AUnitBase>(HitActor);
	
		LoadUnits(SelectedUnits, UnitBase);
	
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


bool AExtendedControllerBase::CheckClickOnWorkArea(FHitResult Hit_Pawn)
{
	StopWorkOnSelectedUnit();
	
	if (Hit_Pawn.bBlockingHit && HUDBase)
	{
		AActor* HitActor = Hit_Pawn.GetActor();

		ABuildingBase* Base = Cast<ABuildingBase>(HitActor);
		if (Base && Base->IsBase)
		{
			for (int32 i = 0; i < SelectedUnits.Num(); i++) {
				if (SelectedUnits[i] && SelectedUnits[i]->IsWorker)
				{
					SelectedUnits[i]->RemoveFocusEntityTarget();
					SelectedUnits[i]->Base = Base;
					SelectedUnits[i]->SetUnitState(UnitData::GoToBase);
					//SelectedUnits[i]->SwitchEntityTag(FMassStateGoToBaseTag::StaticStruct());
					SelectedUnits[i]->SwitchEntityTagByState(UnitData::GoToBase, SelectedUnits[i]->UnitStatePlaceholder);
				}
			}
			return true;
		}
		
		AWorkArea* WorkArea = Cast<AWorkArea>(HitActor);

		if(WorkArea && !WorkArea->IsNoBuildZone)
		{
			TEnumAsByte<WorkAreaData::WorkAreaType> Type = WorkArea->Type;
		
			bool isResourceExtractionArea = Type == WorkAreaData::Primary || Type == WorkAreaData::Secondary || 
									 Type == WorkAreaData::Tertiary || Type == WorkAreaData::Rare ||
									 Type == WorkAreaData::Epic || Type == WorkAreaData::Legendary;


			if(WorkArea && isResourceExtractionArea)
			{
				for (int32 i = 0; i < SelectedUnits.Num(); i++)
				{
					if (SelectedUnits[i] && SelectedUnits[i]->UnitState != UnitData::Dead)
					{
						
						AWorkingUnitBase* Worker = Cast<AWorkingUnitBase>(SelectedUnits[i]);
						Worker->RemoveFocusEntityTarget();
						SendWorkerToResource(Worker, WorkArea);
					}
				}
			} else if(WorkArea && WorkArea->Type == WorkAreaData::BuildArea)
			{
				int NumberSended = 0;
				for (int32 i = 0; i < SelectedUnits.Num() && WorkArea->MaxWorkerCount > NumberSended; i++) {
					if (SelectedUnits[i]->IsWorker)
					{
						AWorkingUnitBase* Worker = Cast<AWorkingUnitBase>(SelectedUnits[i]);
						if(Worker && (Worker->TeamId == WorkArea->TeamId || WorkArea->TeamId == 0))
						{
							Worker->RemoveFocusEntityTarget();
							SendWorkerToWorkArea(Worker, WorkArea);
							NumberSended++;
						}
					}
				}
			}

			if(WorkArea) return true;
		}
		
	}

	return false;
}

void AExtendedControllerBase::CastEndsEvent(AUnitBase* UnitBase)
{
	if (!UnitBase) return;

	// Ensure this logic runs only on the server
	if (!UnitBase->HasAuthority()) return;
	
		FHitResult Hit;
		GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, false, Hit);
		if (UnitBase->ActivatedAbilityInstance)
		{
			UnitBase->ActivatedAbilityInstance->OnAbilityCastComplete(Hit);
		}
}

// --- Squad selection: forward to server in PC and apply on client ---
void AExtendedControllerBase::Server_SelectUnitsFromSameSquad_Implementation(AUnitBase* SelectedUnit)
{
	UE_LOG(LogTemp, Log, TEXT("[PC][Server_SelectUnitsFromSameSquad] Called. Controller=%s HasAuthority=%d Selected=%s"),
		*GetName(), HasAuthority(), SelectedUnit ? *SelectedUnit->GetName() : TEXT("NULL"));

	if (!SelectedUnit)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PC][Server_SelectUnitsFromSameSquad] SelectedUnit is NULL"));
		return;
	}

	ARTSGameModeBase* GameMode = Cast<ARTSGameModeBase>(GetWorld()->GetAuthGameMode());
	if (!GameMode)
	{
		UE_LOG(LogTemp, Error, TEXT("[PC][Server_SelectUnitsFromSameSquad] GameMode is NULL"));
		return;
	}

	const int32 SquadId = SelectedUnit->SquadId;
	TArray<AUnitBase*> UnitsToSelect;

	if (SquadId == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PC][Server_SelectUnitsFromSameSquad] SelectedUnit has no SquadId (0)"));
	}
	else
	{
		for (int32 i = 0; i < GameMode->AllUnits.Num(); ++i)
		{
			AUnitBase* Unit = Cast<AUnitBase>(GameMode->AllUnits[i]);
			if (Unit && Unit->SquadId == SquadId)
			{
				// Optional team gate: only same team as this controller, unless controller TeamId == 0 (spectator/admin)
				if (Unit->TeamId == SelectableTeamId || SelectableTeamId == 0)
				{
					UnitsToSelect.Emplace(Unit);
				}
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[PC][Server_SelectUnitsFromSameSquad] SquadId=%d UnitsFound=%d ControllerTeam=%d"), SquadId, UnitsToSelect.Num(), SelectableTeamId);
	Client_SelectUnitsFromSameSquad(UnitsToSelect);
}

void AExtendedControllerBase::Client_SelectUnitsFromSameSquad_Implementation(const TArray<AUnitBase*>& Units)
{
	AHUDBase* HUD = Cast<AHUDBase>(GetHUD());
	UE_LOG(LogTemp, Log, TEXT("[PC][Client_SelectUnitsFromSameSquad] Applying selection on client. HUD=%s Units=%d"), HUD ? *HUD->GetName() : TEXT("NULL"), Units.Num());
	if (!HUD)
	{
		return;
	}
	for (AUnitBase* Unit : Units)
	{
		if (!Unit) continue;
		Unit->SetSelected();
		HUD->SelectedUnits.AddUnique(Unit);
		UE_LOG(LogTemp, Verbose, TEXT("[PC][Client_SelectUnitsFromSameSquad] Selected %s (Team=%d Squad=%d)"), *Unit->GetName(), Unit->TeamId, Unit->SquadId);
	}
}


void AExtendedControllerBase::UpdateExtractionSounds(float DeltaSeconds)
{
	if (!IsLocalController()) return;

	FVector CameraLocation = FVector::ZeroVector;
	FRotator CameraRotation = FRotator::ZeroRotator;
	GetPlayerViewPoint(CameraLocation, CameraRotation);

	float SoundMultiplier = GetSoundMultiplier();

	// Mapping to store active work area per resource type for this frame
	TMap<EResourceType, AWorkArea*> ActiveAreas;
	TMap<EResourceType, float> FadeOutDurations;

	// Iterate through all WorkAreas in the level
	for (TActorIterator<AWorkArea> It(GetWorld()); It; ++It)
	{
		AWorkArea* Area = *It;
		if (!Area) continue;

		EResourceType ResType = Area->ConvertWorkAreaTypeToResourceType(Area->Type);
		if (ResType == EResourceType::MAX) continue;

		FadeOutDurations.Add(ResType, Area->ExtractionSoundFadeOutDuration);

		if (ActiveAreas.Contains(ResType)) continue;

		bool bHasExtractingWorker = false;
		for (AWorkingUnitBase* Worker : Area->Workers)
		{
			if (Worker && Worker->GetUnitState() == UnitData::ResourceExtraction)
			{
				bHasExtractingWorker = true;
				break;
			}
		}

		if (bHasExtractingWorker)
		{
			float Distance = FVector::Dist(CameraLocation, Area->GetActorLocation());
			bool bIsClose = Distance <= ExtractionSoundDistance;

			bool bIsOnScreen = false;
			FVector2D ScreenPosition;
			if (ProjectWorldLocationToScreen(Area->GetActorLocation(), ScreenPosition))
			{
				bIsOnScreen = true;
			}

			if (bIsClose || bIsOnScreen)
			{
				ActiveAreas.Add(ResType, Area);
			}
		}
	}

	// Now handle the audio components
	for (uint8 i = 0; i < (uint8)EResourceType::MAX; ++i)
	{
		EResourceType Type = (EResourceType)i;
		AWorkArea* ActiveArea = ActiveAreas.FindRef(Type);
		UAudioComponent* AudioComp = ExtractionAudioComponents.FindRef(Type);

		if (ActiveArea && ActiveArea->ExtractionSound)
		{
			float FinalVolume = SoundMultiplier * ActiveArea->ExtractionSoundVolume;

			if (!IsValid(AudioComp))
			{
				AudioComp = UGameplayStatics::SpawnSound2D(this, ActiveArea->ExtractionSound, 0.f, 1.f, 0.f, nullptr, false, true);
				if (AudioComp)
				{
					ExtractionAudioComponents.Add(Type, AudioComp);
					AudioComp->FadeIn(ExtractionSoundFadeInDuration, FinalVolume);
				}
			}
			else
			{
				if (AudioComp->Sound != ActiveArea->ExtractionSound)
				{
					AudioComp->FadeOut(ActiveArea->ExtractionSoundFadeOutDuration, 0.f);
					AudioComp = UGameplayStatics::SpawnSound2D(this, ActiveArea->ExtractionSound, 0.f, 1.f, 0.f, nullptr, false, true);
					ExtractionAudioComponents.Add(Type, AudioComp);
					AudioComp->FadeIn(ExtractionSoundFadeInDuration, FinalVolume);
				}
				else if (!AudioComp->IsPlaying())
				{
					AudioComp->FadeIn(ExtractionSoundFadeInDuration, FinalVolume);
				}
				else
				{
					AudioComp->SetVolumeMultiplier(FinalVolume);
				}
			}
		}
		else if (IsValid(AudioComp) && AudioComp->IsPlaying())
		{
			float FadeOutDuration = FadeOutDurations.Contains(Type) ? FadeOutDurations[Type] : 1.0f;
			AudioComp->FadeOut(FadeOutDuration, 0.f);
			ExtractionAudioComponents.Remove(Type);
		}
	}
}

// Server RPC to mirror ability indicator overlap state from client to server
void AExtendedControllerBase::Server_SetIndicatorOverlap_Implementation(AAbilityIndicator* Indicator, bool bOverlapping)
{
	if (!Indicator)
	{
		return;
	}
	Indicator->IsOverlappedWithNoBuildZone = bOverlapping;
}
