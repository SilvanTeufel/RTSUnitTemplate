// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Characters/Unit/UnitBase.h"
#include "GAS/AttributeSetBase.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "NavCollision.h"
#include "Widgets/UnitBaseHealthBar.h"
#include "Components/CapsuleComponent.h"
#include "Actors/SelectedIcon.h"
#include "Actors/Projectile.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SkeletalMeshComponent.h"
#include "Controller/AIController/BuildingControllerBase.h"
#include "Controller/PlayerController/ControllerBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameInstances/CollisionManager.h"
#include "GameModes/RTSGameModeBase.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "Navigation/CrowdAgentInterface.h"
#include "Navigation/CrowdManager.h"
#include "Net/UnrealNetwork.h"
#include "Widgets/UnitTimerWidget.h"

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
	
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 600.0f, 0.0f);
	GetCharacterMovement()->SetIsReplicated(true);
	
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
	
	SetReplicates(true);
	//GetMesh()->SetIsReplicated(true);
	bReplicates = true;

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponentBase>("AbilitySystemComp");
	
	if(ensure(AbilitySystemComponent != nullptr))
	{
		AbilitySystemComponent->SetIsReplicated(true);
		AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Full);
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
}

void AUnitBase::CreateHealthWidgetComp()
{
	// Check if the HealthWidgetComp is already created
	//if (!HealthCompCreated)
	{
		if (ControllerBase)
		{

			ARTSGameModeBase* RTSGameMode = Cast<ARTSGameModeBase>(GetWorld()->GetAuthGameMode());
			
			if (RTSGameMode && RTSGameMode->AllUnits.Num() > HideHealthBarUnitCount )
			{
				return;
			}
		}
		
		if (HealthWidgetComp && HealthBarWidgetClass)
		{
			// Attach it to the RootComponent
			HealthWidgetComp->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);

			// Optionally set the widget class, size, or any other properties
			HealthWidgetComp->SetWidgetSpace(EWidgetSpace::Screen);
			HealthWidgetComp->SetDrawSize(FVector2D(200, 200)); // Example size, adjust as needed
			HealthWidgetComp->SetDrawAtDesiredSize(true);
			// Set the widget class (if you've assigned a widget class in the editor or dynamically)
			HealthWidgetComp->SetWidgetClass(HealthBarWidgetClass);  // YourHealthBarWidgetClass should be a reference to your widget
			HealthWidgetComp->SetPivot(FVector2d(0.5, 0.5));
			// Activate or make the widget visible, if necessary
			HealthWidgetComp->SetVisibility(true);

			// Register the component so it gets updated
			HealthWidgetComp->RegisterComponent();
		


			HealthWidgetComp->SetRelativeLocation(HealthWidgetCompLocation, false, 0, ETeleportType::None);
			
			UUnitBaseHealthBar* HealthBarWidget = Cast<UUnitBaseHealthBar>(HealthWidgetComp->GetUserWidgetObject());
		
			if (HealthBarWidget) {
				HealthBarWidget->SetOwnerActor(this);
				//HealthBarWidget->SetVisibility(ESlateVisibility::Collapsed);
				HealthCompCreated = true;
			}
		}
	}
}

// Called when the game starts or when spawned
void AUnitBase::BeginPlay()
{
	Super::BeginPlay();
	
		ControllerBase = Cast<AControllerBase>(GetWorld()->GetFirstPlayerController());
		SetupTimerWidget();


		//SpawnSelectedIcon();
		GetCharacterMovement()->GravityScale = 1;
		
		if(IsFlying)
		{
			GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Flying);
			FVector UnitLocation = GetActorLocation();
			SetActorLocation(FVector(UnitLocation.X, UnitLocation.Y, FlyHeight));
		}
	
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
	}
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
	DOREPLIFETIME(AUnitBase, UnitTags);
	DOREPLIFETIME(AUnitBase, AbilitySelectionTag);
	DOREPLIFETIME(AUnitBase, TalentTag);
	DOREPLIFETIME(AUnitBase, UnitToChase);

	DOREPLIFETIME(AUnitBase, DelayDeadVFX);
	DOREPLIFETIME(AUnitBase, DelayDeadSound);


	DOREPLIFETIME(AUnitBase, CanOnlyAttackGround);
	DOREPLIFETIME(AUnitBase, CanOnlyAttackFlying);
	DOREPLIFETIME(AUnitBase, CanDetectInvisible);
	DOREPLIFETIME(AUnitBase, CanAttack);
	DOREPLIFETIME(AUnitBase, IsInvisible);
	DOREPLIFETIME(AUnitBase, bCanBeInvisible);

	DOREPLIFETIME(AUnitBase, CanMove)
	DOREPLIFETIME(AUnitBase, bIsMassUnit)
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

	if(NewHealth <= 0.f)
	{
		SetWalkSpeed(0);
		GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SetActorEnableCollision(false);
		SetDeselected();
		SetUnitState(UnitData::Dead);
		UnitControlTimer = 0.f;
	}

	HealthbarCollapseCheck(NewHealth, OldHealth);
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
                    if((OldHealth > NewHealth || NewHealth > 0.f) && (RTSGameMode && RTSGameMode->AllUnits.Num() <= StrongThis->HideHealthBarUnitCount)) // Verwende StrongThis->Member
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
             if((OldHealth > NewHealth || NewHealth > 0.f) && (RTSGameMode && RTSGameMode->AllUnits.Num() <= HideHealthBarUnitCount))
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
	const float OldShield = Attributes->GetHealth();
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
				   if (NewShield <= OldShield
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
			if(NewShield <= OldShield && (RTSGameMode && RTSGameMode->AllUnits.Num() <= HideHealthBarUnitCount))
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
			FTransform InstanceXform;
			ISMComponent->GetInstanceTransform(InstanceIndex, /*out*/ InstanceXform, /*worldSpace=*/ true );
			FVector NewLocation = InstanceXform.GetLocation();
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


		// --- ADD THIS SECTION to determine spawner's location for Aim Direction ---
		FVector ShootingUnitLocation = ShootingUnit->GetActorLocation(); // Default to actor's root location

		// If this spawning unit is not using skeletal movement, check if it's an ISM
		// and use its instance location for calculating the aim direction.
		if (!ShootingUnit->bUseSkeletalMovement) // Condition: bUseSkeletalMovement is false for 'this' spawner
		{
			if (ShootingUnit->ISMComponent && ShootingUnit->InstanceIndex >= 0)
			{
				FTransform SpawnerInstanceTransform;
				ShootingUnit->ISMComponent->GetInstanceTransform(
					ShootingUnit->InstanceIndex,
					SpawnerInstanceTransform,
					/*bWorldSpace=*/ true
				);
				ShootingUnitLocation = SpawnerInstanceTransform.GetLocation();
			}
		}
		// --- END OF ADDED SECTION ---
		
		FTransform Transform;
		Transform.SetLocation(ShootingUnitLocation + Attributes->GetProjectileScaleActorDirectionOffset()*GetActorForwardVector() + ProjectileSpawnOffset);


		// 2) Figure out the exact world‐space "aim" point
		FVector AimLocation = Target->GetActorLocation();
		if (AUnitBase* UnitTarget = Cast<AUnitBase>(Target))
		{
			if (UnitTarget->bUseSkeletalMovement)
			{
				AimLocation = UnitTarget->GetActorLocation();
			}
			else if (UnitTarget->ISMComponent)
			{
				// pull the _instance_ transform out
				FTransform InstXf;
				UnitTarget->ISMComponent->GetInstanceTransform(
					UnitTarget->InstanceIndex,
					InstXf,
					/*worldSpace=*/ true
				);
				AimLocation = InstXf.GetLocation();
			}
			
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
    float Scale
)
{
    if (!Aim || !Attacker || !ProjectileClass) return;

    AUnitBase* ShootingUnit = Cast<AUnitBase>(Attacker);
    AUnitBase* TargetUnit   = Cast<AUnitBase>(Aim);

	// --- ADD THIS SECTION to determine spawner's location for Aim Direction ---
	FVector ShootingUnitLocation = ShootingUnit->GetActorLocation(); // Default to actor's root location

	// If this spawning unit is not using skeletal movement, check if it's an ISM
	// and use its instance location for calculating the aim direction.
	if (!ShootingUnit->bUseSkeletalMovement) // Condition: bUseSkeletalMovement is false for 'this' spawner
	{
		if (ShootingUnit->ISMComponent && ShootingUnit->InstanceIndex >= 0)
		{
			FTransform SpawnerInstanceTransform;
			ShootingUnit->ISMComponent->GetInstanceTransform(
				ShootingUnit->InstanceIndex,
				SpawnerInstanceTransform,
				/*bWorldSpace=*/ true
			);
			ShootingUnitLocation = SpawnerInstanceTransform.GetLocation();
		}
	}
	// --- END OF ADDED SECTION ---

    // 1) Determine the true “center” we want to spread around
    FVector AimCenter = Aim->GetActorLocation();
    if (TargetUnit)
    {
        if (TargetUnit->bUseSkeletalMovement)
        {
            AimCenter = TargetUnit->GetActorLocation();
        }
        else if (TargetUnit->ISMComponent && TargetUnit->InstanceIndex >= 0)
        {
            FTransform InstXf;
            TargetUnit->ISMComponent->GetInstanceTransform(
                TargetUnit->InstanceIndex,
                InstXf,
                /*worldSpace=*/ true
            );
            AimCenter = InstXf.GetLocation();
        }
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
            SpawnXf.SetLocation(
            ShootingUnitLocation
                + Attributes->GetProjectileScaleActorDirectionOffset() * GetActorForwardVector()
                + ProjectileSpawnOffset
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
	FVector SpawnerLocationForAimDir = GetActorLocation(); // Default to actor's root location

	// If this spawning unit is not using skeletal movement, check if it's an ISM
	// and use its instance location for calculating the aim direction.
	if (!bUseSkeletalMovement) // Condition: bUseSkeletalMovement is false for 'this' spawner
	{
		if (ISMComponent && InstanceIndex >= 0)
		{
			FTransform SpawnerInstanceTransform;
			ISMComponent->GetInstanceTransform(
				InstanceIndex,
				SpawnerInstanceTransform,
				/*bWorldSpace=*/ true
			);
			SpawnerLocationForAimDir = SpawnerInstanceTransform.GetLocation();
		}
	}
	// --- END OF ADDED SECTION ---

    // Base spawn‐origin offset
    const FVector SpawnOrigin = GetActorLocation()
        + Attributes->GetProjectileScaleActorDirectionOffset() * GetActorForwardVector()
        + ProjectileSpawnOffset;

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
		return !IsValid(Unit) || Unit->GetUnitState() == UnitData::Dead || GetDistanceTo(Unit) > SightRadius;
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
int NewTeamId, AWaypoint* Waypoint, int UnitCount, bool SummonContinuously)
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

	GameMode->HighestSquadId++;
	for(int i = 0; i < UnitCount; i++)
	{
		FTransform UnitTransform;
	
		UnitTransform.SetLocation(FVector(Location.X+SpawnParameter.UnitOffset.X, Location.Y+SpawnParameter.UnitOffset.Y, Location.Z+SpawnParameter.UnitOffset.Z));

		
		
		const auto UnitBase = Cast<AUnitBase>
			(UGameplayStatics::BeginDeferredActorSpawnFromClass
			(this, *SpawnParameter.UnitBaseClass, UnitTransform, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn));

		//UnitBase->MassActorBindingComponent->SetupMassOnUnit();
		/*
		if(SpawnParameter.UnitControllerBaseClass)
		{
			AAIController* UnitController = GetWorld()->SpawnActor<AAIController>(SpawnParameter.UnitControllerBaseClass, FTransform());
			if(!UnitController) return;
			APawn* PawnBase = Cast<APawn>(UnitBase);
			if(PawnBase)
			{
				UnitController->Possess(PawnBase);
			}
		}*/
	
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

			UnitBase->SetReplicateMovement(true);
			UnitBase->SetReplicates(true);
			//UnitBase->GetMesh()->SetIsReplicated(true);

			// Does this have to be replicated?
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
			
			//UnitBase->MassActorBindingComponent->SetupMassOnUnit();

			UnitBase->InitializeAttributes();
			UnitBase->SquadId = GameMode->HighestSquadId;
			
			if(Waypoint)
				UnitBase->NextWaypoint = Waypoint;

			UnitBase->ScheduleDelayedNavigationUpdate();
			
			if(SummonedUnitIndexes.Num() < i+1 || SummonContinuously)
			{
				int UIndex = GameMode->AddUnitIndexAndAssignToAllUnitsArray(UnitBase);
			
				FUnitSpawnData UnitSpawnDataSet;
				UnitSpawnDataSet.Id = SpawnParameter.Id;
				UnitSpawnDataSet.UnitBase = UnitBase;
				UnitSpawnDataSet.SpawnParameter = SpawnParameter;
				SummonedUnitsDataSet.Add(UnitSpawnDataSet);
				SummonedUnitIndexes.Add(UIndex);
			}
			else if(IsSpawnedUnitDead(SummonedUnitIndexes[i]))
			{
				UnitBase->UnitIndex = SummonedUnitIndexes[i];
				SetUnitBase(SummonedUnitIndexes[i], UnitBase);
			}

			//UnitBase->MassActorBindingComponent->SetupMassOnUnit();
			//return UnitBase->UnitIndex;
		}
	}

	//return 0;
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
    if (DetectedUnit->IsInvisible && !CanDetectInvisible)
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