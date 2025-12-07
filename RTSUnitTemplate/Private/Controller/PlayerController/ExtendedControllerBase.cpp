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
	MoveAbilityIndicator_Implementation(DeltaSeconds);
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
		UnitBase->ActivateAbilityByInputID(InputID, UnitBase->DefaultAbilities, HitResult);
	}
	
}

void AExtendedControllerBase::ActivateSecondAbilities_Implementation(AGASUnit* UnitBase, EGASAbilityInputID InputID, const FHitResult& HitResult)
{
	if (UnitBase && UnitBase->SecondAbilities.Num() > 0)
	{
		UnitBase->ActivateAbilityByInputID(InputID, UnitBase->SecondAbilities, HitResult);
	}
}

void AExtendedControllerBase::ActivateThirdAbilities_Implementation(AGASUnit* UnitBase, EGASAbilityInputID InputID, const FHitResult& HitResult)
{
	if (UnitBase && UnitBase->ThirdAbilities.Num() > 0)
	{
		UnitBase->ActivateAbilityByInputID(InputID, UnitBase->ThirdAbilities, HitResult);
	}
}

void AExtendedControllerBase::ActivateFourthAbilities_Implementation(AGASUnit* UnitBase, EGASAbilityInputID InputID, const FHitResult& HitResult)
{
	if (UnitBase && UnitBase->FourthAbilities.Num() > 0)
	{
		UnitBase->ActivateAbilityByInputID(InputID, UnitBase->FourthAbilities, HitResult);
	}
}

void AExtendedControllerBase::ActivateAbilities_Implementation(AGASUnit* UnitBase, EGASAbilityInputID InputID, const TArray<TSubclassOf<UGameplayAbilityBase>>& Abilities)
{
	if (UnitBase && Abilities.Num() > 0)
	{
		UnitBase->ActivateAbilityByInputID(InputID, Abilities);
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
		UGameplayStatics::PlaySound2D(this, AbilitySound);
	}
	
	bool bActivatedAny = false;
	if (SelectedUnits.Num() > 0)
	{
		if (SelectedUnits[CurrentUnitWidgetIndex]->IsWorker && SelectedUnits[CurrentUnitWidgetIndex]->CanActivateAbilities)
		{
			ActivateAbilitiesByIndex_Implementation(SelectedUnits[CurrentUnitWidgetIndex], InputID, Hit);
			bActivatedAny = true;
			HUDBase->SetUnitSelected(SelectedUnits[CurrentUnitWidgetIndex]);
			CurrentUnitWidgetIndex = 0;
			SelectedUnits = HUDBase->SelectedUnits;
		}else{
			bool bAnyHasTag = false;

			
			for (AUnitBase* SelectedUnit : SelectedUnits)
			{
				if (SelectedUnit && SelectedUnit->CanActivateAbilities && !SelectedUnit->IsWorker &&  SelectedUnit->AbilitySelectionTag == SelectedUnits[CurrentUnitWidgetIndex]->AbilitySelectionTag)
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

void AExtendedControllerBase::SetWorkAreaPosition_Implementation(AWorkArea* DraggedArea, FVector NewActorPosition)
{
   if (!DraggedArea) return;
	
    UStaticMeshComponent* MeshComponent = DraggedArea->FindComponentByClass<UStaticMeshComponent>();
    if (!MeshComponent)
    {
        DraggedArea->SetActorLocation(NewActorPosition);
        return;
    }

    // 1) Zunächst nur XY setzen, Z bleibt unverändert (oder so, wie es reinkommt)
    FVector TempLocation = FVector(NewActorPosition.X, NewActorPosition.Y, NewActorPosition.Z);
    DraggedArea->SetActorLocation(TempLocation);
	
    // 2) Bounds neu holen (jetzt mit richtiger Weltposition)
    FBoxSphereBounds MeshBounds = MeshComponent->Bounds;
    // „halbe Höhe“ des Meshes (inkl. Scale & Rotation, da in Weltkoordinaten)
    float HalfHeight = MeshBounds.BoxExtent.Z;

	/*
    // Nur zum Debuggen
    DrawDebugBox(
        GetWorld(),
        MeshBounds.Origin,
        MeshBounds.BoxExtent,
        FColor::Yellow,
        false,  // bPersistentLines
        5.0f    // Lebensdauer in Sekunden
    );
    DrawDebugPoint(
        GetWorld(),
        MeshBounds.Origin,
        20.f,
        FColor::Green,
        false,
        5.0f
    );*/

    // 3) LineTrace von etwas über dem Bodensatz des Meshes nach unten
    FVector MeshBottomWorld = MeshBounds.Origin - FVector(0.f, 0.f, HalfHeight);
    FVector TraceStart = MeshBottomWorld + FVector(0.f, 0.f, 1000.f);
    FVector TraceEnd   = MeshBottomWorld - FVector(0.f, 0.f, 5000.f);

    FHitResult HitResult;
    FCollisionQueryParams TraceParams(FName(TEXT("WorkAreaGroundTrace")), true, DraggedArea);
    //TraceParams.AddIgnoredActor(DraggedArea);
	for (TActorIterator<AWorkArea> It(GetWorld()); It; ++It)
	{
		TraceParams.AddIgnoredActor(*It);
	}

    if (GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, TraceParams))
    {
    	/*
        // Debug: Zeichne die Tracelinie
        DrawDebugLine(
            GetWorld(),
            TraceStart,
            HitResult.ImpactPoint,
            FColor::Green,
            false,
            5.f
        );
        DrawDebugLine(
            GetWorld(),
            HitResult.ImpactPoint,
            TraceEnd,
            FColor::Red,
            false,
            5.f
        );
*/
        // Setze die neue Z-Position so, dass MeshBottomWorld auf dem Boden liegt
        float DeltaZ = HitResult.ImpactPoint.Z - MeshBottomWorld.Z;
        NewActorPosition.Z += DeltaZ;
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("SetWorkAreaPosition_Implementation: Kein Boden getroffen!"));
    }
	
    // 4) Schlussendlich Actor anpassen
    DraggedArea->SetActorLocation(NewActorPosition);
	DraggedArea->ForceNetUpdate();
}

void AExtendedControllerBase::Server_FinalizeWorkAreaPosition_Implementation(AWorkArea* DraggedArea, FVector NewActorPosition)
{
	if (!DraggedArea) return;
	// Compute grounded position on the server to ensure mesh bottom sits on ground
	const FVector Grounded = ComputeGroundedLocation(DraggedArea, NewActorPosition);
	DraggedArea->SetActorLocation(Grounded);
	DraggedArea->ForceNetUpdate();

	// Inform all clients (legacy behavior)
	Multicast_ApplyWorkAreaPosition(DraggedArea, Grounded);

	// Additionally, explicitly update clients that share the same TeamId as the WorkArea
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
				ExtPC->Client_UpdateWorkAreaPosition(DraggedArea, Grounded);
			}
		}
	}
}

void AExtendedControllerBase::Multicast_ApplyWorkAreaPosition_Implementation(AWorkArea* DraggedArea, FVector NewActorPosition)
{
	if (!DraggedArea) return;
	DraggedArea->SetActorLocation(NewActorPosition);
}

void AExtendedControllerBase::Client_UpdateWorkAreaPosition_Implementation(AWorkArea* DraggedArea, FVector NewActorPosition)
{
	if (!DraggedArea) return;
	DraggedArea->SetActorLocation(NewActorPosition);
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

	// Trace straight down from above the desired XY to find ground
	const FVector TraceStart = FVector(DesiredLocation.X, DesiredLocation.Y, DesiredLocation.Z + 2000.f);
	const FVector TraceEnd   = FVector(DesiredLocation.X, DesiredLocation.Y, DesiredLocation.Z - 10000.f);

	FHitResult Hit;
	FCollisionQueryParams Params(FName(TEXT("WorkArea_GroundTrace")), true, DraggedArea);
	// Ignore all WorkAreas so we hit world
	for (TActorIterator<AWorkArea> It(DraggedArea->GetWorld()); It; ++It)
	{
		Params.AddIgnoredActor(*It);
	}

	FVector Result = DesiredLocation;
	if (DraggedArea->GetWorld() && DraggedArea->GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params))
	{
		// Place so that mesh bottom rests exactly on the hit surface
		Result.Z = Hit.ImpactPoint.Z - OffsetActorToBottom;
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
                const FVector TraceEnd = TraceStart + MouseDir * 5000.f;
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
    DraggedActor->SetActorLocation(GroundedSnap);

    // ---- Collision Check via BoxOverlapActors (ignoring both actors) ----

    // 1. Create an array of object types to test against
    TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;

    // 2. Create an array of actors to ignore
    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(DraggedActor);
    ActorsToIgnore.Add(OtherActor);

    // 3. We'll store any overlapping actors here
    TArray<AActor*> OverlappingActors;

    // 4. Perform the box overlap at the snapped position using the dragged actor's half-extents.
    bool bSuccess = UKismetSystemLibrary::BoxOverlapActors(
        GetWorld(),
        SnappedActorLocation,       // Box center at the actor pivot -> we want center; approximate using actor loc, same as placement
        DraggedExtent,              // Half-extent (X, Y, Z)
        TArray<TEnumAsByte<EObjectTypeQuery>>(),
        nullptr,          // (Optional) Class filter
        ActorsToIgnore,
        OverlappingActors
    );

    bool bAnyOverlap = false;

    if (bSuccess)
    {
        for (AActor* Overlapped : OverlappingActors)
        {
            if (!Overlapped || Overlapped == DraggedActor || Overlapped == OtherActor || Overlapped->IsA(ALandscape::StaticClass()))
                continue;

            if (Cast<AWorkArea>(Overlapped) || Cast<ABuildingBase>(Overlapped))
            {
                bAnyOverlap = true;
            }
        }
    }

    WorkAreaIsSnapped = !bAnyOverlap;
    if (WorkAreaIsSnapped)
    {
        const bool bWasDifferentTarget = (CurrentSnapActor != OtherActor);
        CurrentSnapActor = OtherActor;
        if (bWasDifferentTarget)
        {
            // Lock new snaps for the cooldown duration
            const float NowLocal = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f);
            NextAllowedSnapTime = NowLocal + FMath::Max(0.f, SnapCooldownSeconds);
        }
    }
    else if (CurrentSnapActor == OtherActor)
    {
        CurrentSnapActor = nullptr;
    }
}


void AExtendedControllerBase::MoveWorkArea_Local(float DeltaSeconds)
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
	
	SelectedUnits[0]->ShowWorkAreaIfNoFog(DraggedWorkArea);
	
    FVector MousePosition, MouseDirection;
    DeprojectMousePositionToWorld(MousePosition, MouseDirection);

    // Raycast from the mouse into the scene
    FVector Start = MousePosition;
    FVector End   = Start + MouseDirection * 5000.f;

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
        const FVector GroundedPos = ComputeGroundedLocation(DraggedWorkArea, NewActorPosition);
        DraggedWorkArea->SetActorLocation(GroundedPos);

        WorkAreaIsSnapped = false;
    }
}


void AExtendedControllerBase::SetWorkArea(FVector AreaLocation)
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
	
	SelectedUnits[0]->ShowWorkAreaIfNoFog(DraggedWorkArea);

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

void AExtendedControllerBase::MoveAbilityIndicator_Implementation(float DeltaSeconds)
{
    // Sanity check that we have at least one "SelectedUnit"
    if (SelectedUnits.Num() == 0)
    {
        return;
    }
    
    // Build the collision query params and ignore all ability indicators from the selected units
    FCollisionQueryParams CollisionParams;
    CollisionParams.bTraceComplex = true;
    for (auto Unit : SelectedUnits)
    {
        if (Unit && Unit->CurrentDraggedAbilityIndicator)
        {
            CollisionParams.AddIgnoredActor(Unit->CurrentDraggedAbilityIndicator);
        }
    }
    
    FVector MousePosition, MouseDirection;
    DeprojectMousePositionToWorld(MousePosition, MouseDirection);
	
    FVector Start = MousePosition;
    FVector End   = Start + MouseDirection * 5000.f;

    FHitResult HitResult;
    bool bHit = GetWorld()->LineTraceSingleByChannel(
        HitResult,
        Start,
        End,
        ECC_Visibility,
        CollisionParams
    );

    if (bHit)
    {
        for (auto Unit : SelectedUnits)
        {
            if (!Unit)
            {
                continue;
            }
            
            AAbilityIndicator* CurrentIndicator = Unit->CurrentDraggedAbilityIndicator;
            if (!CurrentIndicator)
            {
                continue;
            }

        	FVector ALocation = Unit->GetMassActorLocation();

            FVector Direction = HitResult.Location - ALocation;

        	float Distance = FVector::Dist(HitResult.Location, ALocation);
        	if (Unit)
        	{
        		if (Unit->CurrentSnapshot.AbilityClass)
        		{
        			UGameplayAbilityBase* AbilityCDO = Unit->CurrentSnapshot.AbilityClass->GetDefaultObject<UGameplayAbilityBase>();
        			if (AbilityCDO && Distance > AbilityCDO->Range && AbilityCDO->Range != 0.f)
        			{
        				AbilityIndicatorBlinkTimer = AbilityIndicatorBlinkTimer + DeltaSeconds;
        				if (AbilityIndicatorBlinkTimer > 0.25f)
        				{
        					AbilityIndicatorBlinkTimer = 0.0f; // Reset timer
        					if (Unit->AbilityIndicatorVisibility)
        					{
        						Unit->HideAbilityIndicator(CurrentIndicator);
        					}
					        else
					        {
					        	Unit->ShowAbilityIndicator(CurrentIndicator);
					        }
        				}
        			}else
        			{
        				Unit->ShowAbilityIndicator(CurrentIndicator);
        			}
        		}
        	}
        	
            Direction.Z = 0;

            if (!Direction.IsNearlyZero())
            {
                FRotator NewRotation = Direction.Rotation();
                CurrentIndicator->SetActorRotation(NewRotation);
            }
            
            // Overlap detection, snap like MoveWorkArea, material swap + NavMesh validity check
            if (CurrentIndicator->DetectOverlapWithWorkArea && CurrentIndicator->IndicatorMesh)
            {
                // Determine an overlap radius based on the indicator mesh bounds
                const FBoxSphereBounds MeshBounds = CurrentIndicator->IndicatorMesh->CalcBounds(CurrentIndicator->IndicatorMesh->GetComponentTransform());
                const float OverlapRadius = FMath::Max(50.f, MeshBounds.SphereRadius);

                // Check overlap against WorkAreas
                TArray<AActor*> OverlappedWorkAreas;
                const bool bAnyWA = UKismetSystemLibrary::SphereOverlapActors(
                    this,
                    HitResult.Location,
                    OverlapRadius,
                    TArray<TEnumAsByte<EObjectTypeQuery>>(),
                    AWorkArea::StaticClass(),
                    TArray<AActor*>(),
                    OverlappedWorkAreas
                );

                // Check overlap against Buildings (used for snapping acquisition)
                TArray<AActor*> OverlappedBuildings;
                const bool bAnyBld = UKismetSystemLibrary::SphereOverlapActors(
                    this,
                    HitResult.Location,
                    OverlapRadius,
                    TArray<TEnumAsByte<EObjectTypeQuery>>(),
                    ABuildingBase::StaticClass(),
                    TArray<AActor*>(),
                    OverlappedBuildings
                );

                const bool bOverlapsRelevant = (bAnyWA && OverlappedWorkAreas.Num() > 0) || (bAnyBld && OverlappedBuildings.Num() > 0);

                // Simplified NavMesh check (same approach as MoveWorkArea):
                // Consider location invalid if the mouse hit cannot be projected onto the NavMesh.
                bool bOffNavMesh = true;
                FVector MouseGround = HitResult.Location;
                if (UWorld* World = GetWorld())
                {
                    if (UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World))
                    {
                        FNavLocation NavLoc;
                        if (NavSys->ProjectPointToNavigation(HitResult.Location, NavLoc, FVector(100.f, 100.f, 300.f)))
                        {
                            bOffNavMesh = false;
                            MouseGround = NavLoc.Location;
                        }
                    }
                }

                // Indicator snap logic (like MoveWorkArea) for Building and WorkArea overlaps
                auto ComputeSnappedLocation = [&](AActor* OtherActor, FVector& OutLocation)
                {
                    if (!OtherActor) return false;

                    // Dragged (indicator) bounds
                    const FBoxSphereBounds DraggedBounds = CurrentIndicator->IndicatorMesh->CalcBounds(CurrentIndicator->IndicatorMesh->GetComponentTransform());
                    const FVector DraggedCenter = DraggedBounds.Origin;
                    const FVector DraggedExtent = DraggedBounds.BoxExtent;
                    const FVector CenterToActorOffset = DraggedCenter - CurrentIndicator->GetActorLocation();

                    // Other actor bounds
                    FVector OtherCenter, OtherExtent;
                    if (!GetActorBoundsForSnap(OtherActor, OtherCenter, OtherExtent))
                    {
                        return false;
                    }

                    // Building special handling
                    float EffectiveGap = SnapGap;
                    if (ABuildingBase* TargetBuilding = Cast<ABuildingBase>(OtherActor))
                    {
                        EffectiveGap = FMath::Max(0.f, SnapGap + TargetBuilding->SnapGapAdjustment);
                        if (UCapsuleComponent* Capsule = TargetBuilding->FindComponentByClass<UCapsuleComponent>())
                        {
                            const float R = Capsule->GetScaledCapsuleRadius();
                            OtherExtent.X = R; OtherExtent.Y = R;
                            OtherCenter = TargetBuilding->GetActorLocation();
                        }
                    }

                    // Choose side based on mouse-to-target vector
                    const FVector2D ToTarget(MouseGround.X - OtherCenter.X, MouseGround.Y - OtherCenter.Y);
                    FVector NewCenter = DraggedCenter;
                    if (FMath::Abs(ToTarget.X) >= FMath::Abs(ToTarget.Y))
                    {
                        const float Sign = (ToTarget.X >= 0.f) ? 1.f : -1.f;
                        NewCenter.X = OtherCenter.X + Sign * (OtherExtent.X + DraggedExtent.X + EffectiveGap);
                        NewCenter.Y = OtherCenter.Y;
                    }
                    else
                    {
                        const float Sign = (ToTarget.Y >= 0.f) ? 1.f : -1.f;
                        NewCenter.Y = OtherCenter.Y + Sign * (OtherExtent.Y + DraggedExtent.Y + EffectiveGap);
                        NewCenter.X = OtherCenter.X;
                    }

                    // Convert mesh center back to actor location
                    OutLocation = NewCenter - CenterToActorOffset;
                    return true;
                };

                // Maintain existing snap with release hysteresis
                bool bSnappedHandled = false;
                if (CurrentSnapActor && IsValid(CurrentSnapActor) && !bOffNavMesh)
                {
                    // Compute release threshold based on extents
                    const FBoxSphereBounds DraggedBounds = CurrentIndicator->IndicatorMesh->CalcBounds(CurrentIndicator->IndicatorMesh->GetComponentTransform());
                    const FVector DraggedExtent = DraggedBounds.BoxExtent;
                    FVector OtherCenter, OtherExtent;
                    if (GetActorBoundsForSnap(CurrentSnapActor, OtherCenter, OtherExtent))
                    {
                        const float DragXY = FMath::Max(DraggedExtent.X, DraggedExtent.Y);
                        const float OtherXY = FMath::Max(OtherExtent.X, OtherExtent.Y);
                        const float ReleaseThreshold = DragXY + OtherXY + 150.f;
                        const float MouseToSnapTarget = FVector::Dist2D(MouseGround, OtherCenter);
                        const float NowTime = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f);
                        if (MouseToSnapTarget > ReleaseThreshold)
                        {
                            if (LastBeyondReleaseTime < 0.f) LastBeyondReleaseTime = NowTime;
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
                            FVector SnapLoc;
                            if (ComputeSnappedLocation(CurrentSnapActor, SnapLoc))
                            {
                                // Ground the Z by tracing down
                                FVector Grounded = SnapLoc;
                                // Place bottom on ground similar to below
                                float OffsetActorToBottom = 0.f;
                                const FBoxSphereBounds MeshNow = CurrentIndicator->IndicatorMesh->CalcBounds(CurrentIndicator->IndicatorMesh->GetComponentTransform());
                                const float HalfH = MeshNow.BoxExtent.Z;
                                const float BottomZ = MeshNow.Origin.Z - HalfH;
                                OffsetActorToBottom = BottomZ - CurrentIndicator->GetActorLocation().Z;
                                FHitResult GroundHit;
                                FCollisionQueryParams Params(FName(TEXT("AbilityIndicator_SnapGround")), true, CurrentIndicator);
                                Params.AddIgnoredActor(CurrentIndicator);
                                for (auto UnitIgnore : SelectedUnits)
                                {
                                    if (UnitIgnore && UnitIgnore->CurrentDraggedAbilityIndicator)
                                    {
                                        Params.AddIgnoredActor(UnitIgnore->CurrentDraggedAbilityIndicator);
                                    }
                                }
                                const FVector TraceStart = FVector(SnapLoc.X, SnapLoc.Y, SnapLoc.Z + 2000.f);
                                const FVector TraceEnd   = FVector(SnapLoc.X, SnapLoc.Y, SnapLoc.Z - 10000.f);
                                if (GetWorld()->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility, Params))
                                {
                                    Grounded.Z = GroundHit.ImpactPoint.Z - OffsetActorToBottom;
                                }
                                CurrentIndicator->SetActorLocation(Grounded);
                                WorkAreaIsSnapped = true;
                                bSnappedHandled = true;
                            }
                        }
                    }
                    else
                    {
                        CurrentSnapActor = nullptr;
                        WorkAreaIsSnapped = false;
                    }
                }

                // If not snapped, try to acquire a snap target by box-overlap
                if (!bSnappedHandled && !bOffNavMesh)
                {
                    // Prefer buildings/workareas at the current location
                    TArray<AActor*> Overlapped;
                    TArray<AActor*> Ignore;
                    Ignore.Add(CurrentIndicator);
                    const FVector DragExtent = MeshBounds.BoxExtent;
                    bool bAnyOverlap = UKismetSystemLibrary::BoxOverlapActors(
                        this,
                        MouseGround,
                        DragExtent,
                        TArray<TEnumAsByte<EObjectTypeQuery>>(),
                        AActor::StaticClass(),
                        Ignore,
                        Overlapped
                    );

                    AActor* Candidate = nullptr;
                    float BestDist = TNumericLimits<float>::Max();
                    if (bAnyOverlap)
                    {
                        for (AActor* A : Overlapped)
                        {
                            if (!A || A == CurrentIndicator) continue;
                            if (!(Cast<AWorkArea>(A) || Cast<ABuildingBase>(A))) continue;
                            const float D = FVector::Dist2D(MouseGround, A->GetActorLocation());
                            if (D < BestDist)
                            {
                                BestDist = D;
                                Candidate = A;
                            }
                        }
                    }

                    if (Candidate)
                    {
                        // Hysteresis acquire distance smaller than release
                        FVector OtherCenter, OtherExtent;
                        if (GetActorBoundsForSnap(Candidate, OtherCenter, OtherExtent))
                        {
                            const float DragXY = FMath::Max(DragExtent.X, DragExtent.Y);
                            const float OtherXY = FMath::Max(OtherExtent.X, OtherExtent.Y);
                            const float ReleaseThreshold = DragXY + OtherXY + 150.f;
                            const float AcquireThreshold = ReleaseThreshold * AcquireHysteresisFactor;
                            const float MouseToCandidate = FVector::Dist2D(MouseGround, OtherCenter);
                            const float NowLocal = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f);
                            const bool bTryingNewTarget = (CurrentSnapActor != Candidate);
                            if ((!bTryingNewTarget || NowLocal >= NextAllowedSnapTime) && MouseToCandidate <= AcquireThreshold)
                            {
                                CurrentSnapActor = Candidate;
                                NextAllowedSnapTime = NowLocal + FMath::Max(0.f, SnapCooldownSeconds);

                                FVector SnapLoc;
                                if (ComputeSnappedLocation(Candidate, SnapLoc))
                                {
                                    // Ground the Z
                                    FVector Grounded = SnapLoc;
                                    float OffsetActorToBottom = 0.f;
                                    const FBoxSphereBounds MeshNow = CurrentIndicator->IndicatorMesh->CalcBounds(CurrentIndicator->IndicatorMesh->GetComponentTransform());
                                    const float HalfH = MeshNow.BoxExtent.Z;
                                    const float BottomZ = MeshNow.Origin.Z - HalfH;
                                    OffsetActorToBottom = BottomZ - CurrentIndicator->GetActorLocation().Z;
                                    FHitResult GroundHit;
                                    FCollisionQueryParams Params(FName(TEXT("AbilityIndicator_AcquireGround")), true, CurrentIndicator);
                                    Params.AddIgnoredActor(CurrentIndicator);
                                    for (auto UnitIgnore : SelectedUnits)
                                    {
                                        if (UnitIgnore && UnitIgnore->CurrentDraggedAbilityIndicator)
                                        {
                                            Params.AddIgnoredActor(UnitIgnore->CurrentDraggedAbilityIndicator);
                                        }
                                    }
                                    const FVector TraceStart = FVector(SnapLoc.X, SnapLoc.Y, SnapLoc.Z + 2000.f);
                                    const FVector TraceEnd   = FVector(SnapLoc.X, SnapLoc.Y, SnapLoc.Z - 10000.f);
                                    if (GetWorld()->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility, Params))
                                    {
                                        Grounded.Z = GroundHit.ImpactPoint.Z - OffsetActorToBottom;
                                    }
                                    CurrentIndicator->SetActorLocation(Grounded);
                                    WorkAreaIsSnapped = true;
                                    bSnappedHandled = true;
                                }
                            }
                        }
                    }
                }

                // Update material highlighting based on relevance and nav validity
                const bool bNotAllowed = bOverlapsRelevant || bOffNavMesh; // reuse IsOverlappedWithWorkArea as IsNotAllowed
                if (bNotAllowed != CurrentIndicator->IsOverlappedWithWorkArea)
                {
                    CurrentIndicator->IsOverlappedWithWorkArea = bNotAllowed;
                    if (bNotAllowed)
                    {
                        if (CurrentIndicator->TemporaryHighlightMaterial)
                        {
                            CurrentIndicator->IndicatorMesh->SetMaterial(0, CurrentIndicator->TemporaryHighlightMaterial);
                        }
                    }
                    else
                    {
                        if (CurrentIndicator->OriginalMaterial)
                        {
                            CurrentIndicator->IndicatorMesh->SetMaterial(0, CurrentIndicator->OriginalMaterial);
                        }
                    }

                    // Inform server about the not-allowed state so authoritative logic can use it
                    if (!HasAuthority())
                    {
                        Server_SetIndicatorOverlap(CurrentIndicator, bNotAllowed);
                    }
                }

                // If we handled snapping, skip normal placement for this indicator
                if (bSnappedHandled)
                {
                    continue;
                }
            }

            // Ensure the indicator's mesh bottom sits on the ground at the cursor XY
            {
                const FVector DesiredXY(HitResult.Location.X, HitResult.Location.Y, HitResult.Location.Z);

                // Compute how far below the actor location the mesh bottom currently is
                float OffsetActorToBottom = 0.f;
                if (CurrentIndicator->IndicatorMesh)
                {
                    const FBoxSphereBounds MeshBoundsNow = CurrentIndicator->IndicatorMesh->CalcBounds(CurrentIndicator->IndicatorMesh->GetComponentTransform());
                    const float HalfHeightNow = MeshBoundsNow.BoxExtent.Z;
                    const float CurrentBottomZ = MeshBoundsNow.Origin.Z - HalfHeightNow;
                    OffsetActorToBottom = CurrentBottomZ - CurrentIndicator->GetActorLocation().Z;
                }

                // Trace down to find ground at the desired XY
                FHitResult GroundHit;
                FCollisionQueryParams Params(FName(TEXT("AbilityIndicator_GroundTrace")), true, CurrentIndicator);
                Params.AddIgnoredActor(CurrentIndicator);
                // Ignore all indicators from selected units as well
                for (auto UnitIgnore : SelectedUnits)
                {
                    if (UnitIgnore && UnitIgnore->CurrentDraggedAbilityIndicator)
                    {
                        Params.AddIgnoredActor(UnitIgnore->CurrentDraggedAbilityIndicator);
                    }
                }
                // Additionally, when overlap-detection is active, ignore WorkAreas and Buildings so we always stick to the landscape
                if (CurrentIndicator->DetectOverlapWithWorkArea && CurrentIndicator->IndicatorMesh)
                {
                    for (TActorIterator<AWorkArea> It(GetWorld()); It; ++It)
                    {
                        Params.AddIgnoredActor(*It);
                    }
                    for (TActorIterator<ABuildingBase> ItB(GetWorld()); ItB; ++ItB)
                    {
                        Params.AddIgnoredActor(*ItB);
                    }
                }

                const FVector TraceStart = DesiredXY + FVector(0,0,2000.f);
                const FVector TraceEnd   = DesiredXY - FVector(0,0,10000.f);
                FVector FinalLoc = DesiredXY;
                if (GetWorld()->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility, Params))
                {
                    FinalLoc.Z = GroundHit.ImpactPoint.Z - OffsetActorToBottom;
                }
                CurrentIndicator->SetActorLocation(FinalLoc);
            }
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
		SelectedUnits[0]->ShowWorkAreaIfNoFog(SelectedUnits[0]->CurrentDraggedWorkArea);
		// Get all actors overlapping with the CurrentDraggedWorkArea
		TArray<AActor*> OverlappingActors;
		SelectedUnits[0]->CurrentDraggedWorkArea->GetOverlappingActors(OverlappingActors);
	
		bool bIsOverlappingWithValidArea = false;
		bool IsNoBuildZone = false;
		// Loop through the overlapping actors to check if they are instances of AWorkArea or ABuildingBase
		for (AActor* OverlappedActor : OverlappingActors)
		{
			if (OverlappedActor->IsA(AWorkArea::StaticClass()) || OverlappedActor->IsA(ABuildingBase::StaticClass()))
			{
				AWorkArea* NoBuildZone = Cast<AWorkArea>(OverlappedActor);
				if (NoBuildZone && NoBuildZone->IsNoBuildZone == true) IsNoBuildZone = NoBuildZone->IsNoBuildZone;
				
				bIsOverlappingWithValidArea = true;
				break;
			}
		}
	
		// If overlapping with AWorkArea or ABuildingBase, destroy the CurrentDraggedWorkArea
		if ((bIsOverlappingWithValidArea && !WorkAreaIsSnapped) || IsNoBuildZone) // bIsOverlappingWithValidArea &&
		{
			if (DropWorkAreaFailedSound)
			{
				UGameplayStatics::PlaySound2D(this, DropWorkAreaFailedSound);
			}
			
			/*
			if (RTSGameMode)
			{
				// Cast to your custom game mode class.
				AResourceGameMode* MyGameMode = Cast<AResourceGameMode>(RTSGameMode);
				if (MyGameMode)
				{
					// Use ModifyResource to adjust the resource amount.
					// Here we refund the construction cost to the ability's team.
					MyGameMode->ModifyResourceCCost(SelectedUnits[0]->CurrentDraggedWorkArea->ConstructionCost, SelectedUnits[0]->TeamId);
				}
			}*/
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
				UGameplayStatics::PlaySound2D(this, DropWorkAreaSound);
			}
	
				// Finalize placement on the server now that the player dropped the area
				if (SelectedUnits[0]->CurrentDraggedWorkArea)
				{
					Server_FinalizeWorkAreaPosition(SelectedUnits[0]->CurrentDraggedWorkArea,
						SelectedUnits[0]->CurrentDraggedWorkArea->GetActorLocation());
				}
				SendWorkerToWork(SelectedUnits[0]);
				return true;
		}
	}
 //SelectedUnits[0]->CurrentDraggedWorkArea = nullptr;
	return false;
}

bool AExtendedControllerBase::DropWorkAreaForUnit(AUnitBase* UnitBase, bool bWorkAreaIsSnapped, USoundBase* InDropWorkAreaFailedSound)
{
	if (!UnitBase) return false;

	if (UnitBase->CurrentDraggedWorkArea && UnitBase->CurrentDraggedWorkArea->PlannedBuilding == false)
	{
		UnitBase->ShowWorkAreaIfNoFog(UnitBase->CurrentDraggedWorkArea);

		TArray<AActor*> OverlappingActors;
		UnitBase->CurrentDraggedWorkArea->GetOverlappingActors(OverlappingActors);

		bool bIsOverlappingWithValidArea = false;
		bool bIsNoBuildZone = false;
		for (AActor* OverlappedActor : OverlappingActors)
		{
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

		if ((bIsOverlappingWithValidArea && !bWorkAreaIsSnapped) || bIsNoBuildZone)
		{
			if (InDropWorkAreaFailedSound)
			{
				UGameplayStatics::PlaySound2D(this, InDropWorkAreaFailedSound);
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
				UGameplayStatics::PlaySound2D(this, DropWorkAreaSound);
			}

			if (UnitBase->CurrentDraggedWorkArea)
			{
				Server_FinalizeWorkAreaPosition(UnitBase->CurrentDraggedWorkArea,
					UnitBase->CurrentDraggedWorkArea->GetActorLocation());
			}
			SendWorkerToWork(UnitBase);
			return true;
		}
	}
	return false;
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
		FVector End = Start + MouseDirection * 5000; // Extend to a maximum reasonable distance

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

void AExtendedControllerBase::LeftClickPressed()
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
		AttackToggled = false;
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
					LeftClickAttack(SelectedUnits[i], RunLocation);
					PlayAttackSound = true;
				}
			}
			
			if (SelectedUnits[i])
				FireAbilityMouseHit(SelectedUnits[i], Hit);
		}

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
		for (int32 i = 0; i < SelectedUnits.Num(); i++)
		{
				if (SelectedUnits[i] && SelectedUnits[i]->CurrentSnapshot.AbilityClass && SelectedUnits[i]->CurrentDraggedAbilityIndicator)
				{
						FireAbilityMouseHit(SelectedUnits[i], Hit_Pawn);
						AbilityFired = true;
				}
		}
		
		if (AbilityFired) return;
		
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

void AExtendedControllerBase::LeftClickReleased()
{
	LeftClickIsPressed = false;
	HUDBase->bSelectFriendly = false;
	SelectedUnits = HUDBase->SelectedUnits;
	DropUnitBase();
	int BestIndex = GetHighestPriorityWidgetIndex();
	CurrentUnitWidgetIndex = BestIndex;
	if(Cast<AExtendedCameraBase>(GetPawn())->TabToggled)
	{
		SetWidgets(BestIndex);
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

void AExtendedControllerBase::RightClickPressed()
{
	
	AttackToggled = false;
	FHitResult Hit;
	GetHitResultUnderCursor(ECollisionChannel::ECC_Pawn, false, Hit);
	
	if (!CheckClickOnTransportUnit(Hit))
	{
		if(!SelectedUnits.Num() || !SelectedUnits[0]->CurrentDraggedWorkArea)
		{
			GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, false, Hit);
			if(!CheckClickOnWorkArea(Hit))
			{
				RunUnitsAndSetWaypoints(Hit);
			}
		}
	}


	if (SelectedUnits.Num() && SelectedUnits[0] && SelectedUnits[0]->CurrentDraggedWorkArea)
	{
		DestroyDraggedArea(SelectedUnits[0]);
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
		if (Unit && Unit->GetUnitState() != UnitData::Dead && Unit->UnitTags.HasAnyExact(FGameplayTagContainer(Tag)) && Unit->TeamId == TeamId)
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
	if (HasAuthority())
		Client_UpdateHUDSelection_Implementation(NewSelection, TeamId);
	else
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
	const FGameplayTag CurrentTag = SelectedUnits[StartIndex]->AbilitySelectionTag;

	int visited = 0;       // how many units (including empty‑ability ones) we've stepped past
	int defaultHits = 0;   // how many times we've landed on a unit with DefaultAbilities.Num() > 0

	// Single loop over at most 'NumUnits' steps:
	while (visited < NumUnits)
	{
		// advance & wrap
		CurrentUnitWidgetIndex = (CurrentUnitWidgetIndex + Add + NumUnits) % NumUnits;

		// if this unit has no default abilities, skip it
		if (SelectedUnits[CurrentUnitWidgetIndex]->DefaultAbilities.Num() == 0)
		{
			visited++;
			continue;
		}

		// we found one with at least DefaultAbilities
		defaultHits++;

		// if the tag changed, or we looped all the way back to where we started, stop here
		if (SelectedUnits[CurrentUnitWidgetIndex]->AbilitySelectionTag != CurrentTag ||
			CurrentUnitWidgetIndex == StartIndex)
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

/*
void AExtendedControllerBase::AddToCurrentUnitWidgetIndex(int Add)
{
	if (SelectedUnits.Num() == 0)
	{
		return;
	}
	
	if (!SelectedUnits.IsValidIndex(CurrentUnitWidgetIndex))
    {
    	return;
    }

	bool HasAbilities = false;

	for (int x = 0 ; x < SelectedUnits.Num(); x++)
	{
		if (SelectedUnits[x]->DefaultAbilities.Num())
		{
			HasAbilities = true;
		}
	}

	if (!HasAbilities) return;
	
	int StartIndex = CurrentUnitWidgetIndex;
	FGameplayTag CurrentTag = SelectedUnits[CurrentUnitWidgetIndex]->AbilitySelectionTag;

	int i = 0;
	while (i < SelectedUnits.Num())
	{
		// Move index
		CurrentUnitWidgetIndex += Add;

		// Wrap around
		if (CurrentUnitWidgetIndex < 0)
		{
			CurrentUnitWidgetIndex = SelectedUnits.Num() - 1;
		}
		else if (CurrentUnitWidgetIndex >= SelectedUnits.Num())
		{
			CurrentUnitWidgetIndex = 0;
		}

		if (!SelectedUnits[CurrentUnitWidgetIndex]->DefaultAbilities.Num())
			continue;
		// If the newly selected unit has a different tag, stop
		if (SelectedUnits[CurrentUnitWidgetIndex]->AbilitySelectionTag != CurrentTag)
		{
			break;
		}

		// If we've come back to the original index, stop
		if (CurrentUnitWidgetIndex == StartIndex)
		{
			break;
		}
		i++;
	}
}
*/

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
				if (SelectedUnits[i]->IsWorker)
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


// Server RPC to mirror ability indicator overlap state from client to server
void AExtendedControllerBase::Server_SetIndicatorOverlap_Implementation(AAbilityIndicator* Indicator, bool bOverlapping)
{
	if (!Indicator)
	{
		return;
	}
	Indicator->IsOverlappedWithWorkArea = bOverlapping;
}
