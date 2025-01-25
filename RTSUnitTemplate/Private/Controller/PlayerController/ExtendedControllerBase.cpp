// Fill out your copyright notice in the Description page of Project Settings.


#include "Controller/PlayerController/ExtendedControllerBase.h"

#include "GameplayTagsManager.h"
#include "Landscape.h"
#include "Characters/Camera/ExtendedCameraBase.h"
#include "Characters/Unit/BuildingBase.h"
#include "Kismet/KismetSystemLibrary.h"  
#include "GameModes/ResourceGameMode.h"
#include "Landscape.h" 
#include "AssetTypeActions/AssetDefinition_SoundBase.h"

void AExtendedControllerBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	MoveDraggedUnit_Implementation(DeltaSeconds);
	MoveWorkArea_Implementation(DeltaSeconds);
}

void AExtendedControllerBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	//DOREPLIFETIME(AExtendedControllerBase, CurrentDraggedWorkArea);
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
		NewIndex = MaxAbilityArrayIndex;

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
		NewIndex = 0;


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
			// Check if the unit is valid and has the same TeamId as the camera

			if (Unit && Unit->IsWorker && Unit->TeamId == PlayerTeamId && !Unit->BuildArea) // && !Unit->BuildArea
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

			//ActivateAbilitiesByIndex_Implementation(ClosestUnit, InputID, FHitResult());
			//ActivateDefaultAbilities_Implementation(ClosestUnit, InputID, FHitResult());
			AbilityArrayIndex = 0;
			//OnAbilityInputDetected(InputID, ClosestUnit, ClosestUnit->DefaultAbilities);
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
		// Check if the unit is valid and has the same TeamId as the player
		if (Unit && Unit->IsWorker && Unit->TeamId == PlayerTeamId && !Unit->BuildArea) // && !Unit->BuildArea
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
	
	if (SelectedUnits.Num() > 0)
	{
		if (SelectedUnits[0]->IsWorker)
		{
			ActivateAbilitiesByIndex_Implementation(SelectedUnits[0], InputID, Hit);
			HUDBase->SetUnitSelected(SelectedUnits[0]);
			CurrentUnitWidgetIndex = 0;
			SelectedUnits = HUDBase->SelectedUnits;
			//ActivateAbilitiesByIndex_Implementation(SelectedUnits[0], InputID, Hit);
		}else{
			bool bAnyHasTag = false;

			
			for (AUnitBase* SelectedUnit : SelectedUnits)
			{
				if (SelectedUnit && !SelectedUnit->IsWorker &&  SelectedUnit->AbilitySelectionTag == SelectedUnits[CurrentUnitWidgetIndex]->AbilitySelectionTag)
				{
					bAnyHasTag = true;
					ActivateAbilitiesByIndex_Implementation(SelectedUnit, InputID, Hit);
				}
			}

			if (!bAnyHasTag)
			{
				for (AUnitBase* SelectedUnit : SelectedUnits)
				{
					if (SelectedUnit && !SelectedUnit->IsWorker)
					{
						ActivateAbilitiesByIndex_Implementation(SelectedUnit, InputID, Hit);
					}
				}
			}
			
		}
	}else if (CameraUnitWithTag)
	{
		CameraUnitWithTag->SetSelected();
		SelectedUnits.Emplace(CameraUnitWithTag);
		HUDBase->SetUnitSelected(CameraUnitWithTag);
		ActivateAbilitiesByIndex_Implementation(CameraUnitWithTag, InputID, Hit);
	}
	else if(HUDBase)
	{
		FVector CameraLocation = GetPawn()->GetActorLocation();
		ActivateKeyboardAbilitiesOnCloseUnits(InputID, CameraLocation, SelectableTeamId, HUDBase);
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
    TraceParams.AddIgnoredActor(DraggedArea);

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
/*
void AExtendedControllerBase::SnapToActor(AWorkArea* DraggedActor, AActor* OtherActor)
{
	TSet<AActor*> Visited;
	SnapToActorVisited(DraggedActor, OtherActor, Visited);
}
*/
void AExtendedControllerBase::SnapToActorVisited(AWorkArea* DraggedActor, AActor* OtherActor,
	TSet<AActor*>& AlreadyVisited)
{
	 if (!DraggedActor || !OtherActor || DraggedActor == OtherActor)
        return;

    // Avoid infinite loop if we’ve already tried snapping these two in this chain
    if (AlreadyVisited.Contains(OtherActor))
        return;
    AlreadyVisited.Add(OtherActor);

    // ------------------
    // Phase 1: Compute the would-be snap position
    // ------------------
    UStaticMeshComponent* DraggedMesh = DraggedActor->FindComponentByClass<UStaticMeshComponent>();
    UStaticMeshComponent* OtherMesh   = OtherActor->FindComponentByClass<UStaticMeshComponent>();
    if (!DraggedMesh || !OtherMesh)
        return; // skip if missing meshes

    FBoxSphereBounds DraggedBounds = DraggedMesh->CalcBounds(DraggedMesh->GetComponentTransform());
    FBoxSphereBounds OtherBounds   = OtherMesh->CalcBounds(OtherMesh->GetComponentTransform());

    const FVector DraggedCenter = DraggedBounds.Origin;
    const FVector DraggedExtent = DraggedBounds.BoxExtent; // half-size in X, Y, Z
    const FVector OtherCenter   = OtherBounds.Origin;
    const FVector OtherExtent   = OtherBounds.BoxExtent;

    float Gap       = SnapGap; 
    float XOffset   = DraggedExtent.X + OtherExtent.X + Gap;
    float YOffset   = DraggedExtent.Y + OtherExtent.Y + Gap;
    float dx        = FMath::Abs(DraggedCenter.X - OtherCenter.X);
    float dy        = FMath::Abs(DraggedCenter.Y - OtherCenter.Y);

    // Start from current Z
    FVector ProposedSnapPos = DraggedActor->GetActorLocation();

    // Snap logic: choose which axis to align
    if (dx < dy)
    {
        // Closer on X, line them up on X
        ProposedSnapPos.X = OtherCenter.X;
        // Offset Y
        const float SignY = (DraggedCenter.Y >= OtherCenter.Y) ? 1.f : -1.f;
        ProposedSnapPos.Y = OtherCenter.Y + SignY * YOffset;
    }
    else
    {
        // Closer on Y, line them up on Y
        ProposedSnapPos.Y = OtherCenter.Y;
        // Offset X
        const float SignX = (DraggedCenter.X >= OtherCenter.X) ? 1.f : -1.f;
        ProposedSnapPos.X = OtherCenter.X + SignX * XOffset;
    }

    // ------------------
    // Phase 2: Check overlap at the new position (before actually setting it)
    // ------------------
    AActor* Overlapped = CheckForSnapOverlap(DraggedActor, ProposedSnapPos);

    if (!Overlapped)
    {
        // Great! No overlaps -> finalize move
        SetWorkAreaPosition(DraggedActor, ProposedSnapPos);
    }
    else
    {
        // We have an overlap with some actor we care about snapping
        // -> You can attempt a "chain snap" or skip

        // Example: Attempt chain snap by recursively calling SnapToActor 
        // with the overlapped actor. 
        UE_LOG(LogTemp, Log, TEXT("Overlap found (%s). Attempting chain snap..."), *Overlapped->GetName());

        // We'll *first* move to the ProposedSnapPos (or you might skip moving it until chain resolves).
        // Let's do a minimal approach: *don't* set the position until chain tries to fix it.

        // Recursively snap
        SnapToActorVisited(DraggedActor, Overlapped, AlreadyVisited);

        // Now check again if we're still overlapping:
        AActor* StillOverlaps = CheckForSnapOverlap(DraggedActor, DraggedActor->GetActorLocation());
        if (!StillOverlaps)
        {
            // Good, the chain snap resolved it, we can finalize
            // If we didn't set the position inside the recursion,
            // do it now.  If we *did* set it in recursion, no need.
        	UE_LOG(LogTemp, Warning, TEXT("No Overlap Anymore!"));
            SetWorkAreaPosition(DraggedActor, ProposedSnapPos);
        	WorkAreaIsSnapped = true;
        }
        else
        {
            // We still overlap. Decide what to do. 
            // Possibly we do nothing or revert to original location.
            UE_LOG(LogTemp, Warning, TEXT("Still Overlap!"));
        	WorkAreaIsSnapped = false;
        }
    }
}


void AExtendedControllerBase::SnapToActor(AWorkArea* DraggedActor, AActor* OtherActor)
{
    if (!DraggedActor || !OtherActor || DraggedActor == OtherActor)
        return;

    // 1) Get the dragged mesh
    UStaticMeshComponent* DraggedMesh = DraggedActor->FindComponentByClass<UStaticMeshComponent>();
    if (!DraggedMesh)
    {
        UE_LOG(LogTemp, Warning, TEXT("DraggedActor has no StaticMeshComponent!"));
        return;
    }

    // 2) Get the other mesh
    UStaticMeshComponent* OtherMesh = OtherActor->FindComponentByClass<UStaticMeshComponent>();
    if (!OtherMesh)
    {
        UE_LOG(LogTemp, Warning, TEXT("OtherActor has no StaticMeshComponent!"));
        return;
    }

    // 3) Compute each mesh’s bounds in world space
    //    (You could also use DraggedMesh->Bounds directly, but CalcBounds is explicit.)
    FBoxSphereBounds DraggedBounds = DraggedMesh->CalcBounds(DraggedMesh->GetComponentTransform());
    FBoxSphereBounds OtherBounds   = OtherMesh->CalcBounds(OtherMesh->GetComponentTransform());

    // 4) Extract the centers and half-extents
    FVector DraggedCenter = DraggedBounds.Origin;
    FVector DraggedExtent = DraggedBounds.BoxExtent; // half-size in X, Y, Z
    FVector OtherCenter   = OtherBounds.Origin;
    FVector OtherExtent   = OtherBounds.BoxExtent;

    // 5) Decide on a small gap so they don’t overlap visually
    float Gap = SnapGap;

    // 6) Compute how much to offset on X and Y so they "just touch"
    //    (i.e. sum of half-extents along that axis + Gap)
    float XOffset = DraggedExtent.X + OtherExtent.X + Gap;
    float YOffset = DraggedExtent.Y + OtherExtent.Y + Gap;

    // 7) Determine which axis (X or Y) is already closer
    //    - dx < dy => line them up on X; offset on Y
    //    - else => line them up on Y; offset on X
    float dx = FMath::Abs(DraggedCenter.X - OtherCenter.X);
    float dy = FMath::Abs(DraggedCenter.Y - OtherCenter.Y);

    // We’ll start with the current ActorLocation’s Z so we only adjust X/Y in snapping.
    FVector SnappedPos = DraggedActor->GetActorLocation();

    if (dx < dy)
    {
        // They’re closer on X, so snap them to the same X
        SnappedPos.X = OtherCenter.X;

        // Offset Y so they’re side-by-side
        float SignY = (DraggedCenter.Y >= OtherCenter.Y) ? 1.f : -1.f;
        SnappedPos.Y = OtherCenter.Y + SignY * YOffset;
    }
    else
    {
        // They’re closer on Y, so snap them to the same Y
        SnappedPos.Y = OtherCenter.Y;

        // Offset X so they’re side-by-side
        float SignX = (DraggedCenter.X >= OtherCenter.X) ? 1.f : -1.f;
        SnappedPos.X = OtherCenter.X + SignX * XOffset;
    }

    // 8) (Optional) If you need a specific Z (e.g., ground level), set it here.
    //    Otherwise, we keep the dragged actor’s original Z.

    // 9) Finally, move the dragged actor
   // DraggedActor->SetActorLocation(SnappedPos);
	SetWorkAreaPosition(DraggedActor, SnappedPos);

	//
	// ---- Collision Check via BoxOverlapActors (ignoring both actors) ----
	//

	// 1. Create an array of object types to test against
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	// You can include as many object types as needed (e.g. WorldStatic, WorldDynamic, Pawn, etc.):

	// 2. Create an array of actors to ignore
	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(DraggedActor);
	ActorsToIgnore.Add(OtherActor);

	// 3. We'll store any overlapping actors here
	TArray<AActor*> OverlappingActors;

	// 4. Perform the box overlap at the snapped position
	//    using the dragged actor's half-extents.
	bool bSuccess = UKismetSystemLibrary::BoxOverlapActors(
		GetWorld(),
		SnappedPos,       // Box center
		DraggedExtent,    // Half-extent (X, Y, Z)
		TArray<TEnumAsByte<EObjectTypeQuery>>(),
		nullptr,          // (Optional) Class filter. Could be AStaticMeshActor::StaticClass(), etc.
		ActorsToIgnore,
		OverlappingActors
	);

	bool bAnyOverlap = false;

	// bSuccess tells you if the query was *able* to run. Then we check OverlappingActors
	// to see if we collided with anything.
	if (bSuccess)
	{
		for (AActor* Overlapped : OverlappingActors)
		{
			if (!Overlapped || Overlapped == DraggedActor || Overlapped == OtherActor || Overlapped->IsA(ALandscape::StaticClass()))
				continue;



			// If it’s a WorkArea or Building, it's a snap-relevant overlap
			if (Cast<AWorkArea>(Overlapped) || Cast<ABuildingBase>(Overlapped))
			{
				// Log to see which actor we are overlapping
				bAnyOverlap = true;
			}
		}
	}

	if (!bAnyOverlap)
	{
		WorkAreaIsSnapped = true;
	}
	else
	{
		WorkAreaIsSnapped = false;
	}
}

void AExtendedControllerBase::MoveWorkArea_Implementation(float DeltaSeconds)
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

    FVector MousePosition, MouseDirection;
    DeprojectMousePositionToWorld(MousePosition, MouseDirection);

    // Raycast from the mouse into the scene
    FVector Start = MousePosition;
    FVector End   = Start + MouseDirection * 5000.f;

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

    // Compute distance from the drag’s current location to the raycast hit
    float Distance = FVector::Dist(
        HitResult.Location,
        DraggedWorkArea->GetActorLocation()
    );

    //---------------------------------------
    // Get DraggedWorkArea bounding box info
    //---------------------------------------
	UStaticMeshComponent* DraggedMesh = DraggedWorkArea->FindComponentByClass<UStaticMeshComponent>();

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
        if (UStaticMeshComponent* Mesh = DraggedWorkArea->FindComponentByClass<UStaticMeshComponent>())
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

                    	UStaticMeshComponent* OverlappedMesh = OverlappedActor->FindComponentByClass<UStaticMeshComponent>();

                    	if (!OverlappedMesh)
                    	{
                    		// If there's no mesh on this overlapped actor, skip to the next actor
                    		continue;
                    	}
                    	
                    	FBoxSphereBounds OverlappedDraggedBounds = OverlappedMesh->CalcBounds(DraggedMesh->GetComponentTransform());
                    	FVector OverlappedExtent = OverlappedDraggedBounds.BoxExtent;
                        //-------------------------------------------------
                        // Also factor in the OverlappedActor's box extent
                        //-------------------------------------------------
                        // Example: we only care about XY distance for snapping
                        float XYDistance = FVector::Dist2D(
                            DraggedWorkArea->GetActorLocation(),
                            OverlappedActor->GetActorLocation()
                        );

                        // Combine XY extents: (X1 + X2) + (Y1 + Y2)
                        // 0.5f is optional if you want some average
                        float CombinedXYExtent = (Extent.X + OverlappedExtent.X +
                                                  Extent.Y + OverlappedExtent.Y) * 0.5f;

                        float SnapThreshold = SnapDistance + SnapGap + CombinedXYExtent;

                        if (XYDistance < SnapThreshold)
                        {
                            // Snap logic
                            SnapToActor(DraggedWorkArea, OverlappedActor);
                            return; // Break out if you only want one snap
                        }
                    }
                }
            }
        }
    }

    //---------------------------------
    // If no snap, move the WorkArea
    //---------------------------------
    if (bHit && HitResult.GetActor() != nullptr) // && !bAnyOverlap
    {
        CurrentDraggedGround = HitResult.GetActor();

        FVector NewActorPosition = HitResult.Location;
        // Adjust Z if needed
        NewActorPosition.Z += DraggedAreaZOffset; 
        SetWorkAreaPosition(DraggedWorkArea, NewActorPosition);

        WorkAreaIsSnapped = false;
    }
}


void AExtendedControllerBase::SendWorkerToWork_Implementation(AUnitBase* Worker)
{

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
			Worker->SetUEPathfinding = true;
		}
		else
		{
			// If they are not overlapping, set the state to 'GoToBuild'
			Worker->SetUnitState(UnitData::GoToBuild);
			Worker->SetUEPathfinding = true;
		}
	

	

	
	 Worker->CurrentDraggedWorkArea = nullptr;
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

		// Loop through the overlapping actors to check if they are instances of AWorkArea or ABuildingBase
		for (AActor* OverlappedActor : OverlappingActors)
		{
			if (OverlappedActor->IsA(AWorkArea::StaticClass()) || OverlappedActor->IsA(ABuildingBase::StaticClass()))
			{
				bIsOverlappingWithValidArea = true;
				break;
			}
		}

		// If overlapping with AWorkArea or ABuildingBase, destroy the CurrentDraggedWorkArea
		if (bIsOverlappingWithValidArea && !WorkAreaIsSnapped) // bIsOverlappingWithValidArea &&
		{
			SelectedUnits[0]->CurrentDraggedWorkArea->Destroy();
			return true;
		}

		if(SelectedUnits.Num() && SelectedUnits[0] && SelectedUnits[0]->IsWorker)
		{
				SendWorkerToWork(SelectedUnits[0]);
				return true;
		}
		
		//CurrentDraggedWorkArea = nullptr;
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

void AExtendedControllerBase::DestoryWorkAreaOnServer_Implementation(AWorkArea* WorkArea)
{
	if (WorkArea) WorkArea->Destroy();;
}

void AExtendedControllerBase::DestoryWorkArea()
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
			DestoryWorkAreaOnServer(WorkArea);
		}
	}
	
}

void AExtendedControllerBase::LeftClickPressed()
{
	LeftClickIsPressed = true;

	if (!CameraBase || CameraBase->TabToggled) return;
	
	if(AltIsPressed)
	{
		DestoryWorkArea();
	}else if (AttackToggled) {
		AttackToggled = false;
		FHitResult Hit;
		GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, false, Hit);

		int32 NumUnits = SelectedUnits.Num();
		// int32 GridSize = FMath::CeilToInt(FMath::Sqrt((float)NumUnits));
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

				//FVector RunLocation = Hit.Location + FVector(Col * 100, Row * 100, 0.f);  // Adjust x and y positions equally for a square grid
				const FVector RunLocation = Hit.Location + CalculateGridOffset(Row, Col);
			
				if(SetBuildingWaypoint(RunLocation, SelectedUnits[i], BWaypoint, PlayWaypointSound))
				{
					// Do Nothing
				}else
				{
					DrawDebugSphere(GetWorld(), RunLocation, 15, 5, FColor::Red, false, 1.5, 0, 1);
					LeftClickAttack_Implementation(SelectedUnits[i], RunLocation);
					PlayAttackSound = true;
				}
			}
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
		LeftClickSelect();

		FHitResult Hit_Pawn;
		GetHitResultUnderCursor(ECollisionChannel::ECC_Pawn, false, Hit_Pawn);
		
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
}

void AExtendedControllerBase::RightClickPressed()
{
	AttackToggled = false;
	FHitResult Hit;
	GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, false, Hit);
	
	if(!SelectedUnits.Num() || !SelectedUnits[0]->CurrentDraggedWorkArea)
		if(!CheckClickOnWorkArea(Hit))
		{
			RunUnitsAndSetWaypoints(Hit);
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
	if(!RTSGameMode || !RTSGameMode->AllUnits.Num()) return;
	
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

	// Update the HUD with the sorted selection
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
	
	SelectedUnits = HUDBase->SelectedUnits;
}

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

	int StartIndex = CurrentUnitWidgetIndex;
	FGameplayTag CurrentTag = SelectedUnits[CurrentUnitWidgetIndex]->AbilitySelectionTag;

	while (true)
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
	}
}

void AExtendedControllerBase::SendWorkerToResource_Implementation(AWorkingUnitBase* Worker, AWorkArea* WorkArea)
{
	if(Worker)
	{
		Worker->ResourcePlace = WorkArea;
		Worker->SetUnitState(UnitData::GoToResourceExtraction);
	}
}

void AExtendedControllerBase::SendWorkerToWorkArea_Implementation(AWorkingUnitBase* Worker, AWorkArea* WorkArea)
{
	Worker->BuildArea = WorkArea;
	// Check if the worker is overlapping with the build area
	if (Worker->IsOverlappingActor(Worker->BuildArea))
	{
		// If they are overlapping, set the state to 'Build'
		Worker->SetUnitState(UnitData::Build);
	}
	else
	{
		// If they are not overlapping, set the state to 'GoToBuild'
		Worker->SetUnitState(UnitData::GoToBuild);
	}
}
bool AExtendedControllerBase::CheckClickOnWorkArea(FHitResult Hit_Pawn)
{
	StopWorkOnSelectedUnit();
	
	if (Hit_Pawn.bBlockingHit && HUDBase)
	{
		AActor* HitActor = Hit_Pawn.GetActor();
		
		AWorkArea* WorkArea = Cast<AWorkArea>(HitActor);

		if(WorkArea)
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

						SendWorkerToResource(Worker, WorkArea);
					}
				}
			} else if(WorkArea && WorkArea->Type == WorkAreaData::BuildArea)
			{
				if(SelectedUnits.Num() && SelectedUnits[0])
				{
					AWorkingUnitBase* Worker = Cast<AWorkingUnitBase>(SelectedUnits[0]);
					if(Worker && (Worker->TeamId == WorkArea->TeamId || WorkArea->TeamId == 0))
					{
						SendWorkerToWorkArea(Worker, WorkArea);
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
	//if (UnitBase->TeamId == SelectableTeamId){
		FHitResult Hit;
		GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, false, Hit);
		if (UnitBase->ActivatedAbilityInstance)
		{
			UnitBase->ActivatedAbilityInstance->OnAbilityCastComplete(Hit);
		}
	//}
}


