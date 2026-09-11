// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Characters/Unit/UnitBase.h"
#include "GameModes/ResourceGameMode.h"
#include "Actors/EffectArea.h"
#include "Actors/AreaDecalComponent.h"
#include "Characters/Unit/BuildingBase.h"
#include "Characters/Unit/ConstructionUnit.h"
#include "Actors/Waypoint.h"
#include "GAS/AttributeSetBase.h"
#include "AbilitySystemComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "AIController.h"
#include "NavCollision.h"
#include "Widgets/UnitBaseHealthBar.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Actors/Projectile.h"
#include "Hud/HUDBase.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SkeletalMeshComponent.h"
#include "Controller/PlayerController/ControllerBase.h"
#include "Controller/PlayerController/CustomControllerBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameModes/RTSGameModeBase.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "Navigation/CrowdAgentInterface.h"
#include "Navigation/CrowdManager.h"
#include "NavModifierComponent.h"
#include "NavAreas/NavArea_Obstacle.h"
#include "NavMesh/RecastNavMesh.h"
#include "Components/BoxComponent.h"
#include "Core/CollisionUtils.h"
#include "Net/UnrealNetwork.h"
#include "Widgets/UnitTimerWidget.h"
#include "Mass/Replication/UnitRegistryReplicator.h"
#include "Widgets/SquadHealthBar.h"
#include "EngineUtils.h"
#include "System/PlayerTeamSubsystem.h"
#include <climits>
// Mass includes for follow application and spawn return adjustments
#include "Mass/Projectile/ProjectileVisualManager.h"
#include "MassCommonFragments.h"
#include "MassEntitySubsystem.h"
#include "MassEntityManager.h"
#include "Mass/UnitMassTag.h"
#include "Mass/MassActorBindingComponent.h"
#include "MassSignalSubsystem.h"
#include "Mass/Signals/MySignals.h"
#include "Characters/Unit/MassUnitBase.h"
#include "Characters/Unit/GASUnit.h"
#include "Mass/MassUnitVisualFragments.h"
#include "Mass/Replication/ReplicationSettings.h"
#include "MassReplicationFragments.h"

// ------------------------------------------------------------------------------------------------
// Schalter fuer die Spawn-Absicherung. 1 = an (Vorgabe), 0 = aus wie vor dem 29.08.2026.
//
// Nur zum Nachmessen: ohne Vergleichswert sagt "keine Einheit ausserhalb des Netzes" nichts aus,
// weil es vorher vielleicht ebenfalls null war. Mit dem Schalter laesst sich derselbe Build
// beidseitig messen, ohne neu zu bauen. Uebergabe an eine Partie: -dpcvars=rts.spawn.navsicherung=0
// ------------------------------------------------------------------------------------------------
static TAutoConsoleVariable<int32> CVarRTSSpawnNavSicherung(
	TEXT("rts.spawn.navsicherung"),
	1,
	TEXT("1 = Spawnstellen auf das Navigationsnetz ziehen (Vorgabe), 0 = alter Stand ohne Absicherung."),
	ECVF_Default);

const FName AUnitBase::BoxCollisionTag = TEXT("BoxCollision");

AControllerBase* ControllerBase;
// Sets default values
AUnitBase::AUnitBase(const FObjectInitializer& ObjectInitializer):Super(ObjectInitializer)
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = TickInterval; 
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// We replicate now via Mass
	//GetCharacterMovement()->SetIsReplicated(false);
	/*
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 600.0f, 0.0f);
	GetCharacterMovement()->SetIsReplicated(true);
	*/
	
	Niagara_A = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Niagara"));
	Niagara_A->SetupAttachment(RootComponent);
	Niagara_A->SetIsReplicated(false);
	
	Niagara_B = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Niagara_B"));
	Niagara_B->SetupAttachment(RootComponent);
	Niagara_B->SetIsReplicated(false);

	//SetReplicates(false);
	//bReplicates = false;
	
	//SetReplicates(true);
	//bReplicates = true;

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponentBase>("AbilitySystemComp");
	
	if(ensure(AbilitySystemComponent != nullptr))
	{
		AbilitySystemComponent->SetIsReplicated(true);
		AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
		// We switched to Minimal
		//AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Full);
	}
	
	Attributes = CreateDefaultSubobject<UAttributeSetBase>("Attributes");

	// Example: Inside AUnitBase::AUnitBase() constructor
	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	if (MoveComp)
	{
		MoveComp->bUseRVOAvoidance = true;
		MoveComp->AvoidanceWeight = 1.0f;         // Higher weight = higher priority
		MoveComp->SetAvoidanceEnabled(true);  // Adjust based on unit size
		MoveComp->AvoidanceConsiderationRadius = 100.0f; // How far to check for obstacles
	}

	SetCanAffectNavigationGeneration(true, true); // Enable dynamic NavMesh updates
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);


	// Force navigation update
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
	if (NavSys)
	{
		NavSys->UpdateActorInNavOctree(*this); // Update NavMesh representation
	}
	

	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicates(true);
	SetNetUpdateFrequency(2);
	SetMinNetUpdateFrequency(1);
	GetCharacterMovement()->SetIsReplicated(false);
	GetCapsuleComponent()->SetIsReplicated(false);

	GetMesh()->SetIsReplicated(false);
	ISMComponent->SetIsReplicated(false);
	NavObstaclePadding = 5.0f;
	bIsBuilding = false;
}

// Called when the game starts or when spawned
void AUnitBase::ApplyStartupSupplyCost()
{
	if (bStartupSupplyCharged || !HasAuthority())
	{
		return;
	}

	// Level-placed units only. A trained unit already pays supply through its production ability, so
	// charging it here as well would bill it twice.
	if (!IsNetStartupActor())
	{
		return;
	}

	// Buildings are left out on purpose: several of them GRANT capacity, and they pay their own
	// ConstructionCost. This is about the army a faction starts the match with.
	if (bIsBuilding || bIsConstructionUnit)
	{
		return;
	}

	const int32 Amount = (StartupSupplyCost > 0) ? StartupSupplyCost : UnitSpaceNeeded;
	if (Amount <= 0)
	{
		return;
	}

	AResourceGameMode* ResourceGameMode = Cast<AResourceGameMode>(GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr);
	if (!ResourceGameMode)
	{
		return;
	}

	static const EResourceType AllTypes[] = {
		EResourceType::Primary, EResourceType::Secondary, EResourceType::Tertiary,
		EResourceType::Rare, EResourceType::Epic, EResourceType::Legendary };

	for (const EResourceType SupplyType : AllTypes)
	{
		if (!ResourceGameMode->IsSupplyLikeResource(SupplyType))
		{
			continue;
		}
		// Negative means "pay": ModifyResource inverts the sign for supply-like resources, so this
		// raises the used amount exactly as training the unit would have.
		ResourceGameMode->ModifyResource(SupplyType, TeamId, -(float)Amount);
	}

	// Remember the exact figure so ReleaseUnitSupply hands back what was taken, not an estimate.
	ChargedSupplyAmount = Amount;
	bSupplyAmountKnown = true;
	bStartupSupplyCharged = true;
}

void AUnitBase::ReleaseUnitSupply()
{
	if (bSupplyReleased || !HasAuthority())
	{
		return;
	}
	bSupplyReleased = true;

	// Buildings hand back their GRANTED capacity through ReleaseSupplyCapacity() instead.
	//
	// Construction units belong in the same exclusion, and leaving them out was the sign error
	// behind a NEGATIVE used supply ("-55/70"): ApplyStartupSupplyCost skips them explicitly, so
	// they never pay - but every one of them refunded UnitSpaceNeeded on death. One building site
	// per building, dozens per match, each one subtracting from a counter it had never added to.
	if (bIsBuilding || bIsConstructionUnit)
	{
		return;
	}

	// Hand back exactly what was billed - and nothing at all when nothing was recorded.
	//
	// bSupplyAmountKnown is set by the two paths that actually charge: ApplyStartupSupplyCost for
	// level-placed units, and the spawn path for units trained by an ability. Everything else -
	// units placed by the GameMode spawn table, for instance - never paid, so it must not refund.
	// The old fallback to UnitSpaceNeeded is exactly what drove the used amount below zero and
	// produced the "-55/70" in the UI: a refund is only correct against a charge.
	if (!bSupplyAmountKnown)
	{
		// Named so an unpaid-but-refunding path can be identified instead of guessed at. Rate is
		// self-limiting: this runs once per unit, at death.
		if (UnitSpaceNeeded > 0)
		{
			UE_LOG(LogTemp, Verbose,
				TEXT("[Versorgung] %s (Team %d) hat nie Versorgung bezahlt - keine Rueckgabe (UnitSpaceNeeded=%d)."),
				*GetName(), TeamId, UnitSpaceNeeded);
		}
		return;
	}

	const int32 Amount = ChargedSupplyAmount;
	if (Amount <= 0)
	{
		return;
	}

	AResourceGameMode* ResourceGameMode = Cast<AResourceGameMode>(GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr);
	if (!ResourceGameMode)
	{
		return;
	}

	static const EResourceType AllSupplyTypes[] = {
		EResourceType::Primary, EResourceType::Secondary, EResourceType::Tertiary,
		EResourceType::Rare, EResourceType::Epic, EResourceType::Legendary };

	for (const EResourceType SupplyType : AllSupplyTypes)
	{
		if (!ResourceGameMode->IsSupplyLikeResource(SupplyType))
		{
			continue;
		}
		// Positive here: ModifyResource inverts the sign for supply-like resources, so this LOWERS
		// the used amount - the opposite of the charge in ApplyStartupSupplyCost.
		//
		// Named refund. The floor in ModifyResource still catches a refund against an empty
		// counter, but a floor that fires silently only hides the imbalance; this line says WHICH
		// unit refunded more than its side had ever paid.
		const float UsedBefore = ResourceGameMode->GetResource(TeamId, SupplyType);
		if (UsedBefore < (float)Amount)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[Versorgung] %s (%s, Team %d) gibt %d zurueck, verbraucht sind aber nur %.0f."),
				*GetName(), *GetClass()->GetName(), TeamId, Amount, UsedBefore);
		}
		ResourceGameMode->ModifyResource(SupplyType, TeamId, (float)Amount);
	}
}

void AUnitBase::Destroyed()
{
	// Messung: STERBEN/Verschwinden einer Einheit. Gegenstueck zu [EinheitAuf].
	// Destroyed() feuert genau einmal je Actor - der frueher benutzte Signalweg
	// (UnitStateProcessor::HandleStartDead) feuerte fuer EIN totes Gebaeude ueber
	// tausendmal und zaehlte damit Signalaufrufe statt Tode. Reine Protokollzeile.
	// Position mitschreiben: sie unterscheidet zwei Erklaerungen fuer die in R125
	// belegte doppelte Verlustrate der Xeno (31 gegen 17 bei gleicher Produktion).
	// Weit gestreute Sterbeorte -> die Einheiten kommen einzeln an.
	// Gebuendelte Sterbeorte auf dem Weg -> Nahkampf stirbt vor dem Fernkampf.
	// Xeno-Basis liegt bei (6023, -6839); die Entfernung dorthin wird beim Auswerten
	// gerechnet, nicht hier - das Log soll roh bleiben.
	// AKTORNAME zusaetzlich: nur damit laesst sich EIN Arbeiter verfolgen. In R137
	// blieben 96 "Arbeitertode" je Lauf unerklaert - Kampf ist ausgeschlossen (nur
	// 0-3 Gegner in der Basis) und Bauverbrauch auch (der Arbeiter ueberlebt den
	// Bauabschluss, UnitStateProcessor:565-579 schickt ihm SetUnitStatePlaceholder).
	// Bleibt der Verdacht, dass Destroyed() hier gar keinen Tod meldet, sondern
	// einen Actor-Wechsel. Taucht derselbe Name spaeter wieder in [EinheitAuf] auf,
	// ist es kein Tod. Diagnosezeile - vor Auslieferung raus.
	// ZUSTAND beim Verschwinden - das ist die entscheidende Spalte. Kampf,
	// Bauverbrauch und Messfehler sind als Ursache ausgeschlossen (R137), also
	// muss die Zeile selbst sagen, WAS die Einheit gerade tat. Korrelationstests
	// haben hier nicht getragen: bei 0,41 Spawns/s liegt die Zufallserwartung
	// fuer ein 3-s-Fenster schon bei ~70 %, ein Trefferanteil ist damit wertlos.
	const FVector Ort = GetActorLocation();
	UE_LOG(LogTemp, Warning, TEXT("[EinheitAb] %s %d %d %s Zustand %d Health %.0f"),
		*GetClass()->GetName(), FMath::RoundToInt(Ort.X), FMath::RoundToInt(Ort.Y),
		*GetName(), (int32)UnitState, Attributes ? Attributes->GetHealth() : -1.f);

	Super::Destroyed();
}

void AUnitBase::BeginPlay()
{
	Super::BeginPlay();

	// Messung: ENTSTEHEN einer Einheit. Gegenstueck ist [EinheitAb] im Todespfad
	// (UnitStateProcessor::HandleStartDead). Der Klassenname traegt Fraktion UND Typ,
	// deshalb reicht er - TeamId ist zu diesem Zeitpunkt noch nicht zuverlaessig gesetzt.
	// Zweck: die Frage "sterben die Einheiten oder entstehen sie gar nicht erst?"
	// laesst sich NUR ueber Ereignisse beantworten, nie ueber Stichproben der
	// Bestandszahl - Verlust und Neubau heben sich im Abtastintervall sonst auf.
	// Aktorname wie in [EinheitAb] - erst das Paar erlaubt, EINEN Actor ueber
	// seine Lebensdauer zu verfolgen (siehe Begruendung dort).
	UE_LOG(LogTemp, Warning, TEXT("[EinheitAuf] %s %s"),
		*GetClass()->GetName(), *GetName());

	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())

	{
		if (AHUDBase* HUD = Cast<AHUDBase>(PC->GetHUD()))
		{
			HUD->RegisterUnit(this);
		}
	}
	
	BoxCollisionComponent = FCollisionUtils::FindTaggedBoxComponent(this);
	if (BoxCollisionComponent)
	{
	}
	
	ControllerBase = Cast<AControllerBase>(GetWorld()->GetFirstPlayerController());
	SetupTimerWidget();
	
	SetReplicateMovement(false);

	if (HasAuthority())
	{
		// Deferred: TeamId is not reliable at BeginPlay, same reason the supply-granting buildings wait.
		FTimerHandle StartupSupplyHandle;
		GetWorldTimerManager().SetTimer(StartupSupplyHandle, this, &AUnitBase::ApplyStartupSupplyCost, 0.5f, false);
	}

	if (HasAuthority())
	{
		SetMeshRotationServer();
		if (UPlayerTeamSubsystem* TeamSubsystem = GetGameInstance()->GetSubsystem<UPlayerTeamSubsystem>())
		{
			AlliedTeamsMask = TeamSubsystem->GetAlliedTeamsMask(TeamId);
		}
	}
	
	
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UAttributeSetBase::GetHealthAttribute()).AddUObject(this, &AUnitBase::OnAttributeChanged);
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UAttributeSetBase::GetShieldAttribute()).AddUObject(this, &AUnitBase::OnAttributeChanged);
	}
	

	InitHealthbarOwner();
}



void AUnitBase::InitHealthbarOwner()
{
	UUnitBaseHealthBar* HealthBarWidget = Cast<UUnitBaseHealthBar>(HealthWidgetComp->GetUserWidgetObject());
		
	if (HealthBarWidget)
	{
		HealthBarWidget->SetOwnerActor(this);
		HealthWidgetRelativeOffset = HealthWidgetComp->GetRelativeLocation();
	}
	// Ensure proper widget for squads
	EnsureSquadHealthbarState();
}

void AUnitBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (AHUDBase* HUD = Cast<AHUDBase>(PC->GetHUD()))
		{
			HUD->UnregisterUnit(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void AUnitBase::EnsureSquadHealthbarState()
{
	if (!HealthWidgetComp) return;
	if (SquadId <= 0) return; // Regular units keep their own healthbar

	// Select designated owner: alive squadmate with smallest UnitIndex; fallback to first found
	AUnitBase* Best = nullptr;
	int32 BestIndex = TNumericLimits<int32>::Max();
	UWorld* World = GetWorld();
	if (!World) return;

	for (TActorIterator<AUnitBase> It(World); It; ++It)
	{
		AUnitBase* U = *It;
		if (!U || U->TeamId != TeamId || U->SquadId != SquadId) continue;
		if (U->GetUnitState() == UnitData::Dead) continue;
		int32 Index = U->UnitIndex;
		if (Index < 0) Index = INT_MAX - 1; // push invalids back
		if (!Best || Index < BestIndex)
		{
			Best = U;
			BestIndex = Index;
		}
	}

	if (!Best)
	{
		// No alive squadmates: hide this component just in case
		OpenHealthWidget = false;
		if (UUserWidget* UW = HealthWidgetComp->GetUserWidgetObject())
		{
			UW->SetVisibility(ESlateVisibility::Collapsed);
		}
		HealthWidgetComp->SetVisibility(false);
		return;
	}

	if (Best == this)
	{
		// This unit is the designated owner: ensure Squad widget is used
		if (!HealthWidgetComp->GetUserWidgetObject() || !HealthWidgetComp->GetUserWidgetObject()->IsA(USquadHealthBar::StaticClass()))
		{
			HealthWidgetComp->SetWidgetClass(USquadHealthBar::StaticClass());
			// Force-create the widget instance so we can set it up immediately
			HealthWidgetComp->InitWidget();
		}
		else if (!HealthWidgetComp->GetUserWidgetObject())
		{
			HealthWidgetComp->InitWidget();
		}

		if (UUnitBaseHealthBar* HB = Cast<UUnitBaseHealthBar>(HealthWidgetComp->GetUserWidgetObject()))
		{
			HB->SetOwnerActor(this);
			// If it is a SquadHealthBar and the designer wants it always visible, show it now
			if (USquadHealthBar* SquadHB = Cast<USquadHealthBar>(HB))
			{
				if (SquadHB->bAlwaysShowSquadHealthbar)
				{
					HB->SetVisibility(ESlateVisibility::Visible);
					HB->UpdateWidget();
				}
			}
		}
		// Ensure the component itself is visible so the widget can render
		HealthWidgetComp->SetVisibility(true);
	}
	else
	{
		// Not owner: collapse and prevent updates
		OpenHealthWidget = false;
		if (UUserWidget* UW = HealthWidgetComp->GetUserWidgetObject())
		{
			UW->SetVisibility(ESlateVisibility::Collapsed);
		}
		HealthWidgetComp->SetVisibility(false);
	}
}

bool AUnitBase::IsSquadHealthbarOwner() const
{
	if (SquadId <= 0) return false;
	UWorld* World = GetWorld();
	if (!World) return false;

	const int32 MyTeam = TeamId;
	const int32 MySquad = SquadId;

	AUnitBase* Best = nullptr;
	int32 BestIndex = TNumericLimits<int32>::Max();
	for (TActorIterator<AUnitBase> It(World); It; ++It)
	{
		AUnitBase* U = *It;
		if (!U || U->TeamId != MyTeam || U->SquadId != MySquad) continue;
		if (U->GetUnitState() == UnitData::Dead) continue;
		int32 Index = U->UnitIndex;
		if (Index < 0) Index = INT_MAX - 1; // push invalids back
		if (!Best || Index < BestIndex)
		{
			Best = U;
			BestIndex = Index;
		}
	}
	return Best == this;
}


void AUnitBase::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(AUnitBase, bSuppressDeathEffects);
	DOREPLIFETIME(AUnitBase, ToggleUnitDetection);
	DOREPLIFETIME(AUnitBase, RunLocation);
	DOREPLIFETIME(AUnitBase, MeshAssetPath);
	DOREPLIFETIME(AUnitBase, MeshMaterialPath);
	// DOREPLIFETIME(AUnitBase, UnitControlTimer); // Fragment Timer is source of truth
	DOREPLIFETIME(AUnitBase, CanActivateAbilities);
	DOREPLIFETIME(AUnitBase, NavObstaclePadding);

	DOREPLIFETIME(AUnitBase, CastTime); // Added for Build
	DOREPLIFETIME(AUnitBase, PauseDuration);
	DOREPLIFETIME(AUnitBase, AttackDuration);
	DOREPLIFETIME(AUnitBase, PlayRateRunTimeCalculation);
	DOREPLIFETIME(AUnitBase, SpawnProjectileAtPercentage);
	DOREPLIFETIME(AUnitBase, UseProjectile);
	DOREPLIFETIME(AUnitBase, ReduceCastTime); // Added for Build
	DOREPLIFETIME(AUnitBase, ReduceRootedTime); // Added for Build
	DOREPLIFETIME(AUnitBase, UnitToChase);
	DOREPLIFETIME(AUnitBase, FollowUnit);
	DOREPLIFETIME(AUnitBase, AlliedTeamsMask);
	DOREPLIFETIME(AUnitBase, NextWaypoint);
	
	DOREPLIFETIME(AUnitBase, DelayDeadVFX);
	DOREPLIFETIME(AUnitBase, DelayDeadSound);
	
	DOREPLIFETIME(AUnitBase, CanMove);
	DOREPLIFETIME(AUnitBase, CanAnimate);
	DOREPLIFETIME(AUnitBase, CanOnlyAttackGround);
	DOREPLIFETIME(AUnitBase, CanOnlyAttackFlying);
	DOREPLIFETIME(AUnitBase, CanDetectInvisible);
	DOREPLIFETIME(AUnitBase, CanAttack);
	DOREPLIFETIME(AUnitBase, bIsInvisible);
	DOREPLIFETIME(AUnitBase, bCanBeInvisible);
	DOREPLIFETIME(AUnitBase, bIsInvulnerable);
	DOREPLIFETIME(AUnitBase, bHoldPosition);
	DOREPLIFETIME(AUnitBase, MovementAcceptanceRadius);
}


// Called every frame
void AUnitBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// Called to bind functionality to input
void AUnitBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

void AUnitBase::SetDeathVisualState(bool bShouldHide)
{
	// Der Blob-Schatten der MaterialDrivenShadows haengt NICHT an SetHiddenInGame: die Komponente
	// ist ein reines SceneComponent, gezeichnet wird der Schatten vom Subsystem des Plugins. Die
	// Schleife weiter unten setzt also nur ein Flag, und der Schattenfleck blieb nach dem
	// Ausblenden auf dem Boden liegen. Hier ist die richtige Stelle: HandleHideUnit ruft diese
	// Funktion auf Client UND Server (siehe die NetMode-Zeile dort), der Schatten verschwindet
	// damit auf beiden Seiten.
	SetzeBlobSchattenAktiv(!bShouldHide);

	if (bShouldHide)
	{
		// 1. Hide the Skeletal Mesh
		if (GetMesh()) GetMesh()->SetHiddenInGame(true);

		// 2. Hide the Health Bar Widget
		if (HealthWidgetComp) HealthWidgetComp->SetVisibility(false);

		// 3. Hide all other visual components EXCEPT the AreaDecalComponent
		TArray<USceneComponent*> Components;
		GetComponents<USceneComponent>(Components);
		for (USceneComponent* Comp : Components)
		{
			// We keep the RootComponent and any AreaDecalComponent visible
			if (Comp && Comp != GetRootComponent() && !Comp->IsA<UAreaDecalComponent>())
			{
				// Prevent flickering of texture painting (RVT)
				if (Comp->GetName().Contains(TEXT("RVTWriterMesh")))
				{
					continue;
				}
				Comp->SetHiddenInGame(true);
			}
		}
	}
	else
	{
		// Restore if needed (not typically used in this flow)
		if (GetMesh()) GetMesh()->SetHiddenInGame(false);
		if (HealthWidgetComp) HealthWidgetComp->SetVisibility(true);
		
		TArray<USceneComponent*> Components;
		GetComponents<USceneComponent>(Components);
		for (USceneComponent* Comp : Components)
		{
			if (Comp)
			{
				Comp->SetHiddenInGame(false);
			}
		}
	}
}

void AUnitBase::OnRep_MeshAssetPath()
{
	// Check if the asset path is valid
	if (!MeshAssetPath.IsEmpty())
	{
		// Attempt to load the mesh from the given asset path
		USkeletalMesh* NewMesh = LoadObject<USkeletalMesh>(nullptr, *MeshAssetPath);

		// Check if the mesh is valid
		if (NewMesh)
		{
			// Apply the mesh to the component
			GetMesh()->SetSkeletalMesh(NewMesh);
		}
	}
}

void AUnitBase::OnRep_MeshMaterialPath()
{
	// Check if the material asset path is valid
	if (!MeshMaterialPath.IsEmpty())
	{
		// Attempt to load the material from the given asset path
		UMaterialInstance* NewMaterial = LoadObject<UMaterialInstance>(nullptr, *MeshMaterialPath);

		// Check if the material is valid
		if (NewMaterial)
		{
			// Apply the material to the first material slot
			if (bUseSkeletalMovement)
			{
				GetMesh()->SetMaterial(0, NewMaterial);
			}
			else
			{
				ISMComponent->SetMaterial(0, NewMaterial);
			}
		}
	}
}

void AUnitBase::SetMeshRotationServer()
{
	if (GetMesh())
	{
		if (HasAuthority())
		{
			GetMesh()->SetRelativeRotation(ServerMeshRotation);
		}
	}
}

void AUnitBase::ServerStartAttackEvent_Implementation()
{
	MultiCastStartAttackEvent();
}

bool AUnitBase::ServerStartAttackEvent_Validate()
{
	return true;
}

void AUnitBase::MultiCastStartAttackEvent_Implementation()
{
	StartAttackEvent();
}


void AUnitBase::ServerMeeleImpactEvent_Implementation()
{
	MultiCastMeeleImpactEvent();
}

bool AUnitBase::ServerMeeleImpactEvent_Validate()
{
	return true;
}

void AUnitBase::MultiCastMeeleImpactEvent_Implementation()
{
	MeeleImpactEvent();
}


void AUnitBase::IsAttacked(AActor* AttackingCharacter) 
{
	SetUnitState(UnitData::IsAttacked);
	SetWalkSpeed(Attributes->GetIsAttackedSpeed());

}
void AUnitBase::SetRunLocation_Implementation(FVector Location)
{
	RunLocation = Location;
}

// ---- Flying-building helpers -------------------------------------------------------------------
void AUnitBase::StartBuildingFlight(float InFlyHeight)
{
	if (!HasAuthority()) return;

	FlyHeight = InFlyHeight;   // replicated (AMassUnitBase) -> synced to the Mass fragment on all machines
	IsFlying = true;           // replicated -> HandleGroundAndHeight interps Z up to LastGroundLocation+FlyHeight

	// ZUERST Vermeidung und Hindernis abschalten, DANN erst CanMove. Ein Gebaeude ist um ein
	// Vielfaches groesser als eine Einheit; sobald es beweglich wird, draengen Separation und
	// Avoidance es mit voller Ueberlappung auseinander und katapultieren es seitlich weg. Der
	// Start soll nur in Z laufen.
	SetUnitAvoidanceEnabled(false);
	EnableDynamicObstacle(false);

	CanMove = true;            // unlocks the movers + the Z-interp (StopMovement freeze excludes it otherwise)

	// Drop the navmesh obstacle so ground units can path under the lifted-off building.
	Multicast_UnregisterObstacle();

	// Fire SyncUnitBase: its handler removes FMassStateStopMovementTag now that CanMove is true.
	FMassEntityManager* EntityManager = nullptr;
	FMassEntityHandle EntityHandle;
	if (GetMassEntityData(EntityManager, EntityHandle))
	{
		if (UMassSignalSubsystem* SignalSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UMassSignalSubsystem>() : nullptr)
		{
			SignalSubsystem->SignalEntity(UnitSignals::SyncUnitBase, EntityHandle);
		}
	}
}

void AUnitBase::MoveUnitToLocation(FVector WorldLocation, float MoveSpeed, float AcceptanceRadius)
{
	if (!HasAuthority()) return;

	FMassEntityManager* EntityManager = nullptr;
	FMassEntityHandle EntityHandle;
	if (!GetMassEntityData(EntityManager, EntityHandle) || !EntityManager) return;

	FMassMoveTargetFragment* MoveTarget = EntityManager->GetFragmentDataPtr<FMassMoveTargetFragment>(EntityHandle);
	if (!MoveTarget) return;

	UpdateMoveTarget(*MoveTarget, WorldLocation, MoveSpeed, GetWorld());
	MoveTarget->SlackRadius = AcceptanceRadius;

	// Put the unit into Run (sets actor state + Run tag, strips the other state tags). Arrival is the
	// 2D Dist check in the Run state processor -> it switches to Idle when X/Y reaches the target.
	SwitchEntityTagByState(UnitData::Run, UnitStatePlaceholder);
}

void AUnitBase::BeginLanding()
{
	if (!HasAuthority()) return;
	// Keep CanMove=true so HandleGroundAndHeight keeps running and smoothly interps Z down to the ground.
	IsFlying = false;
}

void AUnitBase::FinishLanding(bool bReRegisterObstacle)
{
	if (!HasAuthority()) return;
	IsFlying = false;
	CanMove = false;               // re-freeze the building in place
	AddStopMovementTagToEntity();  // stop the mover
	if (bReRegisterObstacle)
	{
		Multicast_RegisterBuildingAsObstacle(); // re-carve the navmesh hole at the new position
	}
}

bool AUnitBase::IsUnitAtLocation2D(FVector WorldLocation, float AcceptanceRadius) const
{
	return FVector::Dist2D(GetActorLocation(), WorldLocation) <= AcceptanceRadius;
}

void AUnitBase::SnapUnitToLocation2D(FVector WorldLocation)
{
	if (!HasAuthority()) return;

	SnapAusfuehren(WorldLocation);

	// Und dasselbe auf jeder Client-Maschine. Der Client simuliert seine Mass-Entitaet selbst
	// weiter; steht deren Bewegungsziel noch auf dem alten Punkt, zieht sie das Gebaeude jeden
	// Tick weg und die Replikation holt es zurueck - sichtbar als Zittern waehrend der Landung.
	Multicast_SnapUnitToLocation2D(WorldLocation);
}

void AUnitBase::Multicast_SnapUnitToLocation2D_Implementation(FVector WorldLocation)
{
	// Auf dem Server ist die Arbeit schon getan.
	if (HasAuthority()) return;

	SnapAusfuehren(WorldLocation);
}

void AUnitBase::SnapAusfuehren(FVector WorldLocation)
{
	FMassEntityManager* EntityManager = nullptr;
	FMassEntityHandle EntityHandle;
	if (!GetMassEntityData(EntityManager, EntityHandle) || !EntityManager) return;

	FTransformFragment* TransformFrag = EntityManager->GetFragmentDataPtr<FTransformFragment>(EntityHandle);
	if (!TransformFrag) return;

	// Nur X/Y. Das Absenken macht HandleGroundAndHeight ueber die Flughoehe - wer hier auch Z
	// setzt, laesst das Gebaeude im Boden stecken.
	FTransform NeuerTransform = TransformFrag->GetTransform();
	FVector NeuePosition = NeuerTransform.GetLocation();
	NeuePosition.X = WorldLocation.X;
	NeuePosition.Y = WorldLocation.Y;
	NeuerTransform.SetLocation(NeuePosition);
	TransformFrag->SetTransform(NeuerTransform);

	SetActorLocation(NeuePosition, false, nullptr, ETeleportType::TeleportPhysics);

	// Das Bewegungsziel MUSS mitgezogen werden. Bleibt es auf dem alten Punkt stehen, zieht der
	// naechste Mover-Tick das Gebaeude sofort wieder vom Indikator weg.
	//
	// NUR auf der Autoritaet: UpdateMoveTarget und das darin gerufene SetDesiredAction haben je ein
	// ensure gegen NM_Client. Auf dem Client feuerten beide bei jeder Landung, und jedes ensure
	// macht einen Stack-Walk samt Fehlerbericht - gemessen 1,1 s und 2,7 s. Genau das war der
	// Ruckler beim Landen. Der Client braucht das Ziel auch nicht: seine Position kommt ohnehin
	// aus der Replikation, ihm reichen Transform und Aktorposition oben.
	if (HasAuthority())
	{
		if (FMassMoveTargetFragment* MoveTarget = EntityManager->GetFragmentDataPtr<FMassMoveTargetFragment>(EntityHandle))
		{
			UpdateMoveTarget(*MoveTarget, NeuePosition, 0.f, GetWorld());
			MoveTarget->SlackRadius = 1.f;
		}

		AddStopMovementTagToEntity();
	}
}

void AUnitBase::FlyUnitToLocationAndLand(FVector WorldLocation, float InFlyHeight, float MoveSpeed, float AcceptanceRadius, float DescendTime)
{
	if (!HasAuthority()) return;

	FlyLandTarget = WorldLocation;
	FlyLandAcceptance = AcceptanceRadius;
	FlyLandDescendTime = FMath::Max(0.1f, DescendTime);

	StartBuildingFlight(InFlyHeight);
	MoveUnitToLocation(WorldLocation, MoveSpeed, AcceptanceRadius);

	// Poll for arrival, then start the descend timer.
	GetWorldTimerManager().SetTimer(FlyLandArrivalTimer, this, &AUnitBase::PollFlyArrival, 0.2f, true);
}

void AUnitBase::PollFlyArrival()
{
	if (!HasAuthority() || !IsFlying)
	{
		GetWorldTimerManager().ClearTimer(FlyLandArrivalTimer);
		return;
	}

	if (IsUnitAtLocation2D(FlyLandTarget, FlyLandAcceptance) || GetUnitState() == UnitData::Idle)
	{
		GetWorldTimerManager().ClearTimer(FlyLandArrivalTimer);
		BeginLanding(); // start descending
		GetWorldTimerManager().SetTimer(FlyLandDescendTimer, this, &AUnitBase::FinishLandingDefault, FlyLandDescendTime, false);
	}
}

void AUnitBase::FinishLandingDefault()
{
	FinishLanding(true);
}

void AUnitBase::SetWalkSpeed_Implementation(float Speed)
{
	UCharacterMovementComponent* MovementPtr = GetCharacterMovement();
	if(Speed == 0.f)
	{
		MovementPtr->StopMovementImmediately();
	}else
	{
		MovementPtr->MaxWalkSpeed = Speed;
	}
}


void AUnitBase::SetWaypoint(AWaypoint* NewNextWaypoint)
{
	if (NextWaypoint == NewNextWaypoint) return;

	if (ABuildingBase* Building = Cast<ABuildingBase>(this))
	{
		if (NextWaypoint && IsValid(NextWaypoint)) NextWaypoint->RemoveAssignedUnit(this);
		NextWaypoint = NewNextWaypoint;
		if (NextWaypoint && IsValid(NextWaypoint)) NextWaypoint->AddAssignedUnit(this);
	}
	else
	{
		NextWaypoint = NewNextWaypoint;
	}
}

AWaypoint* AUnitBase::GetNextWaypoint() const
{
	return IsValid(NextWaypoint) ? NextWaypoint : nullptr;
}

void AUnitBase::SetHealth_Implementation(float NewHealth)
{
	float OldHealth = Attributes->GetHealth();

	// Unverwundbarkeit wird hier NICHT mehr geprueft. Der frueher an dieser Stelle stehende
	// Waechter war wirkungslos: der gesamte Kampfschaden laeuft ueber
	// UAttributeSetBase::PostGameplayEffectExecute und schreibt den Attributwert direkt, ohne
	// SetHealth je aufzurufen. Der Waechter sitzt jetzt dort - siehe AUnitBase::bIsInvulnerable.

	// Fire Blueprint event when crossing 25% or 50% thresholds (up or down)
	{
		const float LocalMaxHealth = Attributes->GetMaxHealth();
		if (LocalMaxHealth > 0.f && OldHealth != NewHealth)
		{
			const float OldPct = OldHealth / LocalMaxHealth;
			const float NewPct = NewHealth / LocalMaxHealth;

			auto Fire = [this, NewHealth](bool bIncrease, bool bLow, bool bHigh)
			{
				OnHealthThresholdCrossed(bIncrease, bLow, bHigh, NewHealth);
			};

			// Downward crossings
			if (OldPct >= 0.50f && NewPct < 0.50f) { Fire(false, false, true); }
			if (OldPct >= 0.25f && NewPct < 0.25f) { Fire(false, true,  false); }

			// Upward crossings
			if (OldPct <= 0.25f && NewPct > 0.25f) { Fire(true,  true,  false); }
			if (OldPct <= 0.50f && NewPct > 0.50f) { Fire(true,  false, true); }
		}
	}
	
	Attributes->SetAttributeHealth(NewHealth);
	UpdateEntityHealth(NewHealth, Attributes->GetShield());
	if(NewHealth <= 0.f)
	{
		SetWalkSpeed(0);
		GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SetActorEnableCollision(false);
		SetDeselected();
		CanBeSelected = false;
		SetUnitState(UnitData::Dead);

		// A dead unit no longer occupies supply. Without this the used amount only ever grew, so the
		// side that takes casualties slowly locked itself out of training anything at all.
		ReleaseUnitSupply();

		if (ABuildingBase* Building = Cast<ABuildingBase>(this))
		{
			// Hand the supply capacity back here rather than in Destroyed(): a killed building is only
			// switched to Dead, the actor lives on as a ruin, so Destroyed() never runs and the team
			// kept the supply of buildings it had already lost.
			Building->ReleaseSupplyCapacity();

			if (Building->HasWaypoint && Building->NextWaypoint)
			{
				AWaypoint* WP = Building->NextWaypoint;
				Building->SetWaypoint(nullptr);
				if (HasAuthority() && WP->GetAssignedUnitCount() <= 0)
				{
					WP->Destroy(true, true);
				}
			}
		}

		// Server-authoritative: immediately remove this unit from the replicated registry to avoid stale ghosts
		if (HasAuthority())
		{
			if (UWorld* W = GetWorld())
			{
				if (AUnitRegistryReplicator* Reg = AUnitRegistryReplicator::GetOrSpawn(*W))
				{
					bool bRemoved = false;
					if (UnitIndex != INDEX_NONE)
					{
						bRemoved |= Reg->Registry.RemoveByUnitIndex(UnitIndex);
					}
					bRemoved |= Reg->Registry.RemoveByOwner(GetFName());
					if (bRemoved)
					{
						Reg->Registry.MarkArrayDirty();
						Reg->ForceNetUpdate();
					}
				}
			}
		}

		DeadEffectsAndEvents();
		UnitControlTimer = 0.f;
	}
}

void AUnitBase::DeadMultiCast_Implementation()
{
	SetUnitState(UnitData::Dead);
	SwitchEntityTagByState(UnitData::Dead, UnitData::Dead);

	// Death VFX pool: DeadVFXArray entries + the legacy single DeadVFX (kept so existing assignments
	// still fire and need no re-doing). One is picked at random. Empty pool -> null (DeadSound still
	// plays). Cosmetic only, so the pick is local per machine (A: simple variant).
	UNiagaraSystem* ChosenDeadVFX = DeadVFX;
	{
		TArray<UNiagaraSystem*> Pool;
		Pool.Reserve(DeadVFXArray.Num() + 1);
		for (UNiagaraSystem* VFX : DeadVFXArray)
		{
			if (VFX)
			{
				Pool.Add(VFX);
			}
		}
		if (DeadVFX && !Pool.Contains(DeadVFX))
		{
			Pool.Add(DeadVFX);
		}
		if (Pool.Num() > 0)
		{
			ChosenDeadVFX = Pool[FMath::RandRange(0, Pool.Num() - 1)];
		}
	}

	// A death that is only bookkeeping (the Xeno worker becoming its building) must not play the
	// explosion - the unit is hidden at that point, so the effect appears out of nowhere.
	if (bSuppressDeathEffects)
	{
		return;
	}

	FireEffects_Implementation(ChosenDeadVFX, DeadSound, ScaleDeadVFX, ScaleDeadSound, DelayDeadVFX, DelayDeadSound, -1);
}

void AUnitBase::MulticastSuppressDeathEffects_Implementation()
{
	bSuppressDeathEffects = true;
}

void AUnitBase::KillSilently()
{
	if (!HasAuthority())
	{
		return;
	}

	// Reliable multicast first so every client has the flag before the death multicast reaches it -
	// relying on the replicated property alone would race the RPC.
	bSuppressDeathEffects = true;
	MulticastSuppressDeathEffects();

	SetHealth(0.f);
}
void AUnitBase::Multicast_SwitchToIdle_Implementation()
{
	SwitchEntityTagByState(UnitData::Idle, UnitData::Idle);
}

void AUnitBase::DeadEffectsAndEvents()
{
	if (!DeadEffectsExecuted)
	{
		if (HasAuthority())
		{
			if (ARTSGameModeBase* GM = Cast<ARTSGameModeBase>(GetWorld()->GetAuthGameMode()))
			{
				GM->CheckWinLoseCondition(this);
			}
		}

		DeadMultiCast();
		//FireEffects(DeadVFX, DeadSound, ScaleDeadVFX, ScaleDeadSound, DelayDeadVFX, DelayDeadSound);
		StoppedMoving();
		IsDead();
		DeadEffectsExecuted = true;
	}
	// If we are part of a squad, ensure the healthbar ownership migrates to another alive member
	if (SquadId > 0)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			for (TActorIterator<AUnitBase> It(World); It; ++It)
			{
				AUnitBase* U = *It;
				if (!U || U == this) continue;
				if (U->TeamId == TeamId && U->SquadId == SquadId)
				{
					U->EnsureSquadHealthbarState();
				}
			}
		}
	}
}





void AUnitBase::SetShield_Implementation(float NewShield)
{
	Attributes->SetAttributeShield(NewShield);
	UpdateEntityHealth(Attributes->GetHealth(), NewShield);
}

void AUnitBase::SetMana_Implementation(float NewMana)
{
	Attributes->SetAttributeMana(NewMana);
}



void AUnitBase::OnAttributeChanged(const FOnAttributeChangeData& Data)
{

	// 1. Sync local Mass fragment immediately
	UpdateEntityHealth(Attributes->GetHealth(), Attributes->GetShield());

	// 1b. Shield-impact flash: incoming damage was absorbed by Shield (Shield went DOWN). Fires on
	// server (GE execute) AND client (OnRep_Shield -> attribute-change delegate), so the visual runs
	// on every machine that renders the unit. Optional per unit.
	if (bEnableShieldImpactEffect && ShieldImpactMaterial &&
		Data.Attribute == UAttributeSetBase::GetShieldAttribute() &&
		Data.OldValue > KINDA_SMALL_NUMBER &&
		Data.NewValue < Data.OldValue - KINDA_SMALL_NUMBER)
	{
		TriggerShieldImpact();
	}

	// 2. Immediate UI Reaction (The "Signal")
	// Only trigger popup if it's not the initial sync (OldValue > 0)
	if (Data.OldValue > 0.5f)
	{
		const float Delta = Data.NewValue - Data.OldValue;
		if (Delta < -1.0f || Delta > 10.0f) // Damage or significant heal
		{
			OpenHealthWidget = true;
			bShowLevelOnly = false;
			CheckHealthBarVisibility(); // Forces UI visibility refresh instantly
		}
	}

	if (SquadId > 0)
	{
		UpdateSquadHealthBar();
	}
	else
	{
		UpdateWidget();
	}
}

void AUnitBase::TriggerShieldImpact()
{
	if (!ShieldImpactMaterial) return;
	// Purely a rendered effect: skip on a dedicated server (nothing is drawn there).
	if (GetNetMode() == NM_DedicatedServer) return;

	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp) return;

	if (!ShieldImpactMID)
	{
		ShieldImpactMID = UMaterialInstanceDynamic::Create(ShieldImpactMaterial, this);
	}
	if (!ShieldImpactMID) return;

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	ShieldImpactMID->SetScalarParameterValue(ShieldImpactTimeParam, Now);

	// Overlay pass on the mesh; removed again after the flash so idle units pay nothing.
	MeshComp->SetOverlayMaterial(ShieldImpactMID);
	GetWorldTimerManager().SetTimer(ShieldImpactTimerHandle, this, &AUnitBase::ClearShieldImpact,
		FMath::Max(ShieldImpactDuration, 0.05f), false);
}

void AUnitBase::ClearShieldImpact()
{
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetOverlayMaterial(nullptr);
	}
}

void AUnitBase::UpdateWidget()
{

	if(!HealthWidgetComp) return;
	
	UUnitBaseHealthBar* HealthBarWidget = Cast<UUnitBaseHealthBar>(HealthWidgetComp->GetUserWidgetObject());
	
	if (HealthBarWidget)
	{
		HealthBarWidget->UpdateWidget();
	}
}

void AUnitBase::UpdateSquadHealthBar()
{
	if (SquadId <= 0) return;

	AUnitBase* Best = nullptr;
	int32 BestIndex = TNumericLimits<int32>::Max();
	UWorld* World = GetWorld();
	if (World)
	{
		for (TActorIterator<AUnitBase> It(World); It; ++It)
		{
			AUnitBase* U = *It;
			if (!U || U->TeamId != TeamId || U->SquadId != SquadId) continue;
			if (U->GetUnitState() == UnitData::Dead) continue;
			
			int32 Index = U->UnitIndex;
			if (Index < 0) Index = INT_MAX - 1;
			if (!Best || Index < BestIndex)
			{
				Best = U;
				BestIndex = Index;
			}
		}
	}

	if (Best)
	{
		Best->UpdateWidget();
	}
}

void AUnitBase::IncreaseExperience()
{
	LevelData.Experience++;
		
	UpdateWidget();
}

void AUnitBase::SetSelected()
{
	Selected();
}

void AUnitBase::SetDeselected()
{
	// Remove rotation tag on deselection
	if (MassActorBindingComponent)
	{
		FMassEntityHandle Entity = MassActorBindingComponent->GetMassEntityHandle();
		if (Entity.IsValid())
		{
			UMassEntitySubsystem* MassSubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
			if (MassSubsystem)
			{
				FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();

				// DIAGNOSE [TagDiag] (17.08.2026) - diese Entfernung geht NUR lokal (kein Server-RPC).
				// Genau daraus entstand die gemessene Asymmetrie beim Casting: Server dreht zur Maus,
				// Client zum Faehigkeitsziel. Die Zeile nennt den Moment beim Namen, falls trotz der
				// Absicherung in AHUDBase::SetUnitSelected noch ein anderer Pfad hier hereinlaeuft.
				if (GetNetMode() == NM_Client
					&& DoesEntityHaveTag(EntityManager, Entity, FMassRotateToMouseTag::StaticStruct()))
				{
					UE_LOG(LogTemp, Warning,
						TEXT("[TagDiag] SetDeselected nimmt %s den Maus-Ziel-Tag (nur lokal), Zustand=%d"),
						*GetName(), (int32)GetUnitState());
				}

				EntityManager.Defer().RemoveTag<FMassRotateToMouseTag>(Entity);
	// LUX-ANPASSUNG (16.08.2026): den Sperr-Tag zusammen mit dem Ziel-Tag entfernen.
	EntityManager.Defer().RemoveTag<FMassStopWhileAimingTag>(Entity);
				EntityManager.Defer().RemoveFragment<FMassRotateToMouseFragment>(Entity);
			}
		}
	}
	
	Deselected();
}

void AUnitBase::SetupTimerWidget()
{
	if (TimerWidgetComp) {

		//TimerWidgetComp->SetRelativeLocation(TimerWidgetCompLocation, false, 0, ETeleportType::None);
		UUnitTimerWidget* Timerbar = Cast<UUnitTimerWidget>(TimerWidgetComp->GetUserWidgetObject());

		if (Timerbar) {
			Timerbar->SetOwnerActor(this);
		}
		
		TimerWidgetRelativeOffset = TimerWidgetComp->GetRelativeLocation();
	}
}

void AUnitBase::SetTimerWidgetCastingColor(FLinearColor Color)
{
	if (TimerWidgetComp) {

		UUnitTimerWidget* Timerbar = Cast<UUnitTimerWidget>(TimerWidgetComp->GetUserWidgetObject());

		if (Timerbar) {
			Timerbar->CastingColor = Color;
		}
	}
}



FVector AUnitBase::GetProjectileSpawnLocation(const FVector& AdditionalOffset) const
{
	const bool bIsClient = GetWorld() && GetWorld()->GetNetMode() == NM_Client;
	const AMassUnitBase* MassUnit = Cast<AMassUnitBase>(this);

	if (MassUnit && MassUnit->MassActorBindingComponent)
	{
		FMassEntityHandle EntityHandle = MassUnit->MassActorBindingComponent->GetEntityHandle();
		if (EntityHandle.IsValid())
		{
			UMassEntitySubsystem* EntitySubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
			if (EntitySubsystem)
			{
				const FMassEntityManager& EntityManager = EntitySubsystem->GetEntityManager();
				const FTransformFragment* TF = EntityManager.GetFragmentDataPtr<FTransformFragment>(EntityHandle);
				const FMassAgentCharacteristicsFragment* AC = EntityManager.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(EntityHandle);
				
				if (TF)
				{
					float LastGround = AC ? AC->LastGroundLocation : (TF->GetTransform().GetLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
					
					TArray<UActorComponent*> Comps = GetComponentsByTag(USceneComponent::StaticClass(), TEXT("ProjectileSpawn"));
					FVector RelativeMuzzleLocation = ProjectileSpawnOffset;

					if (Comps.Num() > 0 && Comps[0])
					{
						if (USceneComponent* SpawnComp = Cast<USceneComponent>(Comps[0]))
						{
							bool bFoundInMass = false;
							if (UInstancedStaticMeshComponent* ParentISM = Cast<UInstancedStaticMeshComponent>(SpawnComp->GetAttachParent()))
							{
								if (const FMassUnitVisualFragment* VisualFrag = MassUnit->GetVisualFragment())
								{
									for (const FMassUnitVisualInstance& Instance : VisualFrag->VisualInstances)
									{
										if (Instance.TemplateISM == ParentISM)
										{
											RelativeMuzzleLocation = (SpawnComp->GetRelativeTransform() * Instance.CurrentRelativeTransform).GetLocation();
											bFoundInMass = true;
											break;
										}
									}
								}
							}
							
							if (!bFoundInMass)
							{
								RelativeMuzzleLocation = GetActorTransform().InverseTransformPosition(SpawnComp->GetComponentLocation());
							}
						}
					}
					
					// Z-Position: Boden + (Hälfte + OffsetZ)
					float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

					// KORREKTUR: Verwende FlyHeight statt HalfHeight, wenn die Einheit fliegt
					float VerticalOffset = (AC && AC->bIsFlying) ? AC->FlyHeight : HalfHeight;
					float FinalZ = LastGround + (VerticalOffset + RelativeMuzzleLocation.Z);
					
					// XY-Position: Predicted Transform + Rotated OffsetXY
					FVector MuzzleXY = RelativeMuzzleLocation;
					MuzzleXY.Z = 0.f;
					FVector FinalPos = TF->GetTransform().TransformPosition(MuzzleXY);
					FinalPos.Z = FinalZ;

					// KORREKTUR: Berücksichtige AttributeOffset auf dem Server
					float AttributeOffset = bIsClient ? 0.f : (Attributes ? Attributes->GetProjectileScaleActorDirectionOffset() : 0.f);
					if (AttributeOffset != 0.f)
					{
						FVector ShootingUnitForward = TF->GetTransform().GetRotation().Vector();
						FinalPos += AttributeOffset * ShootingUnitForward;
					}

					return FinalPos + GetActorRotation().RotateVector(AdditionalOffset);
				}
			}
		}
	}

	const FName ProjectileSpawnTag = TEXT("ProjectileSpawn");

	// 1. Try to find a component with the specific tag (Server or non-Mass Client)
	TArray<UActorComponent*> Comps = GetComponentsByTag(USceneComponent::StaticClass(), ProjectileSpawnTag);


	if (Comps.Num() > 0)
	{
		if (USceneComponent* SpawnComp = Cast<USceneComponent>(Comps[0]))
		{
			// Check if it's attached to an ISM
			UInstancedStaticMeshComponent* ParentISM = Cast<UInstancedStaticMeshComponent>(SpawnComp->GetAttachParent());
			if (ParentISM && MassUnit)
			{
				const FMassUnitVisualFragment* VisualFrag = MassUnit->GetVisualFragment();
				if (VisualFrag)
				{
					for (const FMassUnitVisualInstance& Instance : VisualFrag->VisualInstances)
					{
						if (Instance.TemplateISM == ParentISM)
						{
							FTransform VisualMuzzleTransform = SpawnComp->GetRelativeTransform() * Instance.CurrentRelativeTransform * MassUnit->GetMassActorTransform();
							FVector ResultLoc = VisualMuzzleTransform.GetLocation();

							return ResultLoc;
						}
					}
				}
			}
			
			// If not attached to ISM (e.g. attached to StaticMeshComponent), just use standard component location
			FVector MuzzleLocation = GetActorTransform().TransformPosition(GetActorTransform().InverseTransformPosition(SpawnComp->GetComponentLocation())) + GetActorRotation().RotateVector(AdditionalOffset);

			return MuzzleLocation;
		}
	}

	// 2. Fallback: Old Mass-based search for bHasMuzzle
	if (MassUnit) {
		const FMassUnitVisualFragment* VisualFrag = MassUnit->GetVisualFragment();
		if (VisualFrag) {
			for (const FMassUnitVisualInstance& Instance : VisualFrag->VisualInstances) {
				if (Instance.bHasMuzzle) {
					FTransform VisualMuzzleTransform = Instance.MuzzleOffset * Instance.CurrentRelativeTransform * MassUnit->GetMassActorTransform();
					return VisualMuzzleTransform.GetLocation();
				}
			}
		}
	}

	// 3. Fallback to default offset logic
	FVector ShootingUnitLocation = GetMassActorLocation();


	// KORREKTUR: Wenn die Einheit fliegt, addiere die Flughöhe zur Basis-Z-Position
	if (IsFlying)
	{
		ShootingUnitLocation.Z += FlyHeight;
	}

	const FRotator ShootingUnitRotation = MassUnit ? MassUnit->GetMassActorRotation() : GetActorRotation();
	const FVector ShootingUnitForward = ShootingUnitRotation.Vector();
	
	const FVector RotatedProjectileSpawnOffset = ShootingUnitRotation.RotateVector(ProjectileSpawnOffset);
	
	// On client, ProjectileSpawnOffset already includes the attribute offset because it was synced from AIS_ProjectileSpawnOffset.
	float AttributeOffset = bIsClient ? 0.f : Attributes->GetProjectileScaleActorDirectionOffset();
	
	return ShootingUnitLocation + AttributeOffset * ShootingUnitForward + RotatedProjectileSpawnOffset + AdditionalOffset;
}


void AUnitBase::SpawnProjectile_Implementation(AActor* Target, AActor* Attacker) // FVector TargetLocation
{
	AUnitBase* ShootingUnit = Cast<AUnitBase>(Attacker);

	if (!ProjectileBaseClass || !ShootingUnit) return;

	const AProjectile* ProjectileCDO = ProjectileBaseClass->GetDefaultObject<AProjectile>();
	
	if (ProjectileCDO->bUseMass)
	{
		SpawnProjectileWithEntities(Target, Attacker);
		return;
	}

	float TwinDistance = ProjectileCDO->TwinProjectileDistance;
	int32 HomingCount = ProjectileCDO->HomingMissleCount;

	int32 BaseCount = (HomingCount > 0) ? HomingCount : 1;

	// 2) Figure out the exact world‐space "aim" point
	FVector AimLocation = Target->GetActorLocation();
	if (AUnitBase* UnitTarget = Cast<AUnitBase>(Target))
	{
		if (!UnitTarget->bUseSkeletalMovement)
			AimLocation = UnitTarget->GetMassActorLocation(); 
	}
		
	TArray<FVector> SpawnPositions;
	FVector CenterSpawnPos = ShootingUnit->GetProjectileSpawnLocation();

	if (TwinDistance >= 10.f)
	{
		FVector DirToTarget = (AimLocation - CenterSpawnPos).GetSafeNormal2D();
		FVector RightVector = DirToTarget.IsNearlyZero() ? ShootingUnit->GetActorRightVector() : FVector::CrossProduct(FVector::UpVector, DirToTarget);
		FVector RightOffset = RightVector * TwinDistance;
		SpawnPositions.Add(CenterSpawnPos - RightOffset); // Left
		SpawnPositions.Add(CenterSpawnPos + RightOffset); // Right
	}
	else
	{
		SpawnPositions.Add(CenterSpawnPos);
	}

	for (const FVector& ActualSpawnPos : SpawnPositions)
	{
		for (int32 i = 0; i < BaseCount; ++i)
		{
			FTransform Transform;
			Transform.SetLocation(ActualSpawnPos);

			FVector Direction = (AimLocation - Transform.GetLocation()).GetSafeNormal();
			
			// For homing multi-missiles, add a slight spread to the initial direction so they don't overlap perfectly
			if (HomingCount > 0 && BaseCount > 1)
			{
				float SpreadAngle = 10.0f; // degrees
				float Angle = (360.0f / BaseCount) * i;
				FVector Right, Up;
				Direction.FindBestAxisVectors(Right, Up);
				Direction = (Direction + (Right * FMath::Cos(FMath::DegreesToRadians(Angle)) + Up * FMath::Sin(FMath::DegreesToRadians(Angle))) * 0.1f).GetSafeNormal();
			}

			FRotator InitialRotation = Direction.Rotation() + ProjectileRotationOffset;

			Transform.SetRotation(FQuat(InitialRotation));
			Transform.SetScale3D(ShootingUnit->ProjectileScale);

			const auto MyProjectile = Cast<AProjectile>
								(UGameplayStatics::BeginDeferredActorSpawnFromClass
								(this, ProjectileBaseClass, Transform,  ESpawnActorCollisionHandlingMethod::AlwaysSpawn));
				if (MyProjectile != nullptr)
				{
					if (HomingCount > 0) MyProjectile->FollowTarget = true;
					
					// Initialize in-projectile rotation offset so per-frame steering matches mesh forward
					MyProjectile->RotationOffset = ShootingUnit->ProjectileRotationOffset;
					
					MyProjectile->Init(Target, Attacker);
					MyProjectile->SetProjectileVisibility();
					UGameplayStatics::FinishSpawningActor(MyProjectile, Transform);
					MyProjectile->SetReplicates(true);
				}
		}
	}
}


void AUnitBase::SpawnProjectileFromClass_Implementation(
    AActor* Aim,
    AActor* Attacker,
    TSubclassOf<AProjectile> ProjectileClass,
    int MaxPiercedTargets,
    bool FollowTarget,
    int ProjectileCount,
    float Spread,
    bool IsBouncingNext,
    bool IsBouncingBack,
    bool DisableAutoZOffset,
    float ZOffset,
    float Scale,
    FVector SpawnOffset)
{
    if (!Aim || !Attacker || !ProjectileClass) return;

    AUnitBase* ShootingUnit = Cast<AUnitBase>(Attacker);
    if (!ShootingUnit) return;
    AUnitBase* TargetUnit   = Cast<AUnitBase>(Aim);

    const AProjectile* ProjectileCDO = ProjectileClass->GetDefaultObject<AProjectile>();

    // We proceed to the main loop to handle both Actor-based and Mass-based projectiles uniformly,
    // ensuring correct transforms and multi-shot logic.
    if (ProjectileCDO->bUseMass)
    {
    }

    float TwinDistance = ProjectileCDO->TwinProjectileDistance;
    int32 HomingCount = ProjectileCDO->HomingMissleCount;

    int32 BaseCount = (HomingCount > 0) ? HomingCount : ProjectileCount;

    FVector ShootingUnitLocation = ShootingUnit->GetMassActorLocation();

    // 1) Determine the true “center” we want to spread around
    FVector AimCenter = Aim->GetActorLocation();
    if (TargetUnit && !TargetUnit->bUseSkeletalMovement)
    {
    	AimCenter = TargetUnit->GetMassActorLocation(); 
    }

    // 2) Compute vertical half-height for auto Z-offset
    FVector TargetBoxSize = Aim->GetComponentsBoundingBox().GetSize();
    const float HalfHeight = DisableAutoZOffset ? 0.f : TargetBoxSize.Z * 0.5f;

    TArray<FVector> SpawnPositions;
    FVector CenterSpawnPos = ShootingUnit->GetProjectileSpawnLocation(SpawnOffset);

    if (TwinDistance >= 10.f)
    {
        FVector DirToTarget = (AimCenter - CenterSpawnPos).GetSafeNormal2D();
        FVector RightVector = DirToTarget.IsNearlyZero() ? ShootingUnit->GetActorRightVector() : FVector::CrossProduct(FVector::UpVector, DirToTarget);
        FVector RightOffset = RightVector * TwinDistance;
        SpawnPositions.Add(CenterSpawnPos - RightOffset); // Left
        SpawnPositions.Add(CenterSpawnPos + RightOffset); // Right
    }
    else
    {
        SpawnPositions.Add(CenterSpawnPos);
    }

    for (const FVector& ActualSpawnPos : SpawnPositions)
    {
        for (int32 Count = 0; Count < BaseCount; ++Count)
        {
            // alternate left/right spread
            int32  MultiAngle     = (Count == 0) ? 0 : ((Count & 1) ? 1 : -1);
            FVector ToCenterDir   = (AimCenter - ShootingUnitLocation).GetSafeNormal();
            FVector PerpOffsetDir = FRotator(0.f, MultiAngle * 90.f, 0.f).RotateVector(ToCenterDir);
            
            float ActualSpread = Spread;
            if (HomingCount <= 0)
            {
                if (Count == 0) ActualSpread = 0.f;
                else if (Count == 1 || Count == 2) ActualSpread = Spread;
                else if (Count == 3 || Count == 4) ActualSpread = Spread / 2.0f;
                else if (Count == 5 || Count == 6) ActualSpread = Spread + (Spread / 2.0f);
                else ActualSpread = Spread + (Count / 2) * (Spread / 2.0f);
            }
            else if (ProjectileCount <= 1)
            {
                ActualSpread = 0.f;
            }
            FVector SpreadOffset  = PerpOffsetDir * ActualSpread;

            // final aim point for this shot
            FVector LocationToShoot = AimCenter + SpreadOffset;
            LocationToShoot.Z     += HalfHeight + ZOffset;

            // 3) Build spawn transform
            FTransform SpawnXf;
            SpawnXf.SetLocation(ActualSpawnPos);

            FVector Dir          = (LocationToShoot - SpawnXf.GetLocation()).GetSafeNormal();

            // For homing multi-missiles, add a slight initial direction variance
            if (HomingCount > 0 && BaseCount > 1)
            {
                float Angle = (360.0f / BaseCount) * Count;
                FVector Right, Up;
                Dir.FindBestAxisVectors(Right, Up);
                Dir = (Dir + (Right * FMath::Cos(FMath::DegreesToRadians(Angle)) + Up * FMath::Sin(FMath::DegreesToRadians(Angle))) * 0.1f).GetSafeNormal();
            }

            const FRotator InitialRot  = Dir.Rotation() + ProjectileRotationOffset;
            SpawnXf.SetRotation(FQuat(InitialRot));
            SpawnXf.SetScale3D(ShootingUnit->ProjectileScale * Scale);

            // 4) Deferred spawn + init
            
            if (ProjectileCDO && ProjectileCDO->bUseMass)
            {
                float SpeedFromAttributes = Attributes ? Attributes->GetProjectileSpeed() : 0.f;
                float FinalSpeed = SpeedFromAttributes;

                // ROBUSTHEIT: Falls das Attribut 0 liefert, nutze den CDO-Standardwert
                if (FinalSpeed <= 0.f && ProjectileCDO)
                {
                    FinalSpeed = ProjectileCDO->MovementSpeed;
                }

                float InitialAngle = 0.f;
                float RotSpeed = 0.f;
                float MaxRadius = 0.f;
                float InterpSpeed = ProjectileCDO->HomingInterpSpeed;
                bool bFollow = ProjectileCDO->FollowTarget;

                if (HomingCount > 0)
                {
                    bFollow = true;
                    FinalSpeed += FMath::RandRange(-ProjectileCDO->HomingSpeedVariation, ProjectileCDO->HomingSpeedVariation);
                    FinalSpeed = FMath::Max(FinalSpeed, 100.f); // Mindestens 100 Einheiten/s bei Homing
                    InitialAngle = FMath::RandRange(0.f, 360.f);
                    RotSpeed = ProjectileCDO->HomingRotationSpeed * FMath::RandRange(0.9f, 1.4f);
                    if (FMath::RandBool()) RotSpeed *= -1.f;
                    MaxRadius = ProjectileCDO->HomingMaxSpiralRadius * FMath::RandRange(0.8f, 1.2f);
                }

                // Resolve Entity Handles for server/client replication
                FMassEntityHandle ShooterEntity;
                if (UMassActorBindingComponent* ShooterBind = ShootingUnit->FindComponentByClass<UMassActorBindingComponent>())
                {
                    ShooterEntity = ShooterBind->GetEntityHandle();
                }

                FMassEntityHandle TargetEntity;
                if (Aim)
                {
                    if (UMassActorBindingComponent* TargetBind = Aim->FindComponentByClass<UMassActorBindingComponent>())
                    {
                        TargetEntity = TargetBind->GetEntityHandle();
                    }
                }

                // 1) Spawn authoritative entity on Server
                if (UProjectileVisualManager* VisualManager = GetWorld()->GetSubsystem<UProjectileVisualManager>())
                {
                    VisualManager->SpawnMassProjectile(ProjectileClass, SpawnXf, Attacker, Aim, LocationToShoot, ShooterEntity, TargetEntity, FinalSpeed, TeamId, bFollow, InitialAngle, RotSpeed, MaxRadius, InterpSpeed, nullptr, SpawnXf.GetScale3D(), -1.f, MaxPiercedTargets, false, nullptr, nullptr, nullptr, FEffectAreaInfo());
                }
            }
            else
            {
                AProjectile* MyProj = Cast<AProjectile>(
                    UGameplayStatics::BeginDeferredActorSpawnFromClass(
                        this,
                        ProjectileClass,
                        SpawnXf,
                        ESpawnActorCollisionHandlingMethod::AlwaysSpawn
                    )
                );

                if (MyProj)
                {
                    // cache our manually computed aim point
                    MyProj->TargetLocation   = LocationToShoot;
            
                    if (HomingCount > 0) MyProj->FollowTarget = true;
                    else MyProj->FollowTarget = FollowTarget;
            
                    // Ensure projectile uses the same mesh rotation offset as the shooter
                    MyProj->RotationOffset = ShootingUnit->ProjectileRotationOffset;
            
                    MyProj->InitForAbility(Aim, Attacker);
            
                    //MyProj->Mesh_A->OnComponentBeginOverlap.AddDynamic(MyProj, &AProjectile::OnOverlapBegin);
                    MyProj->MaxPiercedTargets = MaxPiercedTargets;
                    MyProj->IsBouncingNext    = IsBouncingNext;
                    MyProj->IsBouncingBack    = IsBouncingBack;
            
                    MyProj->SetProjectileVisibility();
                    UGameplayStatics::FinishSpawningActor(MyProj, SpawnXf);
                    MyProj->SetReplicates(true);
                }
            }
        }
    }

    // 2) Increment replication counter for clients once for the whole burst
    if (RTSReplicationSettings::GetReplicationMode() == RTSReplicationSettings::Mass && HasAuthority())
    {
        const AProjectile* ProjectileCDO_Inner = ProjectileClass->GetDefaultObject<AProjectile>();
        float SpeedFromAttributes = Attributes ? Attributes->GetProjectileSpeed() : 0.f;
        if (SpeedFromAttributes <= 0.f) SpeedFromAttributes = ProjectileCDO_Inner->MovementSpeed;

        // Resolve Entity Handles
        FMassEntityHandle ShooterEntity;
        if (UMassActorBindingComponent* ShooterBind = ShootingUnit->FindComponentByClass<UMassActorBindingComponent>())
            ShooterEntity = ShooterBind->GetEntityHandle();

        FMassEntityHandle TargetEntity;
        if (Aim)
            if (UMassActorBindingComponent* TargetBind = Aim->FindComponentByClass<UMassActorBindingComponent>())
                TargetEntity = TargetBind->GetEntityHandle();

        IncrementMassProjectileFireCounter(
            ProjectileClass, 
            SpeedFromAttributes, 
            ShooterEntity, 
            TargetEntity, 
            0.f, // InitialAngle (will be randomized on client if needed)
            ProjectileCDO_Inner->HomingRotationSpeed, 
            ProjectileCDO_Inner->HomingMaxSpiralRadius, 
            ProjectileCDO_Inner->HomingInterpSpeed, 
            ProjectileCDO_Inner->FollowTarget || (HomingCount > 0), 
            AimCenter, // Pass center instead of spread location
            ShootingUnit->ProjectileScale * Scale, 
            Spread, 
            -1.f, 
            MaxPiercedTargets,
            ProjectileCount,
            IsBouncingNext,
            IsBouncingBack,
            ZOffset,
            SpawnOffset,
            DisableAutoZOffset,
            TwinDistance,
            nullptr,
            nullptr,
            nullptr,
            FEffectAreaInfo()
        );
    }
}

void AUnitBase::SpawnProjectileFromClassWithAim_Implementation(
    FVector Aim,
    TSubclassOf<AProjectile> ProjectileClass,
    int MaxPiercedTargets,
    int ProjectileCount,
    float Spread,
    bool IsBouncingNext,
    bool IsBouncingBack,
    float ZOffset,
    float Scale,
    FVector SpawnOffset,
    float ExtraDamage,
    TSubclassOf<class UGameplayEffect> NewEffect,
    TSubclassOf<class UGameplayEffect> NewEffect2,
    TSubclassOf<class UGameplayEffect> NewEffect3,
    FEffectAreaInfo AreaInfo
)
{
    if (!ProjectileClass)
        return;

    const AProjectile* ProjectileCDO = ProjectileClass->GetDefaultObject<AProjectile>();
    float TwinDistance = ProjectileCDO->TwinProjectileDistance;
    int32 HomingCount = ProjectileCDO->HomingMissleCount;

    if (HomingCount > 0) HomingCount = ProjectileCount;

    int32 BaseCount = (HomingCount > 0) ? HomingCount : ProjectileCount;
    float FinalDamage = ProjectileCDO->Damage + ExtraDamage;

	// --- ADD THIS SECTION to determine spawner's location for Aim Direction ---
	FVector SpawnerLocationForAimDir = GetMassActorLocation(); // Default to actor's root location

    // Base spawn‐origin offset
    FVector CenterSpawnOrigin = GetProjectileSpawnLocation();

    TArray<FVector> SpawnPositions;
    if (TwinDistance >= 10.f)
    {
        FVector DirToTarget = (Aim - CenterSpawnOrigin).GetSafeNormal2D();
        FVector RightVector = DirToTarget.IsNearlyZero() ? GetActorRightVector() : FVector::CrossProduct(FVector::UpVector, DirToTarget);
        FVector RightOffset = RightVector * TwinDistance;
        SpawnPositions.Add(CenterSpawnOrigin - RightOffset); // Left
        SpawnPositions.Add(CenterSpawnOrigin + RightOffset); // Right
    }
    else
    {
        SpawnPositions.Add(CenterSpawnOrigin);
    }

    for (const FVector& ActualSpawnOrigin : SpawnPositions)
    {
        for (int32 i = 0; i < BaseCount; ++i)
        {
            // Alternate left/right offsets for multi‐shot spread
            const int   MultiAngle    = (i == 0) ? 0 : ((i & 1) ? 1 : -1);
            const FVector ToAimDir     = (Aim - SpawnerLocationForAimDir).GetSafeNormal();
            
            float ActualSpread = Spread;
            if (HomingCount <= 0)
            {
                if (i == 0) ActualSpread = 0.f;
                else if (i == 1 || i == 2) ActualSpread = Spread;
                else if (i == 3 || i == 4) ActualSpread = Spread / 2.0f;
                else if (i == 5 || i == 6) ActualSpread = Spread + (Spread / 2.0f);
                else ActualSpread = Spread + (i / 2) * (Spread / 2.0f);
            }
            else if (ProjectileCount <= 1)
            {
                ActualSpread = 0.f;
            }

            const FVector SpreadOffset = FRotator(0.f, MultiAngle * 90.f, 0.f)
                                         .RotateVector(ToAimDir)
                                         * ActualSpread;

            // Compute this shot’s exact world target point
            FVector LocationToShoot = Aim + SpreadOffset;
            // Overwrite Z so we use the Aim.Z (instead of adding our own Z)
            LocationToShoot.Z = Aim.Z + ZOffset;

            // Build the spawn transform
            FTransform SpawnXf;
            FVector FinalSpawnPos = ActualSpawnOrigin + GetActorRotation().RotateVector(SpawnOffset);
            SpawnXf.SetLocation(FinalSpawnPos);

            FVector Dir         = (LocationToShoot - FinalSpawnPos).GetSafeNormal();

            // For homing multi-missiles, add a slight initial direction variance
            if (HomingCount > 0 && BaseCount > 1)
            {
                float Angle = (360.0f / BaseCount) * i;
                FVector Right, Up;
                Dir.FindBestAxisVectors(Right, Up);
                Dir = (Dir + (Right * FMath::Cos(FMath::DegreesToRadians(Angle)) + Up * FMath::Sin(FMath::DegreesToRadians(Angle))) * 0.1f).GetSafeNormal();
            }

            const FRotator InitialRot = Dir.Rotation() + ProjectileRotationOffset;
            SpawnXf.SetRotation(FQuat(InitialRot));
            SpawnXf.SetScale3D(ProjectileScale * Scale);

            // Spawn deferred so we can Init
            if (ProjectileCDO && ProjectileCDO->bUseMass)
            {
                float SpeedFromAttributes = Attributes ? Attributes->GetProjectileSpeed() : 0.f;
                float FinalSpeed = SpeedFromAttributes;

                // ROBUSTHEIT: Falls das Attribut 0 liefert, nutze den CDO-Standardwert
                if (FinalSpeed <= 0.f && ProjectileCDO)
                {
                    FinalSpeed = ProjectileCDO->MovementSpeed;
                }

                float InitialAngle = 0.f;
                float RotSpeed = 0.f;
                float MaxRadius = 0.f;
                float InterpSpeed = ProjectileCDO->HomingInterpSpeed;
                
                // For aimed shots (static point), we only want to "follow" if it's a homing projectile
                // Non-homing aimed shots should be linear to allow flying past the target easily
                bool bFollow = false;

                if (HomingCount > 0)
                {
                    bFollow = true;
                    FinalSpeed += FMath::RandRange(-ProjectileCDO->HomingSpeedVariation, ProjectileCDO->HomingSpeedVariation);
                    FinalSpeed = FMath::Max(FinalSpeed, 100.f); // Mindestens 100 Einheiten/s bei Homing
                    InitialAngle = FMath::RandRange(0.f, 360.f);
                    RotSpeed = ProjectileCDO->HomingRotationSpeed * FMath::RandRange(0.9f, 1.4f);
                    if (FMath::RandBool()) RotSpeed *= -1.f;
                    MaxRadius = ProjectileCDO->HomingMaxSpiralRadius * FMath::RandRange(0.8f, 1.2f);
                }

                // Resolve Shooter Entity Handle
                FMassEntityHandle ShooterEntity;
                if (UMassActorBindingComponent* ShooterBind = FindComponentByClass<UMassActorBindingComponent>())
                {
                    ShooterEntity = ShooterBind->GetEntityHandle();
                }

                // 1) Spawn authoritative entity on Server
                if (UProjectileVisualManager* VisualManager = GetWorld()->GetSubsystem<UProjectileVisualManager>())
                {
                    VisualManager->SpawnMassProjectile(ProjectileClass, SpawnXf, this, nullptr, LocationToShoot, ShooterEntity, FMassEntityHandle(), FinalSpeed, TeamId, bFollow, InitialAngle, RotSpeed, MaxRadius, InterpSpeed, nullptr, SpawnXf.GetScale3D(), FinalDamage, MaxPiercedTargets, false, NewEffect, NewEffect2, NewEffect3, AreaInfo);
                }
            }
            else
            {
                AProjectile* Proj = Cast<AProjectile>(
                    UGameplayStatics::BeginDeferredActorSpawnFromClass(
                        this,
                        ProjectileClass,
                        SpawnXf,
                        ESpawnActorCollisionHandlingMethod::AlwaysSpawn
                    )
                );

                if (Proj)
                {
                    // Cache our manual location‐aim
                    Proj->TargetLocation    = LocationToShoot;
            
                    // Initialize in-projectile rotation offset from this unit
                    Proj->RotationOffset = ProjectileRotationOffset;
            
                    Proj->InitForLocationPosition(LocationToShoot, this);
            
                    //Proj->Mesh_A->OnComponentBeginOverlap.AddDynamic(Proj, &AProjectile::OnOverlapBegin);
                    Proj->MaxPiercedTargets = MaxPiercedTargets;
                    Proj->Damage = FinalDamage;
                    Proj->IsBouncingNext    = IsBouncingNext;
                    Proj->IsBouncingBack    = IsBouncingBack;
                    Proj->ProjectileEffect = NewEffect;
                    Proj->ProjectileEffect2 = NewEffect2;
                    Proj->ProjectileEffect3 = NewEffect3;
            
                    Proj->SetProjectileVisibility();
                    UGameplayStatics::FinishSpawningActor(Proj, SpawnXf);
                    Proj->SetReplicates(true);
                }
            }
        }
    }

    // 2) Increment replication counter for clients once for the whole burst
    if (RTSReplicationSettings::GetReplicationMode() == RTSReplicationSettings::Mass && HasAuthority())
    {
        const AProjectile* ProjectileCDO_Inner = ProjectileClass->GetDefaultObject<AProjectile>();
        float SpeedFromAttributes = Attributes ? Attributes->GetProjectileSpeed() : 0.f;
        if (SpeedFromAttributes <= 0.f) SpeedFromAttributes = ProjectileCDO_Inner->MovementSpeed;

        FMassEntityHandle ShooterEntity;
        if (UMassActorBindingComponent* ShooterBind = FindComponentByClass<UMassActorBindingComponent>())
            ShooterEntity = ShooterBind->GetEntityHandle();

        IncrementMassProjectileFireCounter(
            ProjectileClass, 
            SpeedFromAttributes, 
            ShooterEntity, 
            FMassEntityHandle(), 
            0.f, 
            ProjectileCDO_Inner->HomingRotationSpeed, 
            ProjectileCDO_Inner->HomingMaxSpiralRadius, 
            ProjectileCDO_Inner->HomingInterpSpeed, 
            HomingCount > 0, 
            Aim, 
            ProjectileScale * Scale, 
            Spread, 
            FinalDamage, 
            MaxPiercedTargets, 
            ProjectileCount, 
            IsBouncingNext, 
            IsBouncingBack, 
            ZOffset, 
            SpawnOffset, 
            false, 
            TwinDistance, 
            NewEffect, 
            NewEffect2, 
            NewEffect3, 
            AreaInfo
        );
    }
}

bool AUnitBase::SetNextUnitToChase()
{
	// Entferne alle Einheiten, die ungültig, tot oder außerhalb der Sichtweite sind.
	UnitsToChase.RemoveAll([this](const AActor* Actor) -> bool {
		if (!IsValid(Actor)) return true;

		if (const AUnitBase* Unit = Cast<AUnitBase>(Actor))
		{
			return Unit->GetUnitState() == UnitData::Dead || GetDistanceTo(Unit) > MassActorBindingComponent->SightRadius;
		}

		if (const AEffectArea* EffectArea = Cast<AEffectArea>(Actor))
		{
			return GetDistanceTo(EffectArea) > MassActorBindingComponent->SightRadius;
		}

		return GetDistanceTo(Actor) > MassActorBindingComponent->SightRadius;
	});
	
	if (UnitsToChase.IsEmpty()) return false;
    
	float ShortestDistance = TNumericLimits<float>::Max();
	AActor* ClosestActor = nullptr;

	for (auto& Actor : UnitsToChase)
	{
		if (Actor)
		{
			bool bIsDead = false;
			if (const AUnitBase* Unit = Cast<AUnitBase>(Actor))
			{
				bIsDead = (Unit->GetUnitState() == UnitData::Dead);
			}

			if (!bIsDead)
			{
				float Distance = GetDistanceTo(Actor);
				if (Distance < ShortestDistance)
				{
					ShortestDistance = Distance;
					ClosestActor = Actor;
				}
			}
		}
	}

	// Set the closest living unit as the target, if any.
	if (ClosestActor)
	{
		UnitToChase = ClosestActor;
		return true;
	}

	return false;
}


TArray<AUnitBase*> AUnitBase::SpawnUnitsFromParameters(
TSubclassOf<class AUnitBase> UnitBaseClass, UMaterialInstance* Material, USkeletalMesh* CharacterMesh, FRotator HostMeshRotation, FVector Location,
TEnumAsByte<UnitData::EState> UState,
TEnumAsByte<UnitData::EState> UStatePlaceholder,
int NewTeamId, FBuildingCost UsedConstructionCost, AWaypoint* Waypoint, int UnitCount, bool SummonContinuously, bool SpawnAsSquad, bool UseSummonDataSet, bool bSelectable,
bool bDoGroundTrace, float WaypointDirectionOffset, FVector OffsetLocation)
{
	TArray<AUnitBase*> SpawnedUnits;

	if (!IsValid(this))
	{
		return SpawnedUnits;
	}

	FVector BaseSpawnLocation = Location + OffsetLocation;

	// --------------------------------------------------------------------------------------------
	// Spawnrichtung: der Versatz vom Spawner weg wurde bisher NUR gesetzt, wenn ein gueltiger
	// Wegpunkt vorlag. Die KI baut aber ohne Wegpunkt - GA_BuildUnit_Parent uebergibt dort 0 -,
	// also griff der Versatz bei ihr nie und die Einheit erschien praktisch auf dem Gebaeude.
	// Genau dort stanzt das Gebaeude ein Loch ins Navigationsnetz: die Einheit steht ausserhalb
	// des begehbaren Bereichs, jede Pfadsuche scheitert, und die Rueckhol-Hilfe
	// (UUnitSoftAvoidanceProcessor) greift nicht, weil ihre eigene Projektion mitten im Loch
	// ebenfalls fehlschlaegt - dort entfernt sie nur die Markierung und setzt keine Kraft.
	// Ergebnis war ein Arbeiter, der direkt nach dem Spawn am Gebaeude haengen bleibt.
	//
	// Es gibt jetzt immer eine Richtung: der Wegpunkt, wenn vorhanden, sonst die Blickrichtung
	// des Spawners, im Notfall +X. Damit kann der Versatz nie ausfallen.
	// --------------------------------------------------------------------------------------------
	const bool bSpawnGuard = CVarRTSSpawnNavSicherung.GetValueOnAnyThread() != 0;

	if (WaypointDirectionOffset > 0.f && (bSpawnGuard || (Waypoint && IsValid(Waypoint))))
	{
		FVector Direction = FVector::ZeroVector;
		if (Waypoint && IsValid(Waypoint))
		{
			Direction = (Waypoint->GetActorLocation() - Location).GetSafeNormal2D();
		}
		if (Direction.IsNearlyZero())
		{
			Direction = GetActorForwardVector().GetSafeNormal2D();
		}
		if (Direction.IsNearlyZero())
		{
			Direction = FVector(1.f, 0.f, 0.f);
		}
		const float SpawnerRadius = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleRadius() : 0.f;
		BaseSpawnLocation += Direction * (SpawnerRadius + WaypointDirectionOffset);
	}

	FUnitSpawnParameter SpawnParameter;
	SpawnParameter.UnitBaseClass = UnitBaseClass;
	SpawnParameter.UnitOffset = FVector3d(0.f,0.f,0.f);
	SpawnParameter.ServerMeshRotation = HostMeshRotation;
	// Deliberate rotation from the caller - mark it so the spawn path applies it.
	SpawnParameter.bOverrideServerMeshRotation = true;
	SpawnParameter.State = UState;
	SpawnParameter.StatePlaceholder = UStatePlaceholder;
	SpawnParameter.Material = Material;
	SpawnParameter.CharacterMesh = CharacterMesh;
	SpawnParameter.ConstructionCost = UsedConstructionCost;
	// Waypointspawn
	
	if (!SpawnParameter.UnitBaseClass) return SpawnedUnits;
		
	ARTSGameModeBase* GameMode = Cast<ARTSGameModeBase>(GetWorld()->GetAuthGameMode());
	
	if(!GameMode) return SpawnedUnits;

	int32 SharedSquadId = 0;
	if (SpawnAsSquad)
	{
		GameMode->HighestSquadId++;
		SharedSquadId = GameMode->HighestSquadId;
	}

	// Spawnabstand: der Kapselradius der zu spawnenden Klasse, doppelt, plus 10.
	//
	// Vorher hing `FinalSpawnLocation` NICHT von `i` ab - `UnitOffset` steht auf (0,0,0) und
	// wurde in der Schleife nie veraendert. Alle Einheiten eines Schubs erschienen also exakt
	// auf demselben Punkt. Der Abstand war nicht zu klein, er war null. Auseinander geschoben
	// hat sie erst `AdjustIfPossibleButAlwaysSpawn` und die Separation danach - genau das
	// beobachtete Abstossen. Das kann Einheiten auch auf angrenzende Geometrie druecken, was
	// zum Befund aus #111 passt (Arbeiter 330-481 Einheiten ueber der begehbaren Flaeche).
	float SpawnSpacing = 100.f;
	if (UnitBaseClass)
	{
		if (const AUnitBase* DefaultUnit = UnitBaseClass->GetDefaultObject<AUnitBase>())
		{
			if (const UCapsuleComponent* Capsule = DefaultUnit->GetCapsuleComponent())
			{
				SpawnSpacing = Capsule->GetScaledCapsuleRadius() * 2.f + 10.f;
			}
		}
	}

	// Ringfoermige Verteilung: die erste Einheit in die Mitte, danach Ringe mit 6*Ring
	// Plaetzen im Abstand Ring*SpawnSpacing. Auf diese Weise liegt jeder Nachbar - im Ring
	// wie zwischen zwei Ringen - mindestens SpawnSpacing entfernt, und die Gruppe waechst
	// kompakt nach aussen statt in einer langen Reihe.
	auto RingOffset = [SpawnSpacing](int32 Index) -> FVector
	{
		if (Index <= 0) return FVector::ZeroVector;
		int32 Ring = 1;
		int32 Erster = 1;              // erster Index dieses Rings
		while (Index >= Erster + 6 * Ring)
		{
			Erster += 6 * Ring;
			++Ring;
		}
		const int32 PlatzImRing = Index - Erster;
		const int32 PlaetzeImRing = 6 * Ring;
		const float Winkel = (2.f * PI * PlatzImRing) / PlaetzeImRing;
		const float Radius = Ring * SpawnSpacing;
		return FVector(FMath::Cos(Winkel) * Radius, FMath::Sin(Winkel) * Radius, 0.f);
	};

	// --------------------------------------------------------------------------------------------
	// Absicherung jeder einzelnen Spawnstelle: der Punkt muss auf dem Navigationsnetz liegen.
	//
	// Bisher wurde die berechnete Stelle ungeprueft benutzt. Sie kann aber im Loch liegen, das ein
	// Gebaeude ins Netz stanzt - bei Ringplatz 0 sogar zwangslaeufig, wenn kein Versatz griff.
	// Wer dort steht, findet keinen Pfad und wird von der Rueckhol-Hilfe nicht erfasst.
	//
	// Ablauf: erst am Wunschpunkt projizieren; schlaegt das fehl oder landet die Projektion in
	// einem Sperrbereich (NavArea_Obstacle, z.B. Gebaeudegrundriss), ringfoermig nach aussen
	// weitersuchen. Findet sich gar nichts, bleibt es beim Wunschpunkt - gespawnt wird immer,
	// eine Einheit darf nicht verloren gehen, nur weil die Karte an der Stelle unklar ist.
	// --------------------------------------------------------------------------------------------
	UNavigationSystemV1* SpawnNavSystem = UNavigationSystemV1::GetCurrent(GetWorld());

	auto IsWalkable = [](UNavigationSystemV1* Nav, const FNavLocation& Kandidat) -> bool
	{
		if (!Nav) return false;
		if (const ARecastNavMesh* Recast = Cast<ARecastNavMesh>(Nav->GetNavDataForProps(FNavAgentProperties())))
		{
			const uint32 AreaID = Recast->GetPolyAreaID(Kandidat.NodeRef);
			const UClass* AreaClass = Recast->GetAreaClass(AreaID);
			if (AreaClass && AreaClass->IsChildOf(UNavArea_Obstacle::StaticClass()))
			{
				return false;
			}
		}
		return true;
	};

	auto SnapToWalkableGround = [&](const FVector& Wunsch, bool& bGefunden) -> FVector
	{
		bGefunden = false;
		if (!SpawnNavSystem) return Wunsch;
		if (CVarRTSSpawnNavSicherung.GetValueOnAnyThread() == 0) return Wunsch;   // Vergleichsmessung

		const float Weite = FMath::Max(200.f, SpawnSpacing * 2.f);
		const FVector Suchbox(Weite, Weite, 500.f);

		FNavLocation Treffer;
		if (SpawnNavSystem->ProjectPointToNavigation(Wunsch, Treffer, Suchbox) && IsWalkable(SpawnNavSystem, Treffer))
		{
			bGefunden = true;
			return Treffer.Location;
		}

		// Ringfoermig nach aussen: die Radien decken auch grosse Hauptgebaeude ab.
		static const float Radien[] = { 200.f, 400.f, 800.f, 1600.f };
		static const int32 Segmente = 12;
		for (float R : Radien)
		{
			for (int32 Segment = 0; Segment < Segmente; ++Segment)
			{
				const float Winkel = (2.f * PI * Segment) / Segmente;
				const FVector Kandidat = Wunsch + FVector(FMath::Cos(Winkel) * R, FMath::Sin(Winkel) * R, 0.f);
				FNavLocation KandidatNav;
				if (SpawnNavSystem->ProjectPointToNavigation(Kandidat, KandidatNav, Suchbox) && IsWalkable(SpawnNavSystem, KandidatNav))
				{
					bGefunden = true;
					return KandidatNav.Location;
				}
			}
		}
		return Wunsch;
	};

	for(int i = 0; i < UnitCount; i++)
	{
		FTransform UnitTransform;

		FVector FinalSpawnLocation = BaseSpawnLocation + (FVector)SpawnParameter.UnitOffset + RingOffset(i);

		// Erst auf begehbaren Boden ziehen, dann die Hoehe bestimmen.
		bool bOnNavMesh = false;
		const FVector DesiredSpot = FinalSpawnLocation;
		FinalSpawnLocation = SnapToWalkableGround(FinalSpawnLocation, bOnNavMesh);
		const float NavHeight = FinalSpawnLocation.Z;

		// Belegzeile fuer die Absicherung. Jede Korrektur ueber ein paar Einheiten ist eine
		// Einheit, die vorher neben dem begehbaren Bereich gelandet waere - genau der Fall,
		// in dem ein Arbeiter am Gebaeude haengen blieb. "kein Netz" heisst: auch die
		// Ringsuche fand nichts, dort bleibt ein Restrisiko.
		{
			const float Korrektur = FVector::Dist2D(DesiredSpot, FinalSpawnLocation);
			// Nur melden, wenn die Absicherung ueberhaupt laufen sollte. Bei abgeschaltetem
			// rts.spawn.navsicherung kehrt das Lambda sofort zurueck - dann hiesse "kein
			// begehbarer Punkt" nur "nicht gesucht", und die Zeile wuerde in die Irre fuehren.
			if (!bOnNavMesh && bSpawnGuard)
			{
				UE_LOG(LogTemp, Warning, TEXT("[SpawnDiag] Team %d Platz %d: kein begehbarer Punkt gefunden, Spawn auf Wunschstelle"), NewTeamId, i);
			}
			else if (Korrektur > 25.f)
			{
				UE_LOG(LogTemp, Log, TEXT("[SpawnDiag] Team %d Platz %d: um %.0f uu auf das Netz gezogen"), NewTeamId, i, Korrektur);
			}
		}

		if (bDoGroundTrace)
		{
			FHitResult HitResult;
			// Start dicht ueber der abgesicherten Stelle statt 1000 darueber: von weit oben trifft
			// der Strahl bei dicht bebauten Basen ein Gebaeudedach und die Einheit erscheint darauf.
			FVector TraceStart = FinalSpawnLocation + FVector(0.f, 0.f, 500.f);
			FVector TraceEnd = FinalSpawnLocation - FVector(0.f, 0.f, 1000.f);
			FCollisionQueryParams TraceParams(FName(TEXT("SpawnTrace")), true, this);

			if (GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, TraceParams))
			{
				float CapsuleHalfHeight = 0.f;
				if (UnitBaseClass)
				{
					if (AUnitBase* DefaultUnit = UnitBaseClass->GetDefaultObject<AUnitBase>())
					{
						if (UCapsuleComponent* Capsule = DefaultUnit->GetCapsuleComponent())
						{
							CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
						}
					}
				}
				const float TraceHoehe = HitResult.Location.Z + CapsuleHalfHeight + OffsetLocation.Z;

				// Weicht der Bodentreffer stark von der Netzhoehe ab, hat er etwas anderes als den
				// Boden erwischt (Dach, Anbau). Dann gilt die Netzhoehe - dort ist die Einheit
				// nachweislich lauffaehig.
				if (bOnNavMesh && FMath::Abs(TraceHoehe - (NavHeight + CapsuleHalfHeight)) > 300.f)
				{
					FinalSpawnLocation.Z = NavHeight + CapsuleHalfHeight + OffsetLocation.Z;
				}
				else
				{
					FinalSpawnLocation.Z = TraceHoehe;
				}
			}
			else if (bOnNavMesh)
			{
				// Kein Bodentreffer: die Netzhoehe ist die verlaesslichere Angabe.
				float CapsuleHalfHeight = 0.f;
				if (UnitBaseClass)
				{
					if (AUnitBase* DefaultUnit = UnitBaseClass->GetDefaultObject<AUnitBase>())
					{
						if (UCapsuleComponent* Capsule = DefaultUnit->GetCapsuleComponent())
						{
							CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
						}
					}
				}
				FinalSpawnLocation.Z = NavHeight + CapsuleHalfHeight + OffsetLocation.Z;
			}
		}

		UnitTransform.SetLocation(FinalSpawnLocation);
		
		const auto UnitBase = Cast<AUnitBase>
			(UGameplayStatics::BeginDeferredActorSpawnFromClass
			(this, *SpawnParameter.UnitBaseClass, UnitTransform, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn));
	
		if (UnitBase != nullptr)
		{

			if (SpawnParameter.CharacterMesh)
			{
				UnitBase->MeshAssetPath = SpawnParameter.CharacterMesh->GetPathName();
			}
		
			if (SpawnParameter.Material)
			{
				UnitBase->MeshMaterialPath = SpawnParameter.Material->GetPathName();
			}
		
			if(NewTeamId)
			{
				UnitBase->TeamId = NewTeamId;
			}

			// Rows only dictate the mesh rotation when they say so; otherwise the unit's own
			// class default stands, exactly as it does for hand-placed units.
			if (SpawnParameter.bOverrideServerMeshRotation)
			{
				UnitBase->ServerMeshRotation = SpawnParameter.ServerMeshRotation;
			}
			
			UnitBase->OnRep_MeshAssetPath();
			UnitBase->OnRep_MeshMaterialPath();
			
			UnitBase->SetMeshRotationServer();
		
			UnitBase->UnitState = SpawnParameter.State;
			UnitBase->StoredUnitState = SpawnParameter.State;
			UnitBase->UnitStatePlaceholder = SpawnParameter.StatePlaceholder;
			UnitBase->ConstructionCost = UsedConstructionCost;

			// Record what the production ability actually billed as supply, so the unit hands back
			// exactly that on death.
			//
			// Before this, the charge came from the ability's cost and the refund from the unit's
			// own UnitSpaceNeeded - two numbers that nobody keeps in step. A unit whose ability
			// charges no supply but whose Blueprint says UnitSpaceNeeded=1 refunded one point it
			// had never paid, and the used amount walked into the negative: "-55/70".
			if (AResourceGameMode* SupplyGameMode = Cast<AResourceGameMode>(GameMode))
			{
				int32 BilledSupply = 0;
				if (SupplyGameMode->IsSupplyLikeResource(EResourceType::Primary))   BilledSupply += UsedConstructionCost.PrimaryCost;
				if (SupplyGameMode->IsSupplyLikeResource(EResourceType::Secondary)) BilledSupply += UsedConstructionCost.SecondaryCost;
				if (SupplyGameMode->IsSupplyLikeResource(EResourceType::Tertiary))  BilledSupply += UsedConstructionCost.TertiaryCost;
				if (SupplyGameMode->IsSupplyLikeResource(EResourceType::Rare))      BilledSupply += UsedConstructionCost.RareCost;
				if (SupplyGameMode->IsSupplyLikeResource(EResourceType::Epic))      BilledSupply += UsedConstructionCost.EpicCost;
				if (SupplyGameMode->IsSupplyLikeResource(EResourceType::Legendary)) BilledSupply += UsedConstructionCost.LegendaryCost;

				// Auf die Einheiten dieses Aufrufs AUFTEILEN. Bezahlt wird EINMAL je Aktivierung
				// (der Blueprint ruft ModifyResourceCCost einmal und prueft den Rueckgabewert),
				// gespawnt werden aber UnitCount Einheiten. Wer jeder davon den vollen Betrag
				// aufstempelt, gibt beim Sterben das Vielfache zurueck - genau das trieb den
				// Verbrauch unter null. Der Rest geht an die ersten Einheiten, damit die Summe
				// ueber den Schub exakt dem Bezahlten entspricht.
				const int32 Gesamt = FMath::Max(1, UnitCount);
				const int32 ProEinheit = FMath::Max(0, BilledSupply) / Gesamt;
				const int32 Rest = FMath::Max(0, BilledSupply) % Gesamt;
				UnitBase->ChargedSupplyAmount = ProEinheit + ((i < Rest) ? 1 : 0);
				UnitBase->bSupplyAmountKnown = true;
			}
			
			if(UnitToChase && IsValid(UnitToChase))
			{
				UnitBase->UnitToChase = UnitToChase;
				UnitBase->SetUnitState(UnitData::Chase);
			}
		
			UGameplayStatics::FinishSpawningActor(
			 Cast<AActor>(UnitBase), 
			 UnitTransform
			);
			UnitBase->ForceNetUpdate();

			UnitBase->InitializeAttributes();
			UnitBase->SquadId = (SpawnAsSquad ? SharedSquadId : 0);
			// Ensure squad healthbar setup on server right after SquadId assignment
			UnitBase->EnsureSquadHealthbarState();
			
			if(Waypoint && IsValid(Waypoint))
				UnitBase->SetWaypoint(Waypoint);

			UnitBase->ScheduleDelayedNavigationUpdate();
			// Apply selectability flag from params
			UnitBase->CanBeSelected = bSelectable;
			// Optionally set follow target to this spawner
			
			{
				int UIndex = GameMode->AddUnitIndexAndAssignToAllUnitsArray(UnitBase);
				// Only use SummonedUnitsDataSet when explicitly requested and not summoning continuously
				if (UseSummonDataSet && !SummonContinuously)
				{
					FUnitSpawnData UnitSpawnDataSet;
					UnitSpawnDataSet.Id = SpawnParameter.Id;
					UnitSpawnDataSet.UnitBase = UnitBase;
					UnitSpawnDataSet.SpawnParameter = SpawnParameter;
					SummonedUnitsDataSet.Add(UnitSpawnDataSet);
					// Remove dead entries after each spawn
					GetAliveUnitsInDataSet();
				}
			}
			
			SpawnedUnits.Add(UnitBase);
		}
	}
	
	return SpawnedUnits;
}

void AUnitBase::ApplyFollowTarget(AUnitBase* NewFollowTarget)
{
	// Delegate implementation to MassUnit base helper so logic lives in parent class
	ApplyFollowTargetForUnit(this, NewFollowTarget);
}

bool AUnitBase::IsSpawnedUnitDead(int UIndex)
{

	for (int32 i = SummonedUnitsDataSet.Num() - 1; i >= 0; --i)
	{
		FUnitSpawnData& UnitData = SummonedUnitsDataSet[i];
		// Assuming that UnitBase has a member variable UnitIndex to match with UnitIndex
		if (UnitData.UnitBase && UnitData.UnitBase->UnitIndex == UIndex)
		{
			// Assuming AUnitBase has a method or property named IsDead to check if the unit is dead
			return (UnitData.UnitBase->GetUnitState() == UnitData::Dead);
		}
	}
	
	// If no unit matches the UnitIndex, you can either return false, 
	// or handle it based on how you want to treat units that are not found
	return true;
}

void AUnitBase::SetUnitBase(int UIndex, AUnitBase* NewUnit)
{

	for (int32 i = SummonedUnitsDataSet.Num() - 1; i >= 0; --i)
	{
		FUnitSpawnData& UnitData = SummonedUnitsDataSet[i];
		// Assuming that UnitBase has a member variable UnitIndex to match with UnitIndex
		if (UnitData.UnitBase && UnitData.UnitBase->UnitIndex == UIndex)
		{
			UnitData.UnitBase->SaveAbilityAndLevelData(FString::FromInt(UnitData.UnitBase->UnitIndex));
			UnitData.UnitBase->Destroy(true);
			UnitData.UnitBase = NewUnit;
			UnitData.UnitBase->LoadAbilityAndLevelData(FString::FromInt(UnitData.UnitBase->UnitIndex));
		}
	}

	// If no unit matches the UnitIndex, you can either return false, 
	// or handle it based on how you want to treat units that are not found
}


void AUnitBase::ScheduleDelayedNavigationUpdate()
{
// Force NavMesh update for this unit

	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
	if (NavSys)
	{
		NavSys->UpdateActorInNavOctree(*this);
	}
	
	// Optional: Add delay if needed for physics stabilization
	GetWorld()->GetTimerManager().SetTimerForNextTick([this](){
		UpdateNavigationRelevance();
	});

}

void AUnitBase::IncrementMassProjectileFireCounter(TSubclassOf<class AProjectile> ProjectileClass, float Speed, FMassEntityHandle ShooterEntity, FMassEntityHandle TargetEntity,
    float InitialAngle, float RotSpeed, float MaxRadius, float InterpSpeed, bool bFollow, FVector TargetLocation, FVector Scale, float Spread, float Damage, int32 MaxPiercedTargets,
    int32 ProjectileCount, bool IsBouncingNext, bool IsBouncingBack, float ZOffset, FVector SpawnOffset, bool DisableAutoZOffset, float TwinProjectileDistance, TSubclassOf<class UGameplayEffect> ProjectileEffect, TSubclassOf<class UGameplayEffect> ProjectileEffect2, TSubclassOf<class UGameplayEffect> ProjectileEffect3, FEffectAreaInfo AreaInfo)
{
    if (!ProjectileClass || !HasAuthority()) return;

    FMassEntityHandle EntityToUse = ShooterEntity;
    if (!EntityToUse.IsValid())
    {
        if (UMassActorBindingComponent* Binding = FindComponentByClass<UMassActorBindingComponent>())
        {
            EntityToUse = Binding->GetEntityHandle();
        }
    }

    if (!EntityToUse.IsValid())
    {
        return;
    }

    if (UMassEntitySubsystem* MassSubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>())
    {
        FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();
        if (FMassAIStateFragment* AIS = EntityManager.GetFragmentDataPtr<FMassAIStateFragment>(EntityToUse))
        {
            AIS->ProjectileFireCounter++;
            AIS->LastProjectileClass = ProjectileClass;
            AIS->LastProjectileSpeed = Speed;
            AIS->LastHomingInitialAngle = InitialAngle;
            AIS->LastHomingRotationSpeed = RotSpeed;
            AIS->LastHomingMaxSpiralRadius = MaxRadius;
            AIS->LastHomingInterpSpeed = InterpSpeed;
            AIS->LastbFollowTarget = bFollow;
            AIS->LastProjectileTargetLocation = TargetLocation;
            AIS->LastProjectileScale = Scale;
            AIS->LastProjectileSpread = Spread;
            AIS->LastProjectileDamage = Damage;
            AIS->LastProjectileMaxPiercedTargets = MaxPiercedTargets;
            AIS->LastProjectileCount = ProjectileCount;
            AIS->LastIsBouncingNext = IsBouncingNext;
            AIS->LastIsBouncingBack = IsBouncingBack;
            AIS->LastZOffset = ZOffset;
            AIS->LastProjectileSpawnOffset = SpawnOffset;
            AIS->LastDisableAutoZOffset = DisableAutoZOffset;
            AIS->LastTwinProjectileDistance = TwinProjectileDistance;
            AIS->LastProjectileEffect = ProjectileEffect;
            AIS->LastProjectileEffect2 = ProjectileEffect2;
            AIS->LastProjectileEffect3 = ProjectileEffect3;
          		AIS->LastAreaInfo = AreaInfo;

            // Resolve target NetID
            AIS->LastTargetNetID = 0;
            if (TargetEntity.IsValid())
            {
                if (const FMassNetworkIDFragment* NetIDFrag = EntityManager.GetFragmentDataPtr<FMassNetworkIDFragment>(TargetEntity))
                {
                    AIS->LastTargetNetID = NetIDFrag->NetID.GetValue();
                }
            }
        }
    }
}


void AUnitBase::HandleProjectileImpact_Implementation(AActor* Shooter, const FVector& ImpactLocation, TSubclassOf<class AProjectile> ProjectileClass, float DamageOverride, TSubclassOf<class UGameplayEffect> ProjectileEffect, TSubclassOf<class UGameplayEffect> ProjectileEffect2, TSubclassOf<class UGameplayEffect> ProjectileEffect3)
{
	if (!ProjectileClass)
	{
		return;
	}

	const AProjectile* CDO = Cast<AProjectile>(ProjectileClass->GetDefaultObject());
	if (!CDO)
	{
		return;
	}

	// Mark as attacked
	Attacked(Shooter);

	// Calculate Damage
	float NewDamage = (DamageOverride >= 0.f) ? DamageOverride : CDO->Damage;
	AUnitBase* ShootingUnit = Cast<AUnitBase>(Shooter);

	if (ShootingUnit)
	{
		const UAttributeSetBase* ShooterAttributes = ShootingUnit->Attributes;
		if (ShooterAttributes)
		{
			if (CDO->UseAttributeDamage && DamageOverride < 0.f)
			{
				NewDamage = ShooterAttributes->GetAttackDamage() - Attributes->GetArmor();
				if (ShootingUnit->IsDoingMagicDamage)
					NewDamage = ShooterAttributes->GetAttackDamage() - Attributes->GetMagicResistance();
			}
			else if (DamageOverride < 0.f)
			{
				NewDamage = CDO->Damage - Attributes->GetArmor();
				if (ShootingUnit->IsDoingMagicDamage)
					NewDamage = CDO->Damage - Attributes->GetMagicResistance();
			}
		}
	}

    if (CDO->IsHealing)
    {
        if (Attributes)
        {
            SetHealth_Implementation(FMath::Min(Attributes->GetMaxHealth(), Attributes->GetHealth() + NewDamage));
        }
    }
    else
    {
        UAttributeSetBase* CurrentAttributes = Attributes;
        if (CurrentAttributes && CurrentAttributes->GetShield() <= 0)
            SetHealth_Implementation(CurrentAttributes->GetHealth() - NewDamage);
        else if (CurrentAttributes)
            SetShield_Implementation(CurrentAttributes->GetShield() - NewDamage);

        // Grant Experience - FIX: Added team check to prevent XP on friendly fire
        if (ShootingUnit && (ShootingUnit->TeamId != TeamId))
        {
            ShootingUnit->IncreaseExperience();
        }
    }

	// Apply ProjectileEffect
	if (ProjectileEffect)
	{
		ApplyInvestmentEffect(ProjectileEffect);
	}
	else if (CDO->ProjectileEffect)
	{
		ApplyInvestmentEffect(CDO->ProjectileEffect);
	}

	if (ProjectileEffect2)
	{
		ApplyInvestmentEffect(ProjectileEffect2);
	}
	else if (CDO->ProjectileEffect2)
	{
		ApplyInvestmentEffect(CDO->ProjectileEffect2);
	}

	if (ProjectileEffect3)
	{
		ApplyInvestmentEffect(ProjectileEffect3);
	}
	else if (CDO->ProjectileEffect3)
	{
		ApplyInvestmentEffect(CDO->ProjectileEffect3);
	}

	// Visuals/Sound
	if (APerformanceUnit* PerfShooter = Cast<APerformanceUnit>(ShootingUnit))
	{
		// Face towards impact
		const FVector FromLoc = ShootingUnit->GetMassActorLocation();
		const FRotator FaceRot = (ImpactLocation - FromLoc).Rotation();
		PerfShooter->FireEffectsAtLocation(CDO->ImpactVFX, CDO->ImpactSound, CDO->ScaleImpactVFX, CDO->ScaleImpactSound, ImpactLocation, 2.0f, FaceRot);
	}
}

void AUnitBase::HandleEffectAreaImpact_Implementation(float Damage, bool IsHealing, TSubclassOf<class UGameplayEffect> Effect1, TSubclassOf<class UGameplayEffect> Effect2, TSubclassOf<class UGameplayEffect> Effect3)
{
	if (!HasAuthority()) return;

	float NewDamage = Damage;

	// Apply Magic Resistance logic if it's not a healing area
	// EffectAreas are always considered Magic Damage
	if (!IsHealing && Attributes)
	{
		NewDamage = Damage - Attributes->GetMagicResistance();
	}

	NewDamage = FMath::Max(0.f, NewDamage);

	if (IsHealing)
	{
		if (Attributes)
		{
			SetHealth_Implementation(FMath::Min(Attributes->GetMaxHealth(), Attributes->GetHealth() + NewDamage));
		}
	}
	else
	{
		if (Attributes)
		{
			if (Attributes->GetShield() <= 0)
				SetHealth_Implementation(Attributes->GetHealth() - NewDamage);
			else
				SetShield_Implementation(Attributes->GetShield() - NewDamage);
		}
	}

	// Apply Gameplay Effects
	if (Effect1) ApplyInvestmentEffect(Effect1);
	if (Effect2) ApplyInvestmentEffect(Effect2);
	if (Effect3) ApplyInvestmentEffect(Effect3);
}

void AUnitBase::SpawnEffectArea(int InTeamId, FVector Location, FVector Scale, TSubclassOf<class AEffectArea> EAClass, AUnitBase* ActorToLockOn)
{
	if (!EAClass) return;
	
	FQuat VisualRotationOffset = FQuat::Identity;
	FVector SpawnLocation = Location;

	UWorld* World = GetWorld();
	if (World)
	{
		FHitResult HitResult;
		FVector TraceStart = Location + FVector(0, 0, 1000);
		FVector TraceEnd = Location - FVector(0, 0, 1000);
		FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(EffectAreaTrace), true);

		if (World->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_WorldStatic, TraceParams))
		{
			SpawnLocation.Z = HitResult.ImpactPoint.Z;
			VisualRotationOffset = AEffectArea::CalculateGroundRotationOffset(HitResult.ImpactNormal, FVector::ForwardVector);
		}
	}

	FTransform Transform;
	Transform.SetLocation(SpawnLocation);
	Transform.SetRotation(FQuat::Identity);
	Transform.SetScale3D(Scale);
		
	const auto MyEffectArea = Cast<AEffectArea>
						(UGameplayStatics::BeginDeferredActorSpawnFromClass
						(this, EAClass, Transform,  ESpawnActorCollisionHandlingMethod::AlwaysSpawn));
	
	if (MyEffectArea != nullptr)
	{
		MyEffectArea->TeamId = InTeamId;
		MyEffectArea->VisualRotationOffset = VisualRotationOffset;

		if(ActorToLockOn)
		{
			MyEffectArea->AttachToComponent(ActorToLockOn->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, FName("rootSocket"));
		}
		
		UGameplayStatics::FinishSpawningActor(MyEffectArea, Transform);
	}
}

void AUnitBase::SpawnProjectileWithEntities(AActor* Target, AActor* Attacker, FMassEntityHandle ShooterEntity, FMassEntityHandle TargetEntity)
{
    // Note: ProjectileBaseClass is available via inheritance from APerformanceUnit
    if (!ProjectileBaseClass || !Target || !Attacker) return;

    AUnitBase* ShootingUnit = Cast<AUnitBase>(Attacker);
    if (!ShootingUnit) return;

    // --- Optimization: Increment Mass Replication Counter for Clients ---
    if (RTSReplicationSettings::GetReplicationMode() == RTSReplicationSettings::Mass && HasAuthority())
    {
        const AProjectile* ProjectileCDO = ProjectileBaseClass->GetDefaultObject<AProjectile>();
        if (ProjectileCDO && ProjectileCDO->bUseMass)
        {
            // We no longer return here. The main logic below will spawn the authoritative entity on the server.
            // Note: We might want to move this increment into the loop for multi-shot, 
            // but for now we keep it here to avoid multiple increments if not needed.
            // Actually, for consistency with other functions, let's move it into the loop.
        }
    }
    // --- End Optimization ---

    const AProjectile* ProjectileCDO = ProjectileBaseClass->GetDefaultObject<AProjectile>();
    if (ProjectileCDO && ProjectileCDO->bUseMass)
    {
        float TwinDistance = ProjectileCDO->TwinProjectileDistance;
        int32 HomingCount = ProjectileCDO->HomingMissleCount;
        int32 BaseCount = (HomingCount > 0) ? HomingCount : 1;

        FVector ActualSpawnPos = GetProjectileSpawnLocation(FVector::ZeroVector);
        FVector AimLocation = Target->GetActorLocation();
        if (AUnitBase* UnitTarget = Cast<AUnitBase>(Target))
        {
            if (!UnitTarget->bUseSkeletalMovement)
                AimLocation = UnitTarget->GetMassActorLocation();
        }

        TArray<FVector> SpawnPositions;
        if (TwinDistance >= 10.f)
        {
            FVector DirToTarget = (AimLocation - ActualSpawnPos).GetSafeNormal2D();
            FVector RightVector = DirToTarget.IsNearlyZero() ? Attacker->GetActorRightVector() : FVector::CrossProduct(FVector::UpVector, DirToTarget);
            FVector RightOffset = RightVector * TwinDistance;
            SpawnPositions.Add(ActualSpawnPos - RightOffset); // Left
            SpawnPositions.Add(ActualSpawnPos + RightOffset); // Right
        }
        else
        {
            SpawnPositions.Add(ActualSpawnPos);
        }

        float SpeedFromAttributes = 0.f;
        if (Attributes)
        {
            SpeedFromAttributes = Attributes->GetProjectileSpeed();
        }

        for (const FVector& Pos : SpawnPositions)
        {
            for (int32 i = 0; i < BaseCount; ++i)
            {
                FTransform Transform;
                Transform.SetLocation(Pos);
                FVector Direction = (AimLocation - Pos).GetSafeNormal();

                if (HomingCount > 0 && BaseCount > 1)
                {
                    float Angle = (360.0f / BaseCount) * i;
                    FVector Right, Up;
                    Direction.FindBestAxisVectors(Right, Up);
                    Direction = (Direction + (Right * FMath::Cos(FMath::DegreesToRadians(Angle)) + Up * FMath::Sin(FMath::DegreesToRadians(Angle))) * 0.1f).GetSafeNormal();
                }

                FRotator InitialRotation = Direction.Rotation() + ProjectileRotationOffset;
                Transform.SetRotation(FQuat(InitialRotation));
                Transform.SetScale3D(FVector(ProjectileScale));

                float FinalSpeed = SpeedFromAttributes;

                // ROBUSTHEIT: Falls das Attribut 0 liefert, nutze den CDO-Standardwert
                if (FinalSpeed <= 0.f && ProjectileCDO)
                {
                    FinalSpeed = ProjectileCDO->MovementSpeed;
                }

                float InitialAngle = 0.f;
                float RotSpeed = 0.f;
                float MaxRadius = 0.f;
                float InterpSpeed = ProjectileCDO->HomingInterpSpeed;
                bool bFollow = ProjectileCDO->FollowTarget;

                if (HomingCount > 0)
                {
                    bFollow = true;
                    FinalSpeed += FMath::RandRange(-ProjectileCDO->HomingSpeedVariation, ProjectileCDO->HomingSpeedVariation);
                    FinalSpeed = FMath::Max(FinalSpeed, 100.f); // Mindestens 100 Einheiten/s bei Homing
                    InitialAngle = FMath::RandRange(0.f, 360.f);
                    RotSpeed = ProjectileCDO->HomingRotationSpeed * FMath::RandRange(0.9f, 1.4f);
                    if (FMath::RandBool()) RotSpeed *= -1.f;
                    MaxRadius = ProjectileCDO->HomingMaxSpiralRadius * FMath::RandRange(0.8f, 1.2f);
                }

                if (UProjectileVisualManager* VisualManager = GetWorld()->GetSubsystem<UProjectileVisualManager>())
                {
                    // Identisches Log-Format wie auf dem Client für einfachen Vergleich

                    VisualManager->SpawnMassProjectile(ProjectileBaseClass, Transform, Attacker, Target, AimLocation, ShooterEntity, TargetEntity, FinalSpeed, TeamId, bFollow, InitialAngle, RotSpeed, MaxRadius, InterpSpeed, nullptr, Transform.GetScale3D(), -1.f, ProjectileCDO->MaxPiercedTargets, false, nullptr, nullptr, nullptr, FEffectAreaInfo());
                }
            }
        }

        // Increment replication counter for clients once for the whole burst
        if (RTSReplicationSettings::GetReplicationMode() == RTSReplicationSettings::Mass && HasAuthority())
        {
            float SpeedFromAttributesInner = Attributes ? Attributes->GetProjectileSpeed() : 0.f;
            if (SpeedFromAttributesInner <= 0.f) SpeedFromAttributesInner = ProjectileCDO->MovementSpeed;

            IncrementMassProjectileFireCounter(
                ProjectileBaseClass, 
                SpeedFromAttributesInner, 
                ShooterEntity, 
                TargetEntity, 
                0.f, 
                ProjectileCDO->HomingRotationSpeed, 
                ProjectileCDO->HomingMaxSpiralRadius, 
                ProjectileCDO->HomingInterpSpeed, 
                ProjectileCDO->FollowTarget || (HomingCount > 0), 
                AimLocation, 
                ProjectileScale, 
                0.f, 
                -1.f, 
                ProjectileCDO->MaxPiercedTargets,
                (HomingCount > 0) ? HomingCount : 1, // ProjectileCount
                false, // IsBouncingNext
                false, // IsBouncingBack
                0.f,   // ZOffset
                FVector::ZeroVector, // SpawnOffset
                false, // DisableAutoZOffset
                TwinDistance,
                nullptr,
                nullptr,
                nullptr,
                FEffectAreaInfo()
            );
        }
    }
    else
    {
        SpawnProjectile(Target, Attacker);
    }
}

void AUnitBase::UpdateUnitNavigation()
{
	// Force NavMesh update immediately
	if (UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld()))
	{
		NavSys->UpdateActorInNavOctree(*this);
	}

	// Update navigation relevance immediately after navmesh update
	UpdateNavigationRelevance();
	
}

void AUnitBase::SetAbilityEnabledByKey(const FString& Key, bool bEnable)
{
	const FString NormalizedKey = NormalizeAbilityKey(Key);

	if (HasAuthority())
	{
		if (UWorld* World = GetWorld())
		{
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				ACustomControllerBase* CustomPC = Cast<ACustomControllerBase>(It->Get());
				if (!CustomPC) continue;
				if (CustomPC->SelectableTeamId == TeamId)
				{
					CustomPC->Client_ApplyOwnerAbilityKeyToggle(this, NormalizedKey, bEnable);
				}
			}
		}
	}
}



int32 AUnitBase::GetAliveUnitsInDataSet()
{
	int32 AliveCount = 0;
	// Remove invalid or dead units
	for (int32 i = SummonedUnitsDataSet.Num() - 1; i >= 0; --i)
	{
		FUnitSpawnData& Data = SummonedUnitsDataSet[i];
		if (!IsValid(Data.UnitBase) || Data.UnitBase->GetUnitState() == UnitData::Dead)
		{
			SummonedUnitsDataSet.RemoveAt(i);
		}
	}
	// Count alive
	for (const FUnitSpawnData& Data : SummonedUnitsDataSet)
	{
		if (IsValid(Data.UnitBase) && Data.UnitBase->GetUnitState() != UnitData::Dead)
		{
			++AliveCount;
		}
	}
	return AliveCount;
}

void AUnitBase::Multicast_RegisterBuildingAsObstacle_Implementation()
{
	if (IsValid(NavObstacleProxy))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World) return;

	FBox BoundsBox;

	if (BoxCollisionComponent) {
		BoundsBox = BoxCollisionComponent->Bounds.GetBox();
	}
	else if (UBoxComponent* TaggedBox = FCollisionUtils::FindTaggedBoxComponent(this))
	{
		BoundsBox = TaggedBox->Bounds.GetBox();
	}
	else
	{
		// Try to find a capsule collision component first…
		UCapsuleComponent* Capsule = FindComponentByClass<UCapsuleComponent>();

		if (Capsule)
		{
			// Pull radius & half‑height (accounting for scale)
			float Radius = Capsule->GetScaledCapsuleRadius();
			const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
			const FVector Center = Capsule->GetComponentLocation();

			if (const UMassActorBindingComponent* BindingComponent = FindComponentByClass<UMassActorBindingComponent>())
			{
				Radius += BindingComponent->AdditionalCapsuleRadius;
			}

			// Build an FBox from center±extent
			const FVector Extents = FVector(Radius, Radius, HalfHeight);
			BoundsBox = FBox(Center - Extents, Center + Extents);
		}
		else
		{
			// Fallback to component bounding box
			BoundsBox = GetComponentsBoundingBox(true);
			if (!BoundsBox.IsValid)
			{
				return;
			}
		}
	}

	// 2. Pad the bounds slightly to ensure full coverage
	// Ensure PaddedBounds remains valid even with negative padding
	FBox PaddedBounds = BoundsBox.ExpandBy(NavObstaclePadding);
	if (!PaddedBounds.IsValid)
	{
		PaddedBounds = FBox(BoundsBox.GetCenter(), BoundsBox.GetCenter());
	}
	const FVector Center = PaddedBounds.GetCenter();
	FVector Extent = PaddedBounds.GetExtent();
	
	// Ensure Extent is non-negative
	Extent.X = FMath::Max(0.0f, Extent.X);
	Extent.Y = FMath::Max(0.0f, Extent.Y);
	Extent.Z = FMath::Max(0.0f, Extent.Z);

	// Skip degenerate (empty-bounds) obstacles. When the source bounds were invalid the box
	// collapses to ~zero extent; spawning the proxy + NavModifier then only emits the
	// "Empty bounds, ignoring NavModifierComponent" warning and triggers a wasted navoctree
	// dirty/rebuild on every placement (a contributor to the placement hitch). Nothing to register.
	if (Extent.X < 1.0f && Extent.Y < 1.0f)
	{
		return;
	}

	// 3. Spawn a dedicated, lightweight actor to hold the nav modifier
	NavObstacleProxy = World->SpawnActor<AActor>();
	if (!NavObstacleProxy)
	{
		return;
	}
	
	// 4. Create and configure the Box Component for the volume
	UBoxComponent* BoxComp = NewObject<UBoxComponent>(NavObstacleProxy);
	BoxComp->SetWorldLocation(Center);
	BoxComp->SetBoxExtent(Extent, false);
	BoxComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	NavObstacleProxy->SetRootComponent(BoxComp);
	BoxComp->RegisterComponent();

	// 5. Create and configure the Nav Modifier Component
	UNavModifierComponent* ModComp = NewObject<UNavModifierComponent>(NavObstacleProxy);
	ModComp->SetAreaClass(UNavArea_Obstacle::StaticClass());
	ModComp->FailsafeExtent = Extent;
	ModComp->RegisterComponent();

	// 6. Mark the area dirty to force a navmesh rebuild
	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
	{
		NavSys->AddDirtyArea(PaddedBounds, ENavigationDirtyFlag::All);
	}
}

float AUnitBase::GetCollisionRadiusInDirection(const FVector& Direction) const
{
	UBoxComponent* TargetedBox = BoxCollisionComponent;
	if (!TargetedBox)
	{
		TargetedBox = FCollisionUtils::FindTaggedBoxComponent(this);
	}

	if (TargetedBox) {
		const FVector BoxExtent = TargetedBox->GetUnscaledBoxExtent();
		// Use component rotation in case it's different from actor rotation
		FVector LocalDir = TargetedBox->GetComponentRotation().UnrotateVector(Direction);
		LocalDir.Z = 0.f;
		if (LocalDir.IsNearlyZero()) return 0.f;
		LocalDir.Normalize();
		return 1.0f / FMath::Max(FMath::Abs(LocalDir.X) / BoxExtent.X, FMath::Abs(LocalDir.Y) / BoxExtent.Y);
	}

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		return Capsule->GetScaledCapsuleRadius();
	}
	return 0.f;
}

void AUnitBase::Multicast_UnregisterObstacle_Implementation()
{
	if (IsValid(NavObstacleProxy))
	{
		// Get the bounds *before* destroying the actor
		const FBox BoundsToDirty = NavObstacleProxy->GetComponentsBoundingBox(true);

		// Destroy our proxy actor
		NavObstacleProxy->Destroy();
		NavObstacleProxy = nullptr;

		// Mark the area dirty again so the navmesh can reclaim the space
		if (UWorld* World = GetWorld())
		{
			if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
			{
				NavSys->AddDirtyArea(BoundsToDirty, ENavigationDirtyFlag::All);
			}
		}
	}
}
