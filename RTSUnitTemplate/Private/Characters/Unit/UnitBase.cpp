// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Characters/Unit/UnitBase.h"
#include "GAS/AttributeSetBase.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "NavCollision.h"
#include "Widgets/UnitBaseHealthBar.h"
#include "Components/CapsuleComponent.h"
#include "Actors/Projectile.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SkeletalMeshComponent.h"
#include "Controller/AIController/BuildingControllerBase.h"
#include "Controller/PlayerController/ControllerBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameModes/RTSGameModeBase.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "Navigation/CrowdAgentInterface.h"
#include "Navigation/CrowdManager.h"
#include "Net/UnrealNetwork.h"
#include "Widgets/UnitTimerWidget.h"
#include "Mass/Replication/UnitRegistryReplicator.h"
#include "Widgets/SquadHealthBar.h"
#include "EngineUtils.h"
#include <climits>

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
	GetCharacterMovement()->SetIsReplicated(false);
	/*
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 600.0f, 0.0f);
	GetCharacterMovement()->SetIsReplicated(true);
	*/
	if (RootComponent == nullptr) {
		RootComponent = ObjectInitializer.CreateDefaultSubobject<USceneComponent>(this, TEXT("Root"));
	}
	
	HealthWidgetComp = ObjectInitializer.CreateDefaultSubobject<UWidgetComponent>(this, TEXT("Healthbar"));
	HealthWidgetComp->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	HealthWidgetComp->SetVisibility(true);


	Niagara_A = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Niagara"));
	Niagara_A->SetupAttachment(RootComponent);
	
	Niagara_B = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Niagara_B"));
	Niagara_B->SetupAttachment(RootComponent);

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

	TimerWidgetComp = ObjectInitializer.CreateDefaultSubobject<UWidgetComponent>(this, TEXT("Timer"));
	TimerWidgetComp->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);

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
	
	GetCapsuleComponent()->SetIsReplicated(false);
	GetMesh()->SetIsReplicated(false);
	bReplicates = true;
	SetReplicates(true);
	SetNetUpdateFrequency(2);
	SetMinNetUpdateFrequency(1);
	GetCharacterMovement()->SetIsReplicated(false);
	GetCapsuleComponent()->SetIsReplicated(false);
}

// Called when the game starts or when spawned
void AUnitBase::BeginPlay()
{
	Super::BeginPlay();
	
	ControllerBase = Cast<AControllerBase>(GetWorld()->GetFirstPlayerController());
	SetupTimerWidget();
	
	SetReplicateMovement(false);
	/*
		//SpawnSelectedIcon();
		GetCharacterMovement()->GravityScale = 1;
		
		if(IsFlying)
		{
			GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Flying);
			FVector UnitLocation = GetActorLocation();
			SetActorLocation(FVector(UnitLocation.X, UnitLocation.Y, FlyHeight));
		}
	*/
	if (HasAuthority())
	{
		SetMeshRotationServer();
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
		HealthBarUpdateTriggered = false;
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
					HealthBarUpdateTriggered = true;
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
		HealthBarUpdateTriggered = false;
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
	
	DOREPLIFETIME(AUnitBase, ToggleUnitDetection);
	DOREPLIFETIME(AUnitBase, RunLocation);
	DOREPLIFETIME(AUnitBase, MeshAssetPath);
	DOREPLIFETIME(AUnitBase, MeshMaterialPath);
	DOREPLIFETIME(AUnitBase, UnitControlTimer);
	DOREPLIFETIME(AUnitBase, LineTraceZDistance);
	DOREPLIFETIME(AUnitBase, CanActivateAbilities);

	DOREPLIFETIME(AUnitBase, EvadeDistance); // Added for Build
	DOREPLIFETIME(AUnitBase, EvadeDistanceChase); // Added for Build
	DOREPLIFETIME(AUnitBase, CastTime); // Added for Build
	DOREPLIFETIME(AUnitBase, ReduceCastTime); // Added for Build
	DOREPLIFETIME(AUnitBase, ReduceRootedTime); // Added for Build
	DOREPLIFETIME(AUnitBase, UnitToChase);

	DOREPLIFETIME(AUnitBase, DelayDeadVFX);
	DOREPLIFETIME(AUnitBase, DelayDeadSound);

	DOREPLIFETIME(AUnitBase, CanMove);
	DOREPLIFETIME(AUnitBase, CanOnlyAttackGround);
	DOREPLIFETIME(AUnitBase, CanOnlyAttackFlying);
	DOREPLIFETIME(AUnitBase, CanDetectInvisible);
	DOREPLIFETIME(AUnitBase, CanAttack);
	DOREPLIFETIME(AUnitBase, bIsInvisible);
	DOREPLIFETIME(AUnitBase, bCanBeInvisible);
	DOREPLIFETIME(AUnitBase, bHoldPosition);

	DOREPLIFETIME(AUnitBase, bIsMassUnit);
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
	NextWaypoint = NewNextWaypoint;
}

void AUnitBase::SetHealth_Implementation(float NewHealth)
{
	float OldHealth = Attributes->GetHealth();
	
	Attributes->SetAttributeHealth(NewHealth);
	UpdateEntityHealth(NewHealth);
	if(NewHealth <= 0.f)
	{
		SetWalkSpeed(0);
		GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SetActorEnableCollision(false);
		SetDeselected();
		SetUnitState(UnitData::Dead);

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

	HealthbarCollapseCheck(NewHealth, OldHealth);
}

void AUnitBase::DeadMultiCast_Implementation()
{
	SetUnitState(UnitData::Dead);
	SwitchEntityTagByState(UnitData::Dead, UnitData::Dead);
	FireEffects_Implementation(DeadVFX, DeadSound, ScaleDeadVFX, ScaleDeadSound, DelayDeadVFX, DelayDeadSound);
}
void AUnitBase::Multicast_SwitchToIdle_Implementation()
{
	SwitchEntityTagByState(UnitData::Idle, UnitData::Idle);
}

void AUnitBase::DeadEffectsAndEvents()
{
	if (!DeadEffectsExecuted)
	{
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


void AUnitBase::HealthbarCollapseCheck(float NewHealth, float OldHealth)
{
	
	 // Capture necessary data for the lambda
    TWeakObjectPtr<AUnitBase> WeakThis(this); // Sicherer Zeiger auf 'this'
    float LocalHealthWidgetDisplayDuration = HealthWidgetDisplayDuration; // Kopiere relevante Daten
    // Kopiere auch NewHealth, OldHealth etc., falls im GameThread-Teil benötigt

    if (!IsInGameThread())
    {
        // Führe den kritischen Teil auf dem Game Thread aus
        AsyncTask(ENamedThreads::GameThread, [WeakThis, LocalHealthWidgetDisplayDuration, NewHealth, OldHealth]()
        {
            // Prüfe, ob das Objekt noch existiert, wenn der Task ausgeführt wird
            if (AUnitBase* StrongThis = WeakThis.Get())
            {
                UWorld* World = StrongThis->GetWorld();
                if (World)
                {
                    // Führe hier die Logik aus, die den GameThread benötigt
                    ARTSGameModeBase* RTSGameMode = Cast<ARTSGameModeBase>(World->GetAuthGameMode());
                    if((OldHealth != NewHealth && NewHealth >= 0.f) && (RTSGameMode && RTSGameMode->AllUnits.Num() <= StrongThis->HideHealthBarUnitCount)) // Verwende StrongThis->Member
                    {
                       StrongThis->OpenHealthWidget = true;
                    }

                    World->GetTimerManager().SetTimer(
                        StrongThis->HealthWidgetTimerHandle, // Verwende StrongThis->Member
                        StrongThis,
                        &AUnitBase::HideHealthWidget,
                        LocalHealthWidgetDisplayDuration,
                        false);
                }
            }
            // else: Das Objekt wurde zerstört, bevor der Task lief. Tu nichts.
        });
    }
    else
    {
        // Wir sind bereits auf dem Game Thread, alles sicher
        UWorld* World = GetWorld();
        if (World && IsValid(this)) // Sicherheitschecks beibehalten
        {
        	ARTSGameModeBase* RTSGameMode = Cast<ARTSGameModeBase>(World->GetAuthGameMode());
        	if((OldHealth != NewHealth && NewHealth >= 0.f) && (RTSGameMode && RTSGameMode->AllUnits.Num() <= HideHealthBarUnitCount))
        	{
                OpenHealthWidget = true;
        	}
            World->GetTimerManager().SetTimer(HealthWidgetTimerHandle, this, &AUnitBase::HideHealthWidget, HealthWidgetDisplayDuration, false);
        }
    }

}

void AUnitBase::HideHealthWidget()
{
	OpenHealthWidget = false;
}


void AUnitBase::SetShield_Implementation(float NewShield)
{
	const float OldShield = Attributes->GetShield();
	Attributes->SetAttributeShield(NewShield);
	ShieldCollapseCheck(NewShield, OldShield);
}

void AUnitBase::ShieldCollapseCheck(float NewShield, float OldShield)
{
	// Capture necessary data for the lambda
	TWeakObjectPtr<AUnitBase> WeakThis(this); // Sicherer Zeiger auf 'this'
	float LocalHealthWidgetDisplayDuration = HealthWidgetDisplayDuration; // Kopiere relevante Daten
	// Kopiere auch NewHealth, OldHealth etc., falls im GameThread-Teil benötigt

	if (!IsInGameThread())
	{
		// Führe den kritischen Teil auf dem Game Thread aus
		AsyncTask(ENamedThreads::GameThread, [WeakThis, LocalHealthWidgetDisplayDuration, NewShield, OldShield]()
		{
			// Prüfe, ob das Objekt noch existiert, wenn der Task ausgeführt wird
			if (AUnitBase* StrongThis = WeakThis.Get())
			{
				UWorld* World = StrongThis->GetWorld();
			   if (World)
			   {
				   // AuthGameMode holen
				   ARTSGameModeBase* RTSGameMode = Cast<ARTSGameModeBase>(World->GetAuthGameMode());

		

				   // Shield‑Collapse Check
				   if (NewShield != OldShield
					   && RTSGameMode
					   && RTSGameMode->AllUnits.Num() <= StrongThis->HideHealthBarUnitCount)
				   {
					   StrongThis->OpenHealthWidget = true;
				   }

				   // Timer setzen, um das Widget später zu verstecken
				   World->GetTimerManager().SetTimer(
					   StrongThis->HealthWidgetTimerHandle,
					   StrongThis,
					   &AUnitBase::HideHealthWidget,
					   LocalHealthWidgetDisplayDuration,
					   false
				   );
			   }
			}
			// else: Das Objekt wurde zerstört, bevor der Task lief. Tu nichts.
		});
	}
	else
	{
		// Wir sind bereits auf dem Game Thread, alles sicher
		UWorld* World = GetWorld();
		if (World && IsValid(this)) // Sicherheitschecks beibehalten
		{
			ARTSGameModeBase* RTSGameMode = Cast<ARTSGameModeBase>(World->GetAuthGameMode());
			if (NewShield != OldShield
					  && RTSGameMode
					  && RTSGameMode->AllUnits.Num() <= HideHealthBarUnitCount)
			{
				OpenHealthWidget = true;
			}

			World->GetTimerManager().SetTimer(HealthWidgetTimerHandle, this, &AUnitBase::HideHealthWidget, HealthWidgetDisplayDuration, false);
		}
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

void AUnitBase::IncreaseExperience()
{
	LevelData.Experience++;
		
	UpdateWidget();
}

void AUnitBase::SetSelected()
{
	
	if (SelectionIcon)
	{
		if (!bUseSkeletalMovement)
		{
			FVector NewLocation = GetMassActorLocation();
			if (IsFlying)
				NewLocation.Z = NewLocation.Z-FlyHeight;
			
			SelectionIcon->SetWorldLocation(NewLocation);
		}

		SelectionIcon->ShowSelection();
	}

	Selected();
}

void AUnitBase::SetDeselected()
{
	if (SelectionIcon)
		SelectionIcon->HideSelection();
		
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


void AUnitBase::SpawnProjectile_Implementation(AActor* Target, AActor* Attacker) // FVector TargetLocation
{
	AUnitBase* ShootingUnit = Cast<AUnitBase>(Attacker);

	if (!ProjectileBaseClass) return;

	
	
	if(ShootingUnit)
	{



		FVector ShootingUnitLocation = ShootingUnit->GetMassActorLocation(); 
		
		FTransform Transform;
		const FVector RotatedProjectileSpawnOffset = ShootingUnit->GetActorRotation().RotateVector(ProjectileSpawnOffset);
		Transform.SetLocation(ShootingUnitLocation + Attributes->GetProjectileScaleActorDirectionOffset()*ShootingUnit->GetActorForwardVector() + RotatedProjectileSpawnOffset);


		// 2) Figure out the exact world‐space "aim" point
		FVector AimLocation = Target->GetActorLocation();
		if (AUnitBase* UnitTarget = Cast<AUnitBase>(Target))
		{
			if (!UnitTarget->bUseSkeletalMovement)
				AimLocation = UnitTarget->GetMassActorLocation(); 
		}
		
		FVector Direction = (AimLocation - ShootingUnitLocation).GetSafeNormal();
		FRotator InitialRotation = Direction.Rotation() + ProjectileRotationOffset;

		Transform.SetRotation(FQuat(InitialRotation));
		Transform.SetScale3D(ShootingUnit->ProjectileScale);

		
		const auto MyProjectile = Cast<AProjectile>
							(UGameplayStatics::BeginDeferredActorSpawnFromClass
							(this, ProjectileBaseClass, Transform,  ESpawnActorCollisionHandlingMethod::AlwaysSpawn));
		if (MyProjectile != nullptr)
		{
			MyProjectile->Init(Target, Attacker);
			MyProjectile->SetProjectileVisibility();
			UGameplayStatics::FinishSpawningActor(MyProjectile, Transform);
			MyProjectile->SetReplicates(true);
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
    AUnitBase* TargetUnit   = Cast<AUnitBase>(Aim);

	// --- ADD THIS SECTION to determine spawner's location for Aim Direction ---
	FVector ShootingUnitLocation = ShootingUnit->GetMassActorLocation();  // Default to actor's root location

    // 1) Determine the true “center” we want to spread around
    FVector AimCenter = Aim->GetActorLocation();
    if (TargetUnit && !TargetUnit->bUseSkeletalMovement)
    {
    	AimCenter = TargetUnit->GetMassActorLocation(); 
    }

    // 2) Compute vertical half-height for auto Z-offset
    FVector TargetBoxSize = Aim->GetComponentsBoundingBox().GetSize();
    const float HalfHeight = DisableAutoZOffset ? 0.f : TargetBoxSize.Z * 0.5f;

    for (int32 Count = 0; Count < ProjectileCount; ++Count)
    {
        // alternate left/right spread
        int32  MultiAngle     = (Count == 0) ? 0 : ((Count & 1) ? 1 : -1);
        FVector ToCenterDir   = (AimCenter - ShootingUnitLocation).GetSafeNormal();
        FVector PerpOffsetDir = FRotator(0.f, MultiAngle * 90.f, 0.f).RotateVector(ToCenterDir);
        FVector SpreadOffset  = PerpOffsetDir * Spread;

        // final aim point for this shot
        FVector LocationToShoot = AimCenter + SpreadOffset;
        LocationToShoot.Z     += HalfHeight + ZOffset;

        // 3) Build spawn transform
        if (ShootingUnit)
        {
            FTransform SpawnXf;
            const FVector RotatedProjectileSpawnOffset = ShootingUnit->GetActorRotation().RotateVector(ProjectileSpawnOffset);
            SpawnXf.SetLocation(
            ShootingUnitLocation
                + Attributes->GetProjectileScaleActorDirectionOffset() * ShootingUnit->GetActorForwardVector()
                + RotatedProjectileSpawnOffset + SpawnOffset
            );

            const FVector Dir          = (LocationToShoot - ShootingUnitLocation).GetSafeNormal();
            const FRotator InitialRot  = Dir.Rotation() + ProjectileRotationOffset;
            SpawnXf.SetRotation(FQuat(InitialRot));
            SpawnXf.SetScale3D(ShootingUnit->ProjectileScale * Scale);

            // 4) Deferred spawn + init
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
                MyProj->InitForAbility(Aim, Attacker);

                //MyProj->Mesh_A->OnComponentBeginOverlap.AddDynamic(MyProj, &AProjectile::OnOverlapBegin);
                MyProj->MaxPiercedTargets = MaxPiercedTargets;
                MyProj->FollowTarget      = FollowTarget;
                MyProj->IsBouncingNext    = IsBouncingNext;
                MyProj->IsBouncingBack    = IsBouncingBack;

                MyProj->SetProjectileVisibility();
                UGameplayStatics::FinishSpawningActor(MyProj, SpawnXf);
                MyProj->SetReplicates(true);
            }
        }
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
    float Scale
)
{
    if (!ProjectileClass)
        return;
	
	// --- ADD THIS SECTION to determine spawner's location for Aim Direction ---
	FVector SpawnerLocationForAimDir = GetMassActorLocation(); // Default to actor's root location

    // Base spawn‐origin offset
    const FVector RotatedProjectileSpawnOffset = GetActorRotation().RotateVector(ProjectileSpawnOffset);
    const FVector SpawnOrigin = SpawnerLocationForAimDir
        + Attributes->GetProjectileScaleActorDirectionOffset() * GetActorForwardVector()
        + RotatedProjectileSpawnOffset;

    for (int32 i = 0; i < ProjectileCount; ++i)
    {
        // Alternate left/right offsets for multi‐shot spread
        const int   MultiAngle    = (i == 0) ? 0 : ((i & 1) ? 1 : -1);
        const FVector ToAimDir     = (Aim - SpawnerLocationForAimDir).GetSafeNormal();
        const FVector SpreadOffset = FRotator(0.f, MultiAngle * 90.f, 0.f)
                                     .RotateVector(ToAimDir)
                                     * Spread;

        // Compute this shot’s exact world target point
        FVector LocationToShoot = Aim + SpreadOffset;
        // Overwrite Z so we use the Aim.Z (instead of adding our own Z)
        LocationToShoot.Z = Aim.Z + ZOffset;

        // Build the spawn transform
        FTransform SpawnXf;
        SpawnXf.SetLocation(SpawnOrigin);

        const FVector Dir         = (LocationToShoot - SpawnOrigin).GetSafeNormal();
        const FRotator InitialRot = Dir.Rotation() + ProjectileRotationOffset;
        SpawnXf.SetRotation(FQuat(InitialRot));
        SpawnXf.SetScale3D(ProjectileScale * Scale);

        // Spawn deferred so we can Init
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
            Proj->InitForLocationPosition(LocationToShoot, this);

            //Proj->Mesh_A->OnComponentBeginOverlap.AddDynamic(Proj, &AProjectile::OnOverlapBegin);
            Proj->MaxPiercedTargets = MaxPiercedTargets;
            Proj->IsBouncingNext    = IsBouncingNext;
            Proj->IsBouncingBack    = IsBouncingBack;

            Proj->SetProjectileVisibility();
            UGameplayStatics::FinishSpawningActor(Proj, SpawnXf);
            Proj->SetReplicates(true);
        }
    }
}

bool AUnitBase::SetNextUnitToChase()
{
	// Entferne alle Einheiten, die ungültig, tot oder außerhalb der Sichtweite sind.
	UnitsToChase.RemoveAll([this](const AUnitBase* Unit) -> bool {
		return !IsValid(Unit) || Unit->GetUnitState() == UnitData::Dead || GetDistanceTo(Unit) > MassActorBindingComponent->SightRadius;
	});
	
	if (UnitsToChase.IsEmpty()) return false;
    
	float ShortestDistance = TNumericLimits<float>::Max();
	AUnitBase* ClosestUnit = nullptr;

	for (auto& Unit : UnitsToChase)
	{
		if (Unit && Unit->GetUnitState() != UnitData::Dead)
		{
			float Distance = GetDistanceTo(Unit);
			if (Distance < ShortestDistance)
			{
				ShortestDistance = Distance;
				ClosestUnit = Unit;
			}
		}
	}

	// Set the closest living unit as the target, if any.
	if (ClosestUnit)
	{
		UnitToChase = ClosestUnit;
		return true;
	}

	return false;
}


void AUnitBase::SpawnUnitsFromParameters(
TSubclassOf<class AAIController> AIControllerBaseClass,
TSubclassOf<class AUnitBase> UnitBaseClass, UMaterialInstance* Material, USkeletalMesh* CharacterMesh, FRotator HostMeshRotation, FVector Location,
TEnumAsByte<UnitData::EState> UState,
TEnumAsByte<UnitData::EState> UStatePlaceholder,
int NewTeamId, AWaypoint* Waypoint, int UnitCount, bool SummonContinuously, bool SpawnAsSquad, bool UseSummonDataSet)
{
	FUnitSpawnParameter SpawnParameter;
	SpawnParameter.UnitControllerBaseClass = AIControllerBaseClass;
	SpawnParameter.UnitBaseClass = UnitBaseClass;
	SpawnParameter.UnitOffset = FVector3d(0.f,0.f,0.f);
	SpawnParameter.ServerMeshRotation = HostMeshRotation;
	SpawnParameter.State = UState;
	SpawnParameter.StatePlaceholder = UStatePlaceholder;
	SpawnParameter.Material = Material;
	SpawnParameter.CharacterMesh = CharacterMesh;
	// Waypointspawn

	if (!SpawnParameter.UnitBaseClass) return;
	
	ARTSGameModeBase* GameMode = Cast<ARTSGameModeBase>(GetWorld()->GetAuthGameMode());

	if(!GameMode) return;

	int32 SharedSquadId = 0;
	if (SpawnAsSquad)
	{
		GameMode->HighestSquadId++;
		SharedSquadId = GameMode->HighestSquadId;
	}
	for(int i = 0; i < UnitCount; i++)
	{
		FTransform UnitTransform;
	
		UnitTransform.SetLocation(FVector(Location.X+SpawnParameter.UnitOffset.X, Location.Y+SpawnParameter.UnitOffset.Y, Location.Z+SpawnParameter.UnitOffset.Z));
		
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

			UnitBase->ServerMeshRotation = SpawnParameter.ServerMeshRotation;
			
			UnitBase->OnRep_MeshAssetPath();
			UnitBase->OnRep_MeshMaterialPath();
			
			UnitBase->SetMeshRotationServer();
		
			UnitBase->UnitState = SpawnParameter.State;
			UnitBase->UnitStatePlaceholder = SpawnParameter.StatePlaceholder;
			
			if(UnitToChase)
			{
				UnitBase->UnitToChase = UnitToChase;
				UnitBase->SetUnitState(UnitData::Chase);
			}
		
			UGameplayStatics::FinishSpawningActor(
			 Cast<AActor>(UnitBase), 
			 UnitTransform
			);

			UnitBase->InitializeAttributes();
			UnitBase->SquadId = (SpawnAsSquad ? SharedSquadId : 0);
			// Ensure squad healthbar setup on server right after SquadId assignment
			UnitBase->EnsureSquadHealthbarState();
			
			if(Waypoint)
				UnitBase->NextWaypoint = Waypoint;

			UnitBase->ScheduleDelayedNavigationUpdate();
			
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
			
		}
	}
	
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


void AUnitBase::AddUnitToChase_Implementation(AActor* OtherActor)
{
    
    if (!OtherActor || !OtherActor->IsValidLowLevel() || !IsValid(OtherActor))
    {
        return;
    }

    // Cast the incoming actor to AUnitBase (the detected unit)
    AUnitBase* DetectedUnit = Cast<AUnitBase>(OtherActor);
    if (!DetectedUnit)
    {
        return;
    }


    
    // Only add the unit if it is alive.
    if (DetectedUnit->GetUnitState() == UnitData::Dead || GetUnitState() == UnitData::Dead)
    {
        return;
    }

    // Invisible detection:
    if (DetectedUnit->bIsInvisible && !CanDetectInvisible)
    {
        return;
    }

    // Ground/Flying detection restrictions:
    if (CanOnlyAttackGround && DetectedUnit->IsFlying)
    {
        return;
    }
    if (CanOnlyAttackFlying && !DetectedUnit->IsFlying)
    {
        return;
    }
    
    // Now check team relationships:
    // If detecting friendly units, only add friendly ones.
    // Otherwise, only add enemy units.
    bool isFriendlyUnit = DetectedUnit->TeamId == TeamId;
    if ((DetectFriendlyUnits && isFriendlyUnit) || (!DetectFriendlyUnits && !isFriendlyUnit))
    {
        // Add the detected unit to the UnitsToChase array
        UnitsToChase.Emplace(DetectedUnit);
    }
    
    // Retrieve the detection toggle state
    bool SetState = GetToggleUnitDetection();

    if (!SetState)
    {
        return;
    }
    
    // Use UnitController's state logic:
    // Attempt to set the next unit to chase.
    if (SetNextUnitToChase())
    {
        bool isUnitChasing = GetUnitState() == UnitData::Chase;
        bool canChangeState = !isUnitChasing &&
                             GetUnitState() != UnitData::Attack && GetUnitState() != UnitData::Pause &&
                             	GetUnitState() != UnitData::Casting;

        if (UnitToChase) // Ensure UnitToChase is valid
        {
            float DistanceToTarget = FVector::Dist(GetActorLocation(), UnitToChase->GetActorLocation());
            float AttackRange =Attributes->GetRange(); // Assuming this function exists

            // For friendly units, only chase if the current target's health is below max.
            bool shouldChase = canChangeState &&
                               UnitToChase &&
                               UnitToChase->Attributes->GetHealth() < UnitToChase->Attributes->GetMaxHealth();
            
            // Adjust the state based on whether we're detecting friendly units
            if (DistanceToTarget <= AttackRange)
            {
                // If the target is within attack range, set state to Attack
                if (shouldChase) SetUnitState(UnitData::Attack);
            }
            else if (DetectFriendlyUnits)
            {
                // For friendly units, only chase if the current target's health is below max.
                if (shouldChase) SetUnitState(UnitData::Chase);
                
            }
            else if (canChangeState)
            {
                SetUnitState(UnitData::Chase);
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
