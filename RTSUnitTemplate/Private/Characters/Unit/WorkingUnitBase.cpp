// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
// WorkingUnitBase.h (Corresponding Header)
#include "Characters/Unit/WorkingUnitBase.h"

// Engine Headers
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NavCollision.h"

// Project-Specific Headers
#include "GAS/AttributeSetBase.h"
#include "Widgets/UnitBaseHealthBar.h"
#include "Actors/Projectile.h"
#include "Controller/PlayerController/ExtendedControllerBase.h"
#include "Characters/Unit/BuildingBase.h"
#include "Characters/Unit/ConstructionUnit.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameModes/ResourceGameMode.h"
#include "Net/UnrealNetwork.h"
#include "Subsystems/ResourceVisualManager.h"
#include "MassEntityTypes.h"
#include "MassActorSubsystem.h"

namespace
{
	/**
	 * Halbe Grundflaeche eines Gebaeudes in XY, aus dem ISM-Mesh und dessen Skalierung.
	 *
	 * Bewusst NICHT ueber GetActorBounds: dort zaehlen Kapsel, Healthbar-Widget und Niagara mit, und
	 * genau die Kapsel ist bei diesen Gebaeuden das falsche Mass (BioIntegrator: Kapselradius 102
	 * gegen 248 echte Halbbreite). Der sichtbare Koerper haengt am ISM.
	 *
	 * Der Ursprung der Mesh-Bounds wird mitgerechnet, weil ein Mesh nicht um seinen Mittelpunkt
	 * modelliert sein muss - sonst faellt die Haelfte der Grundflaeche unter den Tisch.
	 */
	bool GebaeudeGrundflaeche(const AActor* Actor, FVector2D& Aus)
	{
		const AMassUnitBase* MassUnit = Cast<AMassUnitBase>(Actor);
		if (!MassUnit || !MassUnit->ISMComponent)
		{
			return false;
		}

		const UStaticMesh* Mesh = MassUnit->ISMComponent->GetStaticMesh();
		if (!Mesh)
		{
			return false;
		}

		const FBoxSphereBounds Bounds = Mesh->GetBounds();
		const FVector Skalierung = MassUnit->ISMComponent->GetRelativeScale3D();

		Aus.X = FMath::Max(FMath::Abs(Bounds.Origin.X - Bounds.BoxExtent.X),
		                   FMath::Abs(Bounds.Origin.X + Bounds.BoxExtent.X)) * FMath::Abs(Skalierung.X);
		Aus.Y = FMath::Max(FMath::Abs(Bounds.Origin.Y - Bounds.BoxExtent.Y),
		                   FMath::Abs(Bounds.Origin.Y + Bounds.BoxExtent.Y)) * FMath::Abs(Skalierung.Y);

		return Aus.X > 1.f && Aus.Y > 1.f;
	}

	/** Dasselbe fuer das Gebaeude, das aus dieser Baustellenklasse einmal entstehen wird. */
	bool ExtensionGrundflaeche(TSubclassOf<AWorkArea> WorkAreaClass, FVector2D& Aus)
	{
		if (!WorkAreaClass)
		{
			return false;
		}

		const AWorkArea* AreaCDO = WorkAreaClass->GetDefaultObject<AWorkArea>();
		if (!AreaCDO || !AreaCDO->BuildingClass)
		{
			return false;
		}

		return GebaeudeGrundflaeche(AreaCDO->BuildingClass->GetDefaultObject<AActor>(), Aus);
	}
}

void AWorkingUnitBase::BeginPlay()
{
	Super::BeginPlay();

	AResourceGameMode* GameMode = Cast<AResourceGameMode>(GetWorld()->GetAuthGameMode());

	if (GameMode)
	{
			GameMode->AssignWorkAreasToWorker(this);
	}
}

void AWorkingUnitBase::Destroyed()
{
	if (BuildArea)
	{
		BuildArea->RemoveWorkerFromArray(this);
		if (BuildArea->Workers.Num() == 0 && !BuildArea->StartedBuilding)
		{
			BuildArea->PlannedBuilding = false;
		}
	}

	// Release the resource slot too, so CurrentWorkers (the HUD count) stays symmetric when a worker dies.
	// Goes through SetResourcePlace so the game mode's per-team counters are resynced as well - calling
	// RemoveWorkerFromArray directly only fixed the node's own count and left the HUD showing a phantom.
	if (ResourcePlace)
	{
		SetResourcePlace(nullptr);
	}

	if (WorkResource)
	{
		WorkResource->Destroy();
		WorkResource = nullptr;
	}
	Super::Destroyed();
}

void AWorkingUnitBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWorkingUnitBase, CurrentDraggedWorkArea);
	DOREPLIFETIME(AWorkingUnitBase, ResourcePlace);
	DOREPLIFETIME(AWorkingUnitBase, Base);
	DOREPLIFETIME(AWorkingUnitBase, BuildArea);
	DOREPLIFETIME(AWorkingUnitBase, CarryingResourceType);
}

bool AWorkingUnitBase::CanMineResourceType(EResourceType ResourceType) const
{
	// MAX is the "no resource" sentinel (also what ConvertToResourceType returns for Base /
	// BuildArea / NoBuildZone areas) - never a valid mining target.
	if (ResourceType == EResourceType::MAX)
	{
		return false;
	}

	// Unrestricted worker: mines anything. This is the default, so existing content is untouched.
	if (!bRestrictMineableResources)
	{
		return true;
	}

	return MineableResourceTypes.Contains(ResourceType);
}

bool AWorkingUnitBase::CanMineWorkArea(const AWorkArea* Area) const
{
	if (!IsValid(Area))
	{
		return false;
	}

	return CanMineResourceType(ConvertToResourceType(Area->Type));
}

EResourceType AWorkingUnitBase::GetRoutingResourceType() const
{
	// Carrying something -> that cargo decides which base is valid.
	if (CarryingResourceType != EResourceType::MAX)
	{
		return CarryingResourceType;
	}

	// Not carrying, but already assigned to a deposit -> route by what it is about to bring home,
	// so it never walks to a base that would reject the load it is going to fetch.
	if (IsValid(ResourcePlace))
	{
		return ConvertToResourceType(ResourcePlace->Type);
	}

	return EResourceType::MAX;
}

bool AWorkingUnitBase::CanDeliverToBase(const ABuildingBase* InBase) const
{
	if (!IsValid(InBase))
	{
		return false;
	}

	return InBase->AcceptsResourceType(GetRoutingResourceType());
}

void AWorkingUnitBase::SetResourcePlace(AWorkArea* NewPlace, bool bRegisterOnNewPlace)
{
	if (ResourcePlace == NewPlace)
	{
		// Same node: only make sure we are actually registered if the caller asked for it.
		if (bRegisterOnNewPlace && IsValid(ResourcePlace))
		{
			ResourcePlace->AddWorkerToArray(this);
		}
		return;
	}

	// Give the old node its slot back. This is the step every raw assignment used to skip.
	const bool bHadPlace = IsValid(ResourcePlace);
	if (bHadPlace)
	{
		ResourcePlace->RemoveWorkerFromArray(this);
	}

	ResourcePlace = NewPlace;

	if (bRegisterOnNewPlace && IsValid(ResourcePlace))
	{
		ResourcePlace->AddWorkerToArray(this);
	}

	// RemoveWorkerFromArray only maintains the per-node count. The "N/Max" the HUD shows comes from the
	// game mode's per-team counters, which are kept by hand with +1/-1 at the assignment sites - and none
	// of those run when a worker simply gives up its deposit (it died, or was sent off to build). That is
	// why a slot stayed occupied by a worker that no longer exists. Recompute instead of patching, so the
	// count cannot drift no matter which path released the worker.
	if (bHadPlace && HasAuthority())
	{
		if (AResourceGameMode* ResourceGameMode = Cast<AResourceGameMode>(GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr))
		{
			ResourceGameMode->SetAllCurrentWorkers(TeamId);
		}
	}
}

void AWorkingUnitBase::ReleaseResourcePlace()
{
	SetResourcePlace(nullptr);
}

FVector AWorkingUnitBase::GetBuildAreaEffectScale(float BaseScale, float ReferenceSize,
                                                  float MinScale, float MaxScale) const
{
	const FVector Fallback(BaseScale, BaseScale, BaseScale);

	if (!IsValid(BuildArea) || ReferenceSize <= KINDA_SMALL_NUMBER)
	{
		return Fallback;
	}

	// Prefer the area's mesh bounds; the actor bounding box also swallows trigger capsules and would
	// report a footprint far larger than what the player sees.
	FVector Size = FVector::ZeroVector;
	if (BuildArea->Mesh)
	{
		Size = BuildArea->Mesh->Bounds.GetBox().GetSize();
	}
	if (Size.IsNearlyZero())
	{
		Size = BuildArea->GetComponentsBoundingBox(true).GetSize();
	}
	if (Size.IsNearlyZero())
	{
		return Fallback;
	}

	const float Footprint = FMath::Max(Size.X, Size.Y);
	const float Scale = FMath::Clamp((Footprint / ReferenceSize) * BaseScale, MinScale, MaxScale);
	return FVector(Scale, Scale, Scale);
}

void AWorkingUnitBase::OnRep_CarryingResourceType()
{
	if (GetNetMode() == NM_DedicatedServer) return;

	UResourceVisualManager* VisualManager = GetWorld()->GetSubsystem<UResourceVisualManager>();
	if (!VisualManager) return;

	UMassActorSubsystem* MassActorSubsystem = GetWorld()->GetSubsystem<UMassActorSubsystem>();
	if (!MassActorSubsystem) return;

	FMassEntityHandle EntityHandle = MassActorSubsystem->GetEntityHandleFromActor(this);
	if (!EntityHandle.IsValid()) return;

	if (CarryingResourceType != EResourceType::MAX)
	{
		TSubclassOf<AWorkResource> ResourceClass = nullptr;
		if (ResourcePlace)
		{
			ResourceClass = ResourcePlace->WorkResourceClass;
		}
		VisualManager->AssignResource(EntityHandle, CarryingResourceType, ResourceClass);
	}
	else
	{
		VisualManager->RemoveResource(EntityHandle);
	}
}

void AWorkingUnitBase::OnRep_CurrentDraggedWorkArea()
{
	if (CurrentDraggedWorkArea)
	{
		ShowWorkAreaIfNoFog_Implementation(CurrentDraggedWorkArea);
	}
}


void AWorkingUnitBase::SpawnWorkArea_Implementation(TSubclassOf<AWorkArea> WorkAreaClass, AWaypoint* Waypoint)
{
		if (!OwningPlayerController)
		{
			UE_LOG(LogTemp, Error, TEXT("No OwningPlayerController"));
			return;
		}
	
		AExtendedControllerBase* ExtendedControllerBase = Cast<AExtendedControllerBase>(OwningPlayerController);
	
		if (!ExtendedControllerBase)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to get owning player controller."));
			return;
		}


		if (WorkAreaClass && !CurrentDraggedWorkArea && ExtendedControllerBase->SelectableTeamId == TeamId) // ExtendedControllerBase->CurrentDraggedGround == nullptr &&
		{
			FVector MousePosition, MouseDirection;
			ExtendedControllerBase->DeprojectMousePositionToWorld(MousePosition, MouseDirection);

			// Raycast from the mouse position into the scene to find the ground
			FVector Start = MousePosition;
			FVector End = Start + MouseDirection * 100000.f; // Extend to a maximum reasonable distance

			FHitResult HitResult;
			FCollisionQueryParams CollisionParams;
			CollisionParams.bTraceComplex = true; // Use complex collision for precise tracing

			// Perform the raycast
			bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, CollisionParams);
	
			FVector SpawnLocation = HitResult.Location; // Assuming you want to use HitResult location as spawn point
			FRotator SpawnRotation = FRotator::ZeroRotator;
			FActorSpawnParameters SpawnParams;
			//SpawnParams.Owner = this;
			SpawnParams.Instigator = ExtendedControllerBase->GetPawn(); // Assuming we want to set the pawn that is responsible for spawning
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	    
			AWorkArea* SpawnedWorkArea = GetWorld()->SpawnActor<AWorkArea>(WorkAreaClass, SpawnLocation, SpawnRotation, SpawnParams);
			if (SpawnedWorkArea)
			{
			
				if(Waypoint) SpawnedWorkArea->NextWaypoint = Waypoint;
				SpawnedWorkArea->TeamId = TeamId;
				CurrentDraggedWorkArea = SpawnedWorkArea;
				//BuildArea = SpawnedWorkArea;
			
			}
			
		}
}

void AWorkingUnitBase::ServerSpawnWorkArea_Implementation(TSubclassOf<AWorkArea> WorkAreaClass, AWaypoint* Waypoint, FVector HitLocation)
{

	if (HasAuthority()) // Make sure the server is executing this code
	{
		FRotator SpawnRotation = FRotator::ZeroRotator;
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		// Spawn the WorkArea on the server
		AWorkArea* SpawnedWorkArea = GetWorld()->SpawnActor<AWorkArea>(WorkAreaClass, HitLocation, SpawnRotation, SpawnParams);
		if (SpawnedWorkArea)
		{
			if (Waypoint)
			{
				SpawnedWorkArea->NextWaypoint = Waypoint;
			}
			SpawnedWorkArea->TeamId = TeamId;
			CurrentDraggedWorkArea = SpawnedWorkArea;

			SpawnedWorkArea->ForceNetUpdate();
			ClientReceiveWorkArea(SpawnedWorkArea);
		}
	}
}

AWorkArea* AWorkingUnitBase::SpawnWorkAreaReplicated(TSubclassOf<AWorkArea> WorkAreaClass,
								   AWaypoint* Waypoint,
								   FVector SpawnLocation,
								   const FBuildingCost ConstructionCost,
								   bool IsPaid,
								   TSubclassOf<AUnitBase> ConstructionUnitClass,
								   bool IsExtensionArea) 
{
	
 // && !CurrentDraggedWorkArea
	if (CurrentDraggedWorkArea){
		// [Bauwahl] Hier stirbt die vorige Baustelle DIESES Arbeiters, damit die neue an ihre Stelle
		// tritt. Wenn die KI oefter Bauauftraege erteilt, als ein Arbeiter laufen und bauen kann,
		// loescht jeder neue Auftrag die Baustelle, zu der der Arbeiter gerade unterwegs ist - und
		// zwar quer ueber alle Gebaeudeklassen. Diese Zeile zeigt, WER wen verdraengt.
		UE_LOG(LogTemp, Warning,
			TEXT("[Bauwahl] Team=%d VERDRAENGT: %s wird geloescht fuer %s"),
			TeamId, *GetNameSafe(CurrentDraggedWorkArea->GetClass()), *GetNameSafe(WorkAreaClass));

		CurrentDraggedWorkArea->PlannedBuilding = true;
		CurrentDraggedWorkArea->ControlTimer = 0.f;
		CurrentDraggedWorkArea->RemoveAreaFromGroup();
		CurrentDraggedWorkArea->Destroy();
		CurrentDraggedWorkArea = nullptr;
	}
	
	if (WorkAreaClass)
	{
		FVector TargetLocation = SpawnLocation;
		FRotator TargetRotation = FRotator::ZeroRotator;

		if (IsExtensionArea)
		{
			ABuildingBase* Unit = Cast<ABuildingBase>(this);
			if (!Unit)
			{
				Unit = Base;
			}

			if (Unit)
			{
				const FVector UnitLoc = Unit->GetActorLocation();
				FVector UnitExtentBounds(100.f, 100.f, 100.f);

				if (UCapsuleComponent* Capsule = Unit->FindComponentByClass<UCapsuleComponent>())
				{
					const float R = Capsule->GetScaledCapsuleRadius();
					UnitExtentBounds.X = R;
					UnitExtentBounds.Y = R;
					UnitExtentBounds.Z = Capsule->GetScaledCapsuleHalfHeight();
				}

				float AbsX = Unit->ExtensionOffset.X + UnitExtentBounds.X;
				float AbsY = Unit->ExtensionOffset.Y + UnitExtentBounds.Y;

				// Der Kapselradius ist ein schlechtes Mass fuer die sichtbare Grundflaeche: beim
				// BioIntegrator sind es 102 gegen 248 echte Halbbreite, weshalb seine Extension bisher
				// IM Gebaeude stand. Und ein fester Offset je Wirt kann ohnehin nicht stimmen, wenn
				// dasselbe Gebaeude zwei verschieden grosse Extensions baut - die CybernaticFactory
				// setzt eine 177 und eine 623 Einheiten breite an dieselbe Kante.
				//
				// Aus den echten Meshmassen gerechnet passt jede Paarung von selbst. Gegenprobe an der
				// MatterForge, deren Extensions der Nutzer als richtig sitzend bezeichnet:
				// 177,5 (Wirt) + 88,9 (Extension) + 20 = 286,4 gegen die dort von Hand gepflegten 285,8.
				if (Unit->bExtensionAutoDistance)
				{
					FVector2D WirtHalb, ExtHalb;
					if (GebaeudeGrundflaeche(Unit, WirtHalb) && ExtensionGrundflaeche(WorkAreaClass, ExtHalb))
					{
						// Der Snap dreht die Extension so, dass ihr lokales +X vom Wirt weg zeigt. Bei
						// einem Rotationsversatz um 90 Grad zeigt stattdessen ihr lokales Y nach aussen,
						// also zaehlt dann diese Halbbreite fuer den Abstand.
						const float Versatz = FMath::Abs(FRotator::NormalizeAxis(Unit->ExtensionRotationOffset));
						const bool bQuergestellt = (Versatz > 45.f && Versatz < 135.f);
						const float ExtLaengs = bQuergestellt ? ExtHalb.Y : ExtHalb.X;

						AbsX = WirtHalb.X + ExtLaengs + Unit->ExtensionGap;
						AbsY = WirtHalb.Y + ExtLaengs + Unit->ExtensionGap;

						// [Extension] Ohne diese Zeile laesst sich nicht unterscheiden, ob der Abstand
						// gerechnet wurde oder ob eine der beiden Grundflaechen still auf den alten
						// ExtensionOffset zurueckgefallen ist.
						UE_LOG(LogTemp, Warning,
							TEXT("[Extension] %s -> %s: Wirt(%.0f/%.0f) + Ext %.0f (%s) + Luft %.0f => X=%.0f Y=%.0f"),
							*GetName(), *GetNameSafe(WorkAreaClass),
							WirtHalb.X, WirtHalb.Y, ExtLaengs,
							bQuergestellt ? TEXT("quer") : TEXT("laengs"),
							Unit->ExtensionGap, AbsX, AbsY);
					}
					else
					{
						UE_LOG(LogTemp, Warning,
							TEXT("[Extension] %s -> %s: Grundflaeche nicht ermittelbar, alter ExtensionOffset gilt."),
							*GetName(), *GetNameSafe(WorkAreaClass));
					}
				}
				const FVector2D Delta2D(SpawnLocation.X - UnitLoc.X, SpawnLocation.Y - UnitLoc.Y);
				const float MouseDist = Delta2D.Size();

				TargetLocation = UnitLoc;
				float DesiredYaw = 0.f;
				FVector Offset(0.f, 0.f, 0.f);

				switch (Unit->ExtensionSnapMethod)
				{
					case EExtensionSnapMethod::Snap8Way:
						{
							float AngleDeg = FMath::RadiansToDegrees(FMath::Atan2(Delta2D.Y, Delta2D.X));
							if (AngleDeg < 0) AngleDeg += 360.f;
							float SnappedAngle = FMath::RoundToFloat(AngleDeg / 45.f) * 45.f;
							DesiredYaw = (SnappedAngle >= 360.f) ? 0.f : SnappedAngle;
							
							float SnappedRad = FMath::DegreesToRadians(DesiredYaw);
							float CosA = FMath::Cos(SnappedRad), SinA = FMath::Sin(SnappedRad);
							float TargetX = (FMath::Abs(CosA) > 0.1f) ? (FMath::Sign(CosA) * AbsX) : 0.f;
							float TargetY = (FMath::Abs(SinA) > 0.1f) ? (FMath::Sign(SinA) * AbsY) : 0.f;

							float Dist = Unit->ExtensionMovementAllowed ? FMath::Min(MouseDist, FVector2D(TargetX, TargetY).Size()) : FVector2D(TargetX, TargetY).Size();
							Offset = FVector(CosA * Dist, SinA * Dist, 0.f);
						}
						break;

					case EExtensionSnapMethod::Snap4Way:
						if (FMath::Abs(Delta2D.X) >= FMath::Abs(Delta2D.Y)) {
							float SignX = (Delta2D.X >= 0.f) ? 1.f : -1.f;
							Offset.X = SignX * (Unit->ExtensionMovementAllowed ? FMath::Min(FMath::Abs(Delta2D.X), AbsX) : AbsX);
							DesiredYaw = (SignX > 0.f) ? 0.f : 180.f;
						} else {
							float SignY = (Delta2D.Y >= 0.f) ? 1.f : -1.f;
							Offset.Y = SignY * (Unit->ExtensionMovementAllowed ? FMath::Min(FMath::Abs(Delta2D.Y), AbsY) : AbsY);
							DesiredYaw = (SignY > 0.f) ? 90.f : 270.f;
						}
						break;

					case EExtensionSnapMethod::Snap2Way:
					case EExtensionSnapMethod::Snap1Way:
						{
							bool bIsXAxis = FMath::Abs(Unit->ExtensionOffset.X) >= FMath::Abs(Unit->ExtensionOffset.Y);
							if (bIsXAxis) {
								float SignX = (Unit->ExtensionSnapMethod == EExtensionSnapMethod::Snap1Way) 
									? FMath::Sign(Unit->ExtensionOffset.X) 
									: ((Delta2D.X >= 0.f) ? 1.f : -1.f);
								if (SignX == 0.f) SignX = 1.f;
								Offset.X = SignX * (Unit->ExtensionMovementAllowed ? FMath::Min(FMath::Abs(Delta2D.X), AbsX) : AbsX);
								DesiredYaw = (SignX > 0.f) ? 0.f : 180.f;
							} else {
								float SignY = (Unit->ExtensionSnapMethod == EExtensionSnapMethod::Snap1Way) 
									? FMath::Sign(Unit->ExtensionOffset.Y) 
									: ((Delta2D.Y >= 0.f) ? 1.f : -1.f);
								if (SignY == 0.f) SignY = 1.f;
								Offset.Y = SignY * (Unit->ExtensionMovementAllowed ? FMath::Min(FMath::Abs(Delta2D.Y), AbsY) : AbsY);
								DesiredYaw = (SignY > 0.f) ? 90.f : 270.f;
							}
						}
						break;

					case EExtensionSnapMethod::None:
					default:
						Offset = Unit->ExtensionOffset;
						DesiredYaw = 0.f;
						break;
				}
				TargetLocation += Offset;

				// Dreht nur die Ausrichtung, nicht die Position: die Extension bleibt an der Seite, die
				// der Snap gewaehlt hat, und wird dort quergestellt. Weil die Baustelle ihre Drehung
				// unten als ServerMeshRotationBuilding weiterreicht, dreht sich das fertige Gebaeude
				// mit - ein Wert fuer Vorschau und Ergebnis.
				DesiredYaw += Unit->ExtensionRotationOffset;
				TargetRotation = FRotator(0.f, DesiredYaw, 0.f);

				if (Unit->ExtensionGroundTrace)
				{
					const FVector TraceStart = TargetLocation + FVector(0, 0, 2000.f);
					const FVector TraceEnd = TargetLocation - FVector(0, 0, 2000.f);
					FCollisionQueryParams Params(SCENE_QUERY_STAT(SpawnExtensionAreaGround), true);
					Params.AddIgnoredActor(Unit);

					FHitResult GroundHit;
					bool bFoundValidGround = false;
					const int32 MaxTries = 8;
					for (int32 Try = 0; Try < MaxTries; ++Try)
					{
						if (!GetWorld()->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility, Params))
						{
							break;
						}
						AActor* HitActor = GroundHit.GetActor();
						if (HitActor && (HitActor->IsA(AWorkArea::StaticClass()) || HitActor->IsA(ABuildingBase::StaticClass())))
						{
							Params.AddIgnoredActor(HitActor);
							continue;
						}
						bFoundValidGround = true;
						break;
					}
					if (bFoundValidGround)
					{
						TargetLocation.Z = GroundHit.Location.Z;
					}
				}
				else
				{
					TargetLocation.Z = UnitLoc.Z + Unit->ExtensionOffset.Z + UnitExtentBounds.Z;
				}

				// Adjust Z for mesh bottom
				if (AWorkArea* DefaultWorkArea = WorkAreaClass->GetDefaultObject<AWorkArea>())
				{
					if (UStaticMeshComponent* MeshComp = DefaultWorkArea->FindComponentByClass<UStaticMeshComponent>())
					{
						const FBoxSphereBounds Bounds = MeshComp->CalcBounds(MeshComp->GetRelativeTransform());
						const float RelativeBottomZ = Bounds.Origin.Z - Bounds.BoxExtent.Z;
						const float Clearance = 2.f;
						TargetLocation.Z += (Clearance - RelativeBottomZ);
					}
				}
			}
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;


		// Spawn the replicated WorkArea on the server
		AWorkArea* SpawnedWorkArea = GetWorld()->SpawnActor<AWorkArea>(
			WorkAreaClass,
			TargetLocation,
			TargetRotation,
			SpawnParams
		);

		if (SpawnedWorkArea)
		{
			// Initialize any properties on the spawned WorkArea
			if (Waypoint)
			{
				SpawnedWorkArea->NextWaypoint = Waypoint;
			}
			SpawnedWorkArea->TeamId          = TeamId;
			SpawnedWorkArea->IsPaid          = IsPaid;
			SpawnedWorkArea->ConstructionCost = ConstructionCost;
			SpawnedWorkArea->IsExtensionArea = IsExtensionArea;

			if (SpawnedWorkArea->IsExtensionArea)
			{
				SpawnedWorkArea->AllowAddingWorkers = false;
				SpawnedWorkArea->ServerMeshRotationBuilding = TargetRotation;

				// Den Wirt HIER festhalten und nicht erst weiter unten: dort haengt es an einer
				// gesetzten ConstructionUnitClass, und ohne die kannte weder die Flaeche noch die
				// spaetere Extension ihr Ursprungsgebaeude. Die Abstandspruefung beim Droppen braucht
				// das aber, um Geschwister-Extensions desselben Gebaeudes durchzulassen.
				SpawnedWorkArea->Origin = this;
			}
			
			CurrentDraggedWorkArea = SpawnedWorkArea;
			//CurrentDraggedWorkArea->SetReplicateMovement(true);

			SpawnedWorkArea->ForceNetUpdate();
			ClientReceiveWorkArea(SpawnedWorkArea);

			// Store optional construction site class
			if (CurrentDraggedWorkArea && ConstructionUnitClass)
			{
				CurrentDraggedWorkArea->ConstructionUnitClass = ConstructionUnitClass;
				CurrentDraggedWorkArea->Origin = this;

				if (AConstructionUnit* ConstructionCDO = Cast<AConstructionUnit>(ConstructionUnitClass->GetDefaultObject()))
				{
					if (ConstructionCDO->SetOffsetsDueToWorkAreaBounds && CurrentDraggedWorkArea->Mesh)
					{
						const FBoxSphereBounds MeshBounds = CurrentDraggedWorkArea->Mesh->CalcBounds(CurrentDraggedWorkArea->Mesh->GetRelativeTransform());
						ConstructionCDO->DefaultOscOffsetA.Z = MeshBounds.Origin.Z - MeshBounds.BoxExtent.Z;
						ConstructionCDO->DefaultOscOffsetB.Z = MeshBounds.Origin.Z + MeshBounds.BoxExtent.Z;
					}
				}
			}
			
			// [Bauweg] Stufe 2 von 3: die Flaeche steht. Ohne diese Zeile ist nicht zu unterscheiden,
			// ob ein Bauauftrag schon an der Platzierung scheitert oder erst am Bauabschluss.
			// Der Abstand ist der Kern der Frage: die Flaeche wird von DIESEM Arbeiter gesetzt, also
			// ist das genau der Weg, den er danach zuruecklegen muss. Platzierungen, die immer
			// weiter nach aussen wandern, werden hier sichtbar - und zwar je Gebaeudeklasse.
			UE_LOG(LogTemp, Warning,
				TEXT("[Bauweg] Team=%d WorkArea GESETZT: %s bei (%.0f, %.0f) Abstand=%.0f IsPaid=%d Ext=%d"),
				TeamId, *GetNameSafe(WorkAreaClass), TargetLocation.X, TargetLocation.Y,
				FVector::Dist2D(GetActorLocation(), TargetLocation),
				IsPaid ? 1 : 0, IsExtensionArea ? 1 : 0);

			return CurrentDraggedWorkArea;
		}

		UE_LOG(LogTemp, Warning,
			TEXT("[Bauweg] Team=%d WorkArea SPAWN FEHLGESCHLAGEN: %s bei (%.0f, %.0f)"),
			TeamId, *GetNameSafe(WorkAreaClass), TargetLocation.X, TargetLocation.Y);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Bauweg] Team=%d WorkArea OHNE KLASSE angefordert."), TeamId);
	}

	return nullptr;
}


void AWorkingUnitBase::ClientReceiveWorkArea_Implementation(AWorkArea* ClientArea)
{
	if (!ClientArea)
	{
		UE_LOG(LogTemp, Warning, TEXT("ClientReceiveWorkArea: ClientArea is NULL!"));
		return;
	}

	UE_LOG(LogTemp, Verbose, TEXT("ClientReceiveWorkArea: Successfully received replicated WorkArea: %s"), *ClientArea->GetName());
	
	if (!OwningPlayerController)
	{
		UE_LOG(LogTemp, Verbose, TEXT("No OwningPlayerController"));
		return;
	}

	AExtendedControllerBase* ExtendedControllerBase = Cast<AExtendedControllerBase>(OwningPlayerController);
	
	if (!ExtendedControllerBase)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to get owning player controller."));
		return;
	}

	if( ExtendedControllerBase->SelectableTeamId == TeamId)
	{
		// Special handling for Extension Areas with EExtensionSnapMethod::None: skip mouse jump
		bool bSkipMouseJump = false;
		if (ClientArea->IsExtensionArea)
		{
			ABuildingBase* Unit = Base;
			if (Unit && Unit->ExtensionSnapMethod == EExtensionSnapMethod::None)
			{
				bSkipMouseJump = true;
			}
		}

		if (!bSkipMouseJump)
		{
			FVector MousePosition, MouseDirection;
			ExtendedControllerBase->DeprojectMousePositionToWorld(MousePosition, MouseDirection);

			// Raycast from the mouse position into the scene to find the ground
			FVector Start = MousePosition;
			FVector End = Start + MouseDirection * 1000000.f; // Extend to a maximum reasonable distance

			FHitResult HitResult;
			FCollisionQueryParams CollisionParams;
			CollisionParams.bTraceComplex = true; // Use complex collision for precise tracing

			// Perform the raycast
			bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, CollisionParams);

			if (bHit)
			{
				ClientArea->SetActorLocation(HitResult.Location);
			}
		}
	}
}

void AWorkingUnitBase::SetCharacterVisibility(bool desiredVisibility)
{
	Super::SetCharacterVisibility(desiredVisibility);
	// WorkResource mesh visibility is handled in SyncAttachedAssetsVisibility called by UUnitVisibilityProcessor.
}

void AWorkingUnitBase::SyncAttachedAssetsVisibility()
{
	if (WorkResource && WorkResource->Mesh)
	{
		const bool bCarry = WorkResource->IsAttached;
		const bool bShow = bCarry && ComputeLocalVisibility();
		WorkResource->Mesh->SetVisibility(bShow, true);
		WorkResource->Mesh->SetHiddenInGame(!bShow);
	}
}
