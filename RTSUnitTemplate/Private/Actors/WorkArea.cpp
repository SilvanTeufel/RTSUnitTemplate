// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Actors/WorkArea.h"

#include "Characters/Unit/BuildingBase.h"
#include "EngineUtils.h"   // TActorIterator (AbandonIfUnclaimed: find a worker still walking here)
#include "Controller/PlayerController/ControllerBase.h"   // bIsAi / SelectableTeamId (AI-only orphan cleanup)
#include "Core/WorkerData.h"
#include "Characters/Unit/UnitBase.h"
#include "Components/CapsuleComponent.h"
#include "GameModes/ResourceGameMode.h"
#include "Net/UnrealNetwork.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Characters/Unit/WorkingUnitBase.h"
#include "Mass/Signals/MySignals.h"   // UnitSignals::Idle / GoToResourceExtraction (ReserveMiningSlotOrReassign follow-up)
#include "Engine/Texture.h"


// Sets default values
AWorkArea::AWorkArea()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	/*
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	*/
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot); // Set it as the root component
	
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(SceneRoot);
	//SetRootComponent(Mesh);
	
	// Set collision enabled 
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly); // Query Only (No Physics Collision)
	Mesh->SetCollisionObjectType(ECC_WorldStatic); // Object Type: WorldStatic
	Mesh->SetGenerateOverlapEvents(true);

	// Set collision responses
	Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block); // Visibility: Block
	Mesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Overlap); // Camera: Overlap
	Mesh->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Overlap); // WorldStatic: Overlap
	Mesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap); // WorldDynamic: Overlap
	Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap); // Pawn: Overlap
	Mesh->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap); // PhysicsBody: Overlap
	Mesh->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Overlap); // Vehicle: Overlap
	Mesh->SetCollisionResponseToChannel(ECC_Destructible, ECR_Overlap); // Destructible: Overlap



	
	TriggerCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("TriggerCapsule"));
	TriggerCapsule->SetupAttachment(SceneRoot);
	TriggerCapsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TriggerCapsule->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerCapsule->InitCapsuleSize(100.f, 100.0f);

	//SceneRoot->SetVisibility(false, true);
	
	//if (HasAuthority())
	//{
		bReplicates = true;
		//bAlwaysRelevant = true;
		//SetReplicateMovement(true);
	//}

	Mesh->SetIsReplicated(false); // was false
	MaxAvailableResourceAmount = AvailableResourceAmount;
	ShrinkResource = true;
}

// Called when the game starts or when spawned
void AWorkArea::BeginPlay()
{
	Super::BeginPlay();
	MaxAvailableResourceAmount = AvailableResourceAmount;
	OriginalActorScale = GetActorScale3D();
	SetReplicateMovement(false);
	if (HasAuthority())
	{
		InitWorkerOverflowTimer();

		// A build area nobody ever claims must not sit on the map forever - it blocks placement for
		// everyone (a stray BroodHive area even ended up on an enemy DataCenter). After the timeout it
		// is removed and, if it had already been paid for, refunded.
		if (Type == WorkAreaData::BuildArea && AbandonTimeoutSeconds > 0.f)
		{
			GetWorld()->GetTimerManager().SetTimer(
				AbandonTimerHandle, this, &AWorkArea::AbandonIfUnclaimed, AbandonTimeoutSeconds, false);
		}

		// Recurring orphan check for AI-owned areas. The AI ownership test lives inside the callback, not
		// here: a build area can spawn before the controllers are up, and asking too early would answer
		// "not AI" for every area and silently disable the whole thing.
		if (Type == WorkAreaData::BuildArea && AiOrphanTimeoutSeconds > 0.f && AiOrphanCheckInterval > 0.f)
		{
			GetWorld()->GetTimerManager().SetTimer(
				AiOrphanTimerHandle, this, &AWorkArea::TickAiOrphanCheck, AiOrphanCheckInterval, true);
		}
	}
}

bool AWorkArea::IsOwnedByAiTeam() const
{
	const UWorld* World = GetWorld();
	if (!World) return false;

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (const AControllerBase* PC = Cast<AControllerBase>(It->Get()))
		{
			if (PC->SelectableTeamId == TeamId)
			{
				return PC->bIsAi;
			}
		}
	}
	return false;
}

void AWorkArea::TickAiOrphanCheck()
{
	if (!HasAuthority()) return;
	if (Type != WorkAreaData::BuildArea) return;
	if (!IsOwnedByAiTeam()) return;   // the human player manages their own areas

	// Cheap claims first. Note that StartedBuilding is deliberately NOT one of them: an area whose
	// builder died mid-build keeps StartedBuilding=true forever and is precisely what has to go.
	if (ConstructionUnit || Building || bFinalBuildingSpawned || Workers.Num() > 0)
	{
		AiOrphanElapsed = 0.f;
		bAiOrphanWasClaimed = true;
		return;
	}

	// StartedBuilding/bConstructionUnitSpawned survive the death of the builder, so they are proof that
	// this area WAS claimed once - not proof that it still is. They only pick the timeout, never skip it.
	if (StartedBuilding || bConstructionUnitSpawned)
	{
		bAiOrphanWasClaimed = true;
	}

	// A unit still walking here counts as a claim - it carries the area as its BuildArea and shows up in
	// none of the fields above. Skipping this check is what made the one-shot abandon timer delete areas
	// whose builder was merely on the way, cutting the Xeno base from ~14-22 buildings down to 5-7.
	for (TActorIterator<AUnitBase> It(GetWorld()); It; ++It)
	{
		const AUnitBase* Unit = *It;
		if (IsValid(Unit) && Unit->BuildArea == this)
		{
			AiOrphanElapsed = 0.f;
			bAiOrphanWasClaimed = true;
			return;
		}
	}

	AiOrphanElapsed += AiOrphanCheckInterval;

	// Nobody is on this area, so it must not keep claiming to be spoken for: GetClosestBuildPlaces filters
	// out everything with PlannedBuilding=true, which means a flag left over from a builder that never
	// arrived hides the area from every future worker. Clearing it gives the area one honest chance to be
	// picked up again before the timeout below removes it.
	if (PlannedBuilding && !StartedBuilding)
	{
		PlannedBuilding = false;
	}

	// An area that was claimed and lost its builder is dead and goes on the short timeout. One that was
	// never claimed at all may simply be queued behind a busy worker, so it gets the long grace period.
	const float Timeout = bAiOrphanWasClaimed ? AiOrphanTimeoutSeconds : AiUnclaimedTimeoutSeconds;
	if (Timeout <= 0.f || AiOrphanElapsed < Timeout) return;

	// Only refund what was actually taken - the cost is charged when a worker STARTS building.
	if (IsPaid)
	{
		if (AResourceGameMode* ResourceGameMode = Cast<AResourceGameMode>(GetWorld()->GetAuthGameMode()))
		{
			ResourceGameMode->ModifyResource(EResourceType::Primary,   TeamId, ConstructionCost.PrimaryCost);
			ResourceGameMode->ModifyResource(EResourceType::Secondary, TeamId, ConstructionCost.SecondaryCost);
			ResourceGameMode->ModifyResource(EResourceType::Tertiary,  TeamId, ConstructionCost.TertiaryCost);
			ResourceGameMode->ModifyResource(EResourceType::Rare,      TeamId, ConstructionCost.RareCost);
			ResourceGameMode->ModifyResource(EResourceType::Epic,      TeamId, ConstructionCost.EpicCost);
			ResourceGameMode->ModifyResource(EResourceType::Legendary, TeamId, ConstructionCost.LegendaryCost);
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[WorkArea] %s removed: orphaned %.0fs (AI team %d, wasClaimed=%d, started=%d, refunded=%d)."),
	       *GetName(), AiOrphanElapsed, TeamId, bAiOrphanWasClaimed ? 1 : 0, StartedBuilding ? 1 : 0, IsPaid ? 1 : 0);

	GetWorld()->GetTimerManager().ClearTimer(AiOrphanTimerHandle);
	Destroy();
}

void AWorkArea::AbandonIfUnclaimed()
{
	if (!HasAuthority()) return;

	// Claimed in any way? Then leave it alone: a worker started, the ConstructionUnit spawned, or the
	// finished building already exists.
	if (StartedBuilding || bConstructionUnitSpawned || ConstructionUnit || Building || Workers.Num() > 0)
	{
		return;
	}

	// A worker still WALKING here counts as assigned - it appears in none of the fields above, it only
	// carries this area as its BuildArea. Without this check the timer deleted areas whose builder was
	// simply still on the way; measured, it wiped CarapacePod/LarvalPod/BroodHive/Bunker areas
	// repeatedly and cut the Xeno base from ~14-22 buildings down to 5-7. The defense push (+2800)
	// made it worst for defense areas, which have the longest walk.
	for (TActorIterator<AUnitBase> It(GetWorld()); It; ++It)
	{
		AUnitBase* Unit = *It;
		if (IsValid(Unit) && Unit->BuildArea == this)
		{
			return;
		}
	}

	// Only refund what was actually taken - the cost is charged when a worker STARTS building, so an
	// area that never got that far was never paid for.
	if (IsPaid)
	{
		if (AResourceGameMode* ResourceGameMode = Cast<AResourceGameMode>(GetWorld()->GetAuthGameMode()))
		{
			ResourceGameMode->ModifyResource(EResourceType::Primary,   TeamId, ConstructionCost.PrimaryCost);
			ResourceGameMode->ModifyResource(EResourceType::Secondary, TeamId, ConstructionCost.SecondaryCost);
			ResourceGameMode->ModifyResource(EResourceType::Tertiary,  TeamId, ConstructionCost.TertiaryCost);
			ResourceGameMode->ModifyResource(EResourceType::Rare,      TeamId, ConstructionCost.RareCost);
			ResourceGameMode->ModifyResource(EResourceType::Epic,      TeamId, ConstructionCost.EpicCost);
			ResourceGameMode->ModifyResource(EResourceType::Legendary, TeamId, ConstructionCost.LegendaryCost);
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[WorkArea] %s abandoned after %.0fs (team %d, refunded=%d)."),
	       *GetName(), AbandonTimeoutSeconds, TeamId, IsPaid ? 1 : 0);

	Destroy();
}

void AWorkArea::InitWorkerOverflowTimer()
{
	if (Type == WorkAreaData::BuildArea && WorkerReturnDelay > 0.f)
	{
		GetWorld()->GetTimerManager().SetTimer(
			OverflowWorkersTimerHandle,
			this,
			&AWorkArea::OnOverflowTimer,
			WorkerReturnDelay,
			true // loop
		);
		return;
	}

	// Resource deposits need the same tick, for the stale-entry purge at the top of OnOverflowTimer rather
	// than for overflow. This timer used to be build-areas-only, which is precisely why deposits were the
	// places that accumulated phantom workers and kept reporting "1/3" with nobody mining.
	const bool bIsResourceType = (Type == WorkAreaData::Primary || Type == WorkAreaData::Secondary ||
		Type == WorkAreaData::Tertiary || Type == WorkAreaData::Rare || Type == WorkAreaData::Epic ||
		Type == WorkAreaData::Legendary);

	if (bIsResourceType && HasAuthority())
	{
		GetWorld()->GetTimerManager().SetTimer(
			OverflowWorkersTimerHandle,
			this,
			&AWorkArea::OnOverflowTimer,
			2.0f,
			true // loop
		);
	}
}

void AWorkArea::AddAreaToGroup_Implementation()
{
	AResourceGameMode* ResourceGameMode = Cast<AResourceGameMode>(GetWorld()->GetAuthGameMode());

	if(ResourceGameMode)
	{
		switch (Type)
		{
		case WorkAreaData::Primary:
			ResourceGameMode->WorkAreaGroups.PrimaryAreas.Add(this);
			break;
		case WorkAreaData::Secondary:
			ResourceGameMode->WorkAreaGroups.SecondaryAreas.Add(this);
			break;
		case WorkAreaData::Tertiary:
			ResourceGameMode->WorkAreaGroups.TertiaryAreas.Add(this);
			break;
		case WorkAreaData::Rare:
			ResourceGameMode->WorkAreaGroups.RareAreas.Add(this);
			break;
		case WorkAreaData::Epic:
			ResourceGameMode->WorkAreaGroups.EpicAreas.Add(this);
			break;
		case WorkAreaData::Legendary:
			ResourceGameMode->WorkAreaGroups.LegendaryAreas.Add(this);
			break;
		/*case WorkAreaData::Base:
			ResourceGameMode->WorkAreaGroups.BaseAreas.Add(this);
			break;*/
		case WorkAreaData::BuildArea:
			ResourceGameMode->WorkAreaGroups.BuildAreas.Add(this);
			break;
		default:
			// Handle any cases not explicitly covered
				break;
		}
	}
}

void AWorkArea::RemoveAreaFromGroup_Implementation()
{
	AResourceGameMode* ResourceGameMode = Cast<AResourceGameMode>(GetWorld()->GetAuthGameMode());

	if(ResourceGameMode)
	{
		switch (Type)
		{
		case WorkAreaData::Primary:
			ResourceGameMode->WorkAreaGroups.PrimaryAreas.Remove(this);
			break;
		case WorkAreaData::Secondary:
			ResourceGameMode->WorkAreaGroups.SecondaryAreas.Remove(this);
			break;
		case WorkAreaData::Tertiary:
			ResourceGameMode->WorkAreaGroups.TertiaryAreas.Remove(this);
			break;
		case WorkAreaData::Rare:
			ResourceGameMode->WorkAreaGroups.RareAreas.Remove(this);
			break;
		case WorkAreaData::Epic:
			ResourceGameMode->WorkAreaGroups.EpicAreas.Remove(this);
			break;
		case WorkAreaData::Legendary:
			ResourceGameMode->WorkAreaGroups.LegendaryAreas.Remove(this);
			break;
		/* case WorkAreaData::Base:
			ResourceGameMode->WorkAreaGroups.BaseAreas.Remove(this);
			break; */
		case WorkAreaData::BuildArea:
			ResourceGameMode->WorkAreaGroups.BuildAreas.Remove(this);
			break;
		default:
			// Handle any cases not explicitly covered
				break;
		}
	}
}
// Called every frame
void AWorkArea::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	if(Building && Building->GetUnitState() == UnitData::Dead)
	{
		PlannedBuilding = false;
		StartedBuilding = false;
		Building = nullptr;
	}

	// If our construction unit (e.g. the extension build drone) was killed mid-build, tear the site down too.
	if (ConstructionUnit && ConstructionUnit->GetUnitState() == UnitData::Dead)
	{
		Destroy();
		return;
	}

	
	ControlTimer += DeltaTime;
	if(ControlTimer >= ResetStartBuildTime)
	{
		if(!Building || (PlannedBuilding && !StartedBuilding))
		{
			if (Workers.Num() == 0)
			{
				PlannedBuilding = false;
				StartedBuilding = false;
			}
		}
		ControlTimer = 0.f;
	}

	if (bMIDEnabled && WorkAreaMID)
	{
		float Value = (BuildTime > 0.f) ? FMath::Pow(FMath::Clamp(CurrentBuildTime / BuildTime, 0.0f, 1.0f), MaterializePower) : 0.0f;
		WorkAreaMID->SetScalarParameterValue(MaterializeParameterName, Value);
	}
	
}

void AWorkArea::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWorkArea, AreaEffect);
	DOREPLIFETIME(AWorkArea, Mesh);
	DOREPLIFETIME(AWorkArea, TeamId);
	DOREPLIFETIME(AWorkArea, IsNoBuildZone);
	DOREPLIFETIME(AWorkArea, ConstructionUnitClass);
	DOREPLIFETIME(AWorkArea, ConstructionUnit);
	DOREPLIFETIME(AWorkArea, BuildingClass);
	DOREPLIFETIME(AWorkArea, ScaleConstructionUnit);
	DOREPLIFETIME(AWorkArea, DroneVerticalOffset);
	DOREPLIFETIME(AWorkArea, DroneBuildingHeightOverride);
	DOREPLIFETIME(AWorkArea, IsExtensionArea);
	DOREPLIFETIME(AWorkArea, Workers);
	DOREPLIFETIME(AWorkArea, MaxWorkerCount);
	DOREPLIFETIME(AWorkArea, CurrentWorkers);
	DOREPLIFETIME(AWorkArea, PlannedBuilding);
	DOREPLIFETIME(AWorkArea, StartedBuilding);
	DOREPLIFETIME(AWorkArea, bMIDEnabled);
	DOREPLIFETIME(AWorkArea, BuildTime);
	DOREPLIFETIME(AWorkArea, CurrentBuildTime);
	DOREPLIFETIME(AWorkArea, AvailableResourceAmount);
	DOREPLIFETIME(AWorkArea, MaxAvailableResourceAmount);
	DOREPLIFETIME(AWorkArea, ShrinkResource);
	DOREPLIFETIME(AWorkArea, AreaDropped);
}

void AWorkArea::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    // Early return if OtherActor is not a AWorkingUnitBase
    AWorkingUnitBase* Worker = Cast<AWorkingUnitBase>(OtherActor);
    if (!Worker) return;

    // Since Worker is already AWorkingUnitBase, no need to cast again
    AUnitBase* UnitBase = Cast<AUnitBase>(Worker);
    if (!UnitBase) return;

    // Check for work area types that involve resource extraction
    bool isResourceExtractionArea = Type == WorkAreaData::Primary || Type == WorkAreaData::Secondary || 
                                     Type == WorkAreaData::Tertiary || Type == WorkAreaData::Rare ||
                                     Type == WorkAreaData::Epic || Type == WorkAreaData::Legendary;
    bool isValidStateForExtraction = UnitBase->GetUnitState() == UnitData::GoToResourceExtraction || 
                                     UnitBase->GetUnitState() == UnitData::Evasion;
	
	AResourceGameMode* ResourceGameMode = Cast<AResourceGameMode>(GetWorld()->GetAuthGameMode());

	if(!ResourceGameMode) return;

	bool CanAffordConstruction;
	
	if(IsPaid)
		CanAffordConstruction = true;
	else	
		CanAffordConstruction = Worker->BuildArea? ResourceGameMode->CanAffordConstruction(Worker->BuildArea->ConstructionCost, Worker->TeamId) : false;//Worker->BuildArea->CanAffordConstruction(Worker->TeamId, ResourceGameMode->NumberOfTeams,ResourceGameMode->TeamResources) : false;
	
    if (isResourceExtractionArea && isValidStateForExtraction && Worker->GetUnitState() != UnitData::GoToBuild)
    {
        HandleResourceExtractionArea(UnitBase);
    }
    else if (Type == WorkAreaData::Base && ResourceGameMode && Worker->GetUnitState() != UnitData::GoToBuild)
    {
        HandleBaseArea(Worker, UnitBase, ResourceGameMode, CanAffordConstruction);
    }
    else if (Type == WorkAreaData::BuildArea && ResourceGameMode)
    {
        HandleBuildArea(Worker, UnitBase, ResourceGameMode, CanAffordConstruction);
    }
}

void AWorkArea::HandleResourceExtractionArea(AUnitBase* UnitBase)
{

		if (this != UnitBase->ResourcePlace) return;

		// Final gate: whatever route set ResourcePlace, extraction never starts on a resource this
		// worker is not permitted to gather.
		if (!UnitBase->CanMineWorkArea(this)) return;

		// Enforce the per-resource worker cap: an overflow worker is reassigned/idled here (returns
		// false) instead of starting extraction.
		if (AWorkingUnitBase* Worker = Cast<AWorkingUnitBase>(UnitBase))
		{
			if (!ReserveMiningSlotOrReassign(Worker)) return;
		}

		UnitBase->UnitControlTimer = 0;
		UnitBase->ExtractingWorkResourceType = ConvertWorkAreaTypeToResourceType(Type);
		UnitBase->SetUnitState(UnitData::ResourceExtraction);
		StartedResourceExtraction();
}

void AWorkArea::HandleBaseArea(AWorkingUnitBase* Worker, AUnitBase* UnitBase, AResourceGameMode* ResourceGameMode, bool CanAffordConstruction)
{
	
			UnitBase->UnitControlTimer = 0;
			UnitBase->SetUEPathfinding = true;

			// Remove from current resource place while at base to allow better redistribution
			if (IsValid(Worker->ResourcePlace))
			{
				Worker->ResourcePlace->RemoveWorkerFromArray(Worker);
			}

			if (IsValid(Worker->BuildArea))
			{
				Worker->BuildArea->RemoveWorkerFromArray(Worker);
			}
	
			if(Worker->WorkResource)
			{
				//ResourceGameMode->ModifyTeamResourceAttributes(Worker->TeamId, Worker->WorkResource->ResourceType, Worker->WorkResource->Amount);
				ResourceGameMode->ModifyResource(Worker->WorkResource->ResourceType, Worker->TeamId, Worker->WorkResource->Amount);
				DespawnWorkResource(UnitBase->WorkResource);
			}
	
	
			if (!SwitchBuildArea(Worker, UnitBase, ResourceGameMode))
			{
				SwitchResourceArea(Worker, UnitBase, ResourceGameMode);
			}
	
	
}

void AWorkArea::SwitchResourceArea(AWorkingUnitBase* Worker, AUnitBase* UnitBase, AResourceGameMode* ResourceGameMode)
{
	if (!ResourceGameMode) return;

	// A held-but-depleted deposit (emptied without being destroyed, e.g. loaded from a save) is not
	// extractable. Release it up front - decrement our per-type worker slot (symmetric with the +1
	// paid on assignment) and forget it - so the logic below treats this worker as unassigned and
	// never re-registers it onto a dead deposit. GetAllResourcePlaces already filters such places.
	// Also release a deposit this worker is no longer permitted to mine (its MineableResourceTypes
	// changed, or no base of its team accepts that resource any more). Without this the "keep the
	// current resource" branch further down would pin the worker to a now-forbidden deposit forever.
	const bool bHoldsForbiddenPlace =
		IsValid(UnitBase->ResourcePlace) &&
		(!UnitBase->CanMineWorkArea(UnitBase->ResourcePlace) ||
		 !ResourceGameMode->CanTeamDeliverResourceType(UnitBase->TeamId, ConvertToResourceType(UnitBase->ResourcePlace->Type)));

	if (IsValid(UnitBase->ResourcePlace) && (UnitBase->ResourcePlace->AvailableResourceAmount <= 0.f || bHoldsForbiddenPlace))
	{
		ResourceGameMode->AddCurrentWorkersForResourceType(UnitBase->TeamId, ConvertToResourceType(UnitBase->ResourcePlace->Type), -1.0f);
		UnitBase->ResourcePlace->RemoveWorkerFromArray(Worker);
		UnitBase->ResourcePlace = nullptr;
	}

	const bool bWorkerDistributionSet = ResourceGameMode->IsWorkerDistributionSet(Worker->TeamId);

	TArray<AWorkArea*> AllWorkPlaces = ResourceGameMode->GetAllResourcePlaces(Worker);

	if (AllWorkPlaces.Num() == 0)
	{
		UnitBase->SetUEPathfinding = true;
		Worker->SetUnitState(UnitData::Idle);
		return;
	}

	const FVector BaseLocation = IsValid(UnitBase->Base) ? UnitBase->Base->GetActorLocation() : UnitBase->GetActorLocation();
	const float ClosestDistance = FVector::Dist(BaseLocation, AllWorkPlaces[0]->GetActorLocation());
	const float DistanceThreshold = ClosestDistance * ResourceGameMode->ResourceDistanceMultiplier;

	TArray<AWorkArea*> CloseWorkPlaces;
	for (AWorkArea* WorkPlace : AllWorkPlaces)
	{
		if (!IsValid(WorkPlace)) continue;
		const float WorkPlaceDistance = FVector::Dist(BaseLocation, WorkPlace->GetActorLocation());
		if (WorkPlaceDistance <= DistanceThreshold)
		{
			CloseWorkPlaces.Add(WorkPlace);
		}
	}

	if (CloseWorkPlaces.Num() == 0 && AllWorkPlaces.Num() > 0)
	{
		CloseWorkPlaces.Add(AllWorkPlaces[0]);
	}

	TArray<AWorkArea*> WorkPlacesForDistribution = bWorkerDistributionSet ? AllWorkPlaces : CloseWorkPlaces;

	AWorkArea* NewResourcePlace = ResourceGameMode->GetSuitableWorkAreaToWorker(Worker->TeamId, WorkPlacesForDistribution);

	if (NewResourcePlace)
	{
		if(UnitBase->ResourcePlace && UnitBase->ResourcePlace->Type != NewResourcePlace->Type)
		{
			ResourceGameMode->AddCurrentWorkersForResourceType(UnitBase->TeamId, ConvertToResourceType(UnitBase->ResourcePlace->Type), -1.0f);
			ResourceGameMode->AddCurrentWorkersForResourceType(UnitBase->TeamId, ConvertToResourceType(NewResourcePlace->Type), +1.0f);
		}
		else if(!UnitBase->ResourcePlace)
		{
			ResourceGameMode->AddCurrentWorkersForResourceType(UnitBase->TeamId, ConvertToResourceType(NewResourcePlace->Type), +1.0f);
		}
		// SetResourcePlace, not a raw assignment: HandleBaseArea releases the old slot on the normal
		// route, but SwitchResourceArea is also reached from paths that did not (the AutoMining rescan,
		// a re-pick after the old deposit was destroyed). A raw assignment there left the worker in the
		// old node's Workers array and produced a phantom "N/Max".
		UnitBase->SetResourcePlace(NewResourcePlace, /*bRegisterOnNewPlace=*/true);
	}
	else if (!UnitBase->ResourcePlace)
	{
		// Fallback: pick the one with fewest workers from close list (extra workers)
		AWorkArea* BestFallback = nullptr;
		int32 LowestWorkerCount = INT_MAX;
		
		for (AWorkArea* WorkPlace : CloseWorkPlaces)
		{
			if (!IsValid(WorkPlace)) continue;

			// For extra workers (who fall back here), we ignore the distribution type check
			// as they should just pick any close resource that has space.
			
			const int32 WorkerCount = WorkPlace->Workers.Num();
			if (WorkerCount < LowestWorkerCount && WorkerCount < WorkPlace->MaxWorkerCount)
			{
				LowestWorkerCount = WorkerCount;
				BestFallback = WorkPlace;
			}
		}

		// No uncapped second fallback here: taking a node that is already at MaxWorkerCount only sends
		// the worker on a walk that ends in ReserveMiningSlotOrReassign bouncing it straight back.
		// Leaving BestFallback null idles the worker instead; the AutoMining rescan retries later.

		if (BestFallback)
		{
			ResourceGameMode->AddCurrentWorkersForResourceType(UnitBase->TeamId, ConvertToResourceType(BestFallback->Type), +1.0f);
			UnitBase->SetResourcePlace(BestFallback, /*bRegisterOnNewPlace=*/true);
		}
		else
		{
			UnitBase->SetUEPathfinding = true;
			Worker->SetUnitState(UnitData::Idle);
			return;
		}
	}
	else
	{
		// Even if keeping the same resource, re-register it since we removed it in HandleBaseArea
		if (IsValid(UnitBase->ResourcePlace))
		{
			UnitBase->ResourcePlace->AddWorkerToArray(Worker);
		}
	}

	UnitBase->SetUEPathfinding = true;
	Worker->SetUnitState(UnitData::GoToResourceExtraction);
	Worker->SwitchEntityTagByState(UnitData::GoToResourceExtraction, Worker->UnitStatePlaceholder);
}

bool AWorkArea::SwitchBuildArea(AWorkingUnitBase* Worker, AUnitBase* UnitBase, AResourceGameMode* ResourceGameMode)
{
	if (!HasAuthority()) return false;
	// A worker that still carries an unfinished build area RESUMES it instead of re-rolling.
	// ABuildingBase::HandleBaseArea unregisters the worker from its area right before calling in here, and
	// GetClosestBuildPlaces hides every area with PlannedBuilding=true - including this worker's own. So
	// whenever nothing else happened to be free, the worker silently dropped the job it already had, and
	// the area was left behind with PlannedBuilding=true forever: invisible to every future worker, no
	// builder, never built. Measured: 15 of 21 areas removed by the orphan cleanup carried exactly this
	// signature (wasClaimed=1, started=0 - a worker had been assigned and nothing was ever built).
	if (IsValid(Worker->BuildArea)
		&& !Worker->BuildArea->IsExtensionArea
		&& !Worker->BuildArea->bFinalBuildingSpawned
		&& !IsValid(Worker->BuildArea->Building)
		&& (Worker->BuildArea->TeamId == 0 || Worker->BuildArea->TeamId == Worker->TeamId))
	{
		AWorkArea* Existing = Worker->BuildArea;
		Worker->ReleaseResourcePlace();
		Existing->PlannedBuilding = true;
		Existing->AddWorkerToArray(Worker);
		UnitBase->SetUEPathfinding = true;
		Worker->SetUnitState(UnitData::GoToBuild);
		Worker->SwitchEntityTagByState(UnitData::GoToBuild, Worker->UnitStatePlaceholder);
		return true;
	}

	TArray<AWorkArea*> BuildAreas = ResourceGameMode->GetClosestBuildPlaces(Worker);

	// Keep only the three closest candidates - but SHRINK ONLY. SetNum(3) also PADS with nullptr when
	// fewer than three are available, and GetRandomClosestWorkArea returns whatever slot it draws. With
	// a single free build area the worker was therefore turned away two times out of three although the
	// area was right in front of it: BuildArea=nullptr, walk back to base, try again. That is the
	// "workers get sent back and forth instead of gathering" report, and it also produced build areas
	// that never got a builder at all.
	if (BuildAreas.Num() > 3)
	{
		BuildAreas.SetNum(3);
	}

	AWorkArea* SelectedArea = ResourceGameMode->GetRandomClosestWorkArea(BuildAreas);
	if (!SelectedArea || SelectedArea->IsExtensionArea)
	{
		Worker->BuildArea = nullptr;
		return false;
	}

	bool CanAffordConstruction;
	if(IsPaid)
		CanAffordConstruction = true;
	else	
		CanAffordConstruction = ResourceGameMode->CanAffordConstruction(SelectedArea->ConstructionCost, Worker->TeamId);

	bool AreaIsForTeam = (SelectedArea->TeamId == 0) || (Worker->TeamId == SelectedArea->TeamId);

	if(CanAffordConstruction && AreaIsForTeam)
	{
		// Only if no one is working there yet, or we can add more workers
		bool bCanWork = false;
		if (SelectedArea->Workers.Num() == 0 && !SelectedArea->PlannedBuilding)
		{
			SelectedArea->PlannedBuilding = true;
			SelectedArea->ControlTimer = 0.f;
			bCanWork = true;
		}
		else if (SelectedArea->AllowAddingWorkers && SelectedArea->Workers.Num() < SelectedArea->MaxWorkerCount)
		{
			bCanWork = true;
		}

		if (bCanWork)
		{
			// Leaving to build means it stops mining, so give the deposit's slot back. SendWorkerToWorkArea
			// (the explicit player order) already did this; this auto-assignment path - the one the AI takes
			// via ReachedBase - never did. The worker stayed registered on its deposit forever, which is why
			// nodes kept showing "1/3" for workers that had long since walked off, and why the slots filled
			// up with phantoms until nobody could mine there any more.
			Worker->ReleaseResourcePlace();

			Worker->BuildArea = SelectedArea;
			SelectedArea->AddWorkerToArray(Worker); // Reserve early
			UnitBase->SetUEPathfinding = true;
			Worker->SetUnitState(UnitData::GoToBuild);
			Worker->SwitchEntityTagByState(UnitData::GoToBuild, Worker->UnitStatePlaceholder);
			return true;
		}
		else
		{
		}
	}
	
	Worker->BuildArea = nullptr;
	return false;
}

void AWorkArea::HandleBuildArea(AWorkingUnitBase* Worker, AUnitBase* UnitBase, AResourceGameMode* ResourceGameMode, bool CanAffordConstruction)
{
		
		if(!ResourceGameMode || !Worker) return; // Exit if the cast fails or game mode is not set

		bool AreaIsForTeam = false;
		if (Worker->BuildArea)
		{
			AreaIsForTeam = 
				(Worker->BuildArea->TeamId == 0) || 
				(Worker->TeamId == Worker->BuildArea->TeamId);
		}

		if(this == Worker->BuildArea && !IsExtensionArea && CanAffordConstruction && Building == nullptr && !StartedBuilding && AreaIsForTeam)
		{
			if (Workers.Num() >= MaxWorkerCount && !Workers.Contains(Worker))
			{
				SwitchBuildArea(Worker, UnitBase, ResourceGameMode);
				return;
			}

			StartedBuilding = true;
			StartedBuild();


			if(!IsPaid)
			{
				ResourceGameMode->ModifyResource(EResourceType::Primary, Worker->TeamId, -Worker->BuildArea->ConstructionCost.PrimaryCost);
				ResourceGameMode->ModifyResource(EResourceType::Secondary, Worker->TeamId, -Worker->BuildArea->ConstructionCost.SecondaryCost);
				ResourceGameMode->ModifyResource(EResourceType::Tertiary, Worker->TeamId, -Worker->BuildArea->ConstructionCost.TertiaryCost);
				ResourceGameMode->ModifyResource(EResourceType::Rare, Worker->TeamId, -Worker->BuildArea->ConstructionCost.RareCost);
				ResourceGameMode->ModifyResource(EResourceType::Epic, Worker->TeamId, -Worker->BuildArea->ConstructionCost.EpicCost);
				ResourceGameMode->ModifyResource(EResourceType::Legendary, Worker->TeamId, -Worker->BuildArea->ConstructionCost.LegendaryCost);
			}
			
			UnitBase->UnitControlTimer = 0;
			UnitBase->SetUEPathfinding = true;
			if (UnitBase->BuildArea)
			{
				UnitBase->CastTime = UnitBase->BuildArea->BuildTime;
			}
			UnitBase->SetUnitState(UnitData::Build);
		}else if (this == Worker->BuildArea && (Building != nullptr || !StartedBuilding) && CanAffordConstruction)
		{
			SwitchBuildArea(Worker, UnitBase, ResourceGameMode);
		}else if(this == Worker->BuildArea &&  Worker->GetUnitState() != UnitData::Build)
		{
			UnitBase->SetUEPathfinding = true;
			if(Worker->WorkResource)
			{
				Worker->SetUnitState(UnitData::GoToBase);
				Worker->SwitchEntityTagByState(UnitData::GoToBase, Worker->UnitStatePlaceholder);
			}
			else
			{
				Worker->SetUnitState(UnitData::GoToResourceExtraction);
				Worker->SwitchEntityTagByState(UnitData::GoToResourceExtraction, Worker->UnitStatePlaceholder);
			}
		}

}

EResourceType AWorkArea::ConvertWorkAreaTypeToResourceType(WorkAreaData::WorkAreaType WorkAreaType)
{
    switch (WorkAreaType)
    {
    case WorkAreaData::Primary: return EResourceType::Primary;
    case WorkAreaData::Secondary: return EResourceType::Secondary;
    case WorkAreaData::Tertiary: return EResourceType::Tertiary;
    case WorkAreaData::Rare: return EResourceType::Rare;
    case WorkAreaData::Epic: return EResourceType::Epic;
    case WorkAreaData::Legendary: return EResourceType::Legendary;
    default: return EResourceType::Primary; // Assuming EResourceType::None exists for error handling
    }
}


void AWorkArea::DespawnWorkResource(AWorkResource* WorkResource)
{
	if (WorkResource != nullptr)
	{
		WorkResource->Destroy();
		WorkResource = nullptr;
		//WorkResource->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	}
}

void AWorkArea::Multicast_SetScale_Implementation(FVector NewScale)
{
	SetActorScale3D(OriginalActorScale * NewScale);
}

void AWorkArea::TemporarilyChangeMaterial()
{
    // Ensure the Mesh component and the temporary material are valid
    if (!Mesh || !TemporaryHighlightMaterial)
    {
        UE_LOG(LogTemp, Warning, TEXT("AWorkArea::TemporarilyChangeMaterial: Mesh or TemporaryHighlightMaterial is not set."));
        return;
    }

    // If a revert timer is already active, clear it. This handles rapidly calling the function.
    GetWorld()->GetTimerManager().ClearTimer(ChangeMaterialTimerHandle);

    // If we haven't stored the original material yet, store it now.
    // This prevents overwriting the true original material if the function is called multiple times.
    if (!OriginalMaterial)
    {
        // We assume the material is on element index 0.
        OriginalMaterial = Mesh->GetMaterial(0); 
    }
    
    // Apply the temporary material
    Mesh->SetMaterial(0, TemporaryHighlightMaterial);

    // Set a timer to call the RevertMaterial function after 3.0 seconds
    const float RevertDelay = 0.25f;
    GetWorld()->GetTimerManager().SetTimer(
        ChangeMaterialTimerHandle,      // The handle to manage this timer
        this,                           // The object to call the function on
        &AWorkArea::RevertMaterial,     // The function to call
        RevertDelay,                    // The delay in seconds
        false                           // Do not loop
    );
}

/**
 * Reverts the material back to the one stored in OriginalMaterial.
 */
void AWorkArea::RevertMaterial()
{
    // Ensure the Mesh and the stored OriginalMaterial are still valid
    if (Mesh && OriginalMaterial)
    {
        // Apply the original material back to the mesh
        Mesh->SetMaterial(0, OriginalMaterial);
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("AWorkArea::RevertMaterial: Could not revert material as Mesh or OriginalMaterial is no longer valid."));
    }
    
    // Clear the stored material pointer now that we are done with it.
    // This makes the logic in TemporarilyChangeMaterial correct for the next time it's called.
    OriginalMaterial = nullptr;

    // It's good practice to clear the handle after the timer has finished its job.
    GetWorld()->GetTimerManager().ClearTimer(ChangeMaterialTimerHandle);
}

void AWorkArea::EnableMID()
{
	if (!bMIDEnabled)
	{
		bMIDEnabled = true;
		SetupMID();
	}
}

void AWorkArea::OnRep_MIDEnabled()
{
	SetupMID();
}

void AWorkArea::SetupMID()
{
	if (Mesh && !WorkAreaMID)
	{
		UMaterialInterface* SourceMaterial = BuildMaterial ? BuildMaterial : Mesh->GetMaterial(0);
		if (SourceMaterial)
		{
			WorkAreaMID = Mesh->CreateDynamicMaterialInstance(0, SourceMaterial);
			WorkAreaMID->SetScalarParameterValue(OffsetParameterName, OffsetParameterValue);
			if (BaseTexParameterValue)
				WorkAreaMID->SetTextureParameterValue(BaseTexParameterName, BaseTexParameterValue);
			if (MetallicTexParameterValue)
				WorkAreaMID->SetTextureParameterValue(MetallicTexParameterName, MetallicTexParameterValue);
			if (NormalTexParameterValue)
				WorkAreaMID->SetTextureParameterValue(NormalTexParameterName, NormalTexParameterValue);
			if (SpecTexParameterValue)
				WorkAreaMID->SetTextureParameterValue(SpecTexParameterName, SpecTexParameterValue);
			if (!GetWorld()->GetTimerManager().IsTimerActive(ChangeMaterialTimerHandle))
			{
				Mesh->SetMaterial(0, WorkAreaMID);
			}
			else
			{
				OriginalMaterial = WorkAreaMID;
			}
		}
	}
}

void AWorkArea::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(OverflowWorkersTimerHandle);
	}
}

// --- Worker array management and return timer ---
void AWorkArea::AddWorkerToArray(AWorkingUnitBase* Worker)
{
	if (!Worker) return;

	// Avoid duplicates only; hard capacity is enforced at the mining slot (ReserveMiningSlotOrReassign).
	if (Workers.Contains(Worker))
	{
		return; // already tracked
	}

	Workers.Add(Worker);
	CurrentWorkers = Workers.Num(); // replicated -> drives the HUD N/Max display
	// Timer runs independently (looping). No immediate action needed here.
}

void AWorkArea::RemoveWorkerFromArray(AWorkingUnitBase* Worker)
{
	if (!Worker) return;
	Workers.Remove(Worker);
	CurrentWorkers = Workers.Num();
	// Timer runs independently (looping). No immediate action needed here.
}

void AWorkArea::OnRep_WorkerCount()
{
	// Replicated count changed on a client; the HUD reads CurrentWorkers directly each frame, so
	// nothing to do here. Hook kept for future per-node client-side reactions.
}

bool AWorkArea::HasFreeMiningSlotFor(const AWorkArea* Area, const AWorkingUnitBase* Worker)
{
	if (!IsValid(Area) || Area->AvailableResourceAmount <= 0.f)
	{
		return false;
	}

	// MaxWorkerCount <= 0 means "unlimited".
	if (Area->MaxWorkerCount <= 0)
	{
		return true;
	}

	// Already registered here -> the worker owns one of the slots, it does not need a new one.
	if (Worker && Area->Workers.Contains(Worker))
	{
		return true;
	}

	return Area->Workers.Num() < Area->MaxWorkerCount;
}

bool AWorkArea::ReserveMiningSlotOrReassign(AWorkingUnitBase* Worker, FName* OutFollowUpSignal)
{
	if (OutFollowUpSignal)
	{
		*OutFollowUpSignal = NAME_None;
	}

	if (!Worker) return true;
	if (!HasAuthority()) return true; // server owns the slot decision; clients just render

	// Make sure this worker is tracked here, then decide by its position in the slot order.
	if (!Workers.Contains(Worker))
	{
		AddWorkerToArray(Worker);
	}

	const int32 SlotIndex = Workers.IndexOfByKey(Worker);
	// MaxWorkerCount <= 0 means "unlimited". Otherwise only the first MaxWorkerCount workers may mine.
	if (MaxWorkerCount <= 0 || (SlotIndex >= 0 && SlotIndex < MaxWorkerCount))
	{
		return true; // within capacity -> mine here
	}

	// --- Overflow: this worker exceeds the cap on this node -> give up the slot and reassign. ---
	RemoveWorkerFromArray(Worker);

	AResourceGameMode* RGM = Cast<AResourceGameMode>(GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr);

	// First try a nearby deposit of the SAME type (cheapest, keeps the worker's job unchanged).
	AWorkArea* Alt = RGM ? RGM->GetNearestAvailableResourceOfTypeWithin(Worker, Type, 3000.f) : nullptr;

	// Widen the search when that fails: ANY deposit this worker is allowed to work and that still has a
	// free slot, at any distance. GetAllResourcePlaces is already permission-filtered and distance
	// sorted. Without this the same-type/3000-unit window almost never hit on a normal map, so the
	// overflow worker went Idle and stopped contributing entirely.
	if (!Alt && RGM)
	{
		for (AWorkArea* Candidate : RGM->GetAllResourcePlaces(Worker))
		{
			if (Candidate != this && HasFreeMiningSlotFor(Candidate, Worker))
			{
				Alt = Candidate;
				break;
			}
		}
	}

	// Release the per-type team slot we paid for on this node's type before moving on.
	if (RGM && IsValid(Worker->ResourcePlace))
	{
		RGM->AddCurrentWorkersForResourceType(Worker->TeamId, ConvertToResourceType(Worker->ResourcePlace->Type), -1.0f);
	}

	// Which state the worker should end up in. Applied directly here for the actor-overlap caller, or
	// handed back via OutFollowUpSignal for the Mass caller (see the header comment).
	TEnumAsByte<UnitData::EState> NewState = UnitData::Idle;
	FName FollowUpSignal = UnitSignals::Idle;

	if (Alt && RGM)
	{
		Worker->SetResourcePlace(Alt, /*bRegisterOnNewPlace=*/true);
		RGM->AddCurrentWorkersForResourceType(Worker->TeamId, ConvertToResourceType(Alt->Type), +1.0f);
		NewState = UnitData::GoToResourceExtraction;
		FollowUpSignal = UnitSignals::GoToResourceExtraction;
	}
	else
	{
		// Nothing this worker may work has room -> idle. SynchronizeUnitState's AutoMining rescan
		// retries later, once a slot frees up.
		Worker->SetResourcePlace(nullptr);
	}

	if (AUnitBase* WU = Cast<AUnitBase>(Worker)) WU->SetUEPathfinding = true;
	Worker->SetUnitState(NewState);

	if (OutFollowUpSignal)
	{
		// Caller is inside a command-buffer window that would strip the tag again - let it re-signal.
		*OutFollowUpSignal = FollowUpSignal;
	}
	else
	{
		Worker->SwitchEntityTagByState(NewState, Worker->UnitStatePlaceholder);
	}
	return false; // do NOT mine here
}

void AWorkArea::OnOverflowTimer()
{
	// Purge stale entries first. A worker can leave this deposit through many routes - dying, morphing into
	// a building, being re-tasked - and every one of them has to remember to unregister. They did not, which
	// left nodes advertising "1/3" with nobody actually mining and eventually filled every slot with ghosts
	// so real workers were turned away. Rather than trust each caller, verify: an entry is only valid while
	// the worker exists and still points back at this area.
	{
		// Which pointer proves membership depends on what this area IS: a deposit is referenced through
		// ResourcePlace, a construction site through BuildArea. Checking only ResourcePlace purged every
		// worker from every build site within seconds - construction stopped dead for both factions.
		const bool bIsBuildArea = (Type == WorkAreaData::BuildArea);

		const int32 Before = Workers.Num();
		Workers.RemoveAll([this, bIsBuildArea](AWorkingUnitBase* Worker)
		{
			if (!IsValid(Worker))
			{
				return true;
			}
			return bIsBuildArea ? (Worker->BuildArea != this) : (Worker->ResourcePlace != this);
		});

		if (!bIsBuildArea)
		{
			// A worker sent off to construct still points at this deposit through ResourcePlace, so the
			// check above keeps it - and the node keeps advertising an occupied slot for someone who is
			// nowhere near it. Releasing the slot here is deliberately independent of the assignment path:
			// there are several places that hand a worker a BuildArea, and relying on each of them to
			// release first is what produced the stale "1/3" in the first place. Collect before releasing,
			// because ReleaseResourcePlace mutates Workers.
			// The state has to agree, not just the pointer: BuildArea is cleared by a good dozen call sites
			// and a leftover value on a worker that is mining again would otherwise get its slot revoked
			// every two seconds.
			TArray<AWorkingUnitBase*> Builders;
			for (AWorkingUnitBase* Worker : Workers)
			{
				if (!IsValid(Worker) || Worker->BuildArea == nullptr)
				{
					continue;
				}

				const TEnumAsByte<UnitData::EState> State = Worker->GetUnitState();
				if (State == UnitData::GoToBuild || State == UnitData::Build || State == UnitData::Casting)
				{
					Builders.Add(Worker);
				}
			}
			for (AWorkingUnitBase* Worker : Builders)
			{
				Worker->ReleaseResourcePlace();
			}
		}

		if (Workers.Num() != Before)
		{
			CurrentWorkers = Workers.Num();

			// The per-team totals the HUD reads are maintained separately, so they need the same correction.
			if (AResourceGameMode* ResourceGameMode = Cast<AResourceGameMode>(GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr))
			{
				ResourceGameMode->SetAllCurrentWorkers(TeamId);
			}
		}
	}

	// Process overflow: send extra workers back until within capacity
	const int32 Allowed = MaxWorkerCount;
	if (Allowed <= 0)
	{
		// Unlimited capacity, nothing to do
		return;
	}

	// While more workers than allowed, remove from the end
	while (Workers.Num() > Allowed)
	{
		AWorkingUnitBase* Worker = Workers.Last();
		if (!Worker)
		{
			Workers.Pop();
			continue;
		}

		if (AUnitBase* Unit = Cast<AUnitBase>(Worker))
		{
			Unit->SwitchEntityTagByState(UnitData::GoToBase, Unit->UnitStatePlaceholder);
			if (AWorkingUnitBase* W = Cast<AWorkingUnitBase>(Unit))
			{
				W->BuildArea = nullptr;
				// If this overflow eviction pulls the worker off a resource deposit it is counted on,
				// give back its per-type worker slot and clear the assignment so CurrentWorkers stays
				// symmetric (it is leaving this->Workers just below) and it re-seeks a new place.
				if (W->ResourcePlace == this)
				{
					if (AResourceGameMode* RGM = Cast<AResourceGameMode>(GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr))
					{
						RGM->AddCurrentWorkersForResourceType(W->TeamId, ConvertToResourceType(Type), -1.0f);
						W->ResourcePlace = nullptr;
					}
				}
			}
		}
		// Remove worker from array via helper (ensures consistent behavior)
		RemoveWorkerFromArray(Worker);
	}
}

float AWorkArea::GetCollisionRadiusInDirection(const FVector& Direction) const
{
	if (TriggerCapsule)
	{
		return TriggerCapsule->GetScaledCapsuleRadius();
	}

	FVector MyOrigin, BoxExtent;
	GetActorBounds(false, MyOrigin, BoxExtent);
	return FMath::Max(BoxExtent.X, BoxExtent.Y);
}
