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
	
	SetReplicates(true);
	GetMesh()->SetIsReplicated(true);
	bReplicates = true;

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponentBase>("AbilitySystemComp");
	
	if(ensure(AbilitySystemComponent != nullptr))
	{
		AbilitySystemComponent->SetIsReplicated(true);
		AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Full);
	}
	
	Attributes = CreateDefaultSubobject<UAttributeSetBase>("Attributes");
	SelectedIconBaseClass = ASelectedIcon::StaticClass();

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
	
}

void AUnitBase::NotifyHit(class UPrimitiveComponent* MyComp, class AActor* Other, class UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
	Super::NotifyHit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);
	
	// If collisions can be processed, handle the logic
	if (bCanProcessCollision)
	{
		AUnitBase* OtherUnit = Cast<AUnitBase>(Other);
		if (OtherUnit)
		{
			//CollisionManager->RegisterCollision();
			// Handle collision with another AUnitBase
			CollisionUnit = OtherUnit;
			CollisionLocation = GetActorLocation();

			SetCollisionCooldown();
		}
	}
}


void AUnitBase::SetCollisionCooldown()
{
	// Re-enable collision processing
	bCanProcessCollision = false;
	/*
	UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(GetRootComponent());
	if (PrimitiveComponent)
	{
		PrimitiveComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	}*/
	GetWorld()->GetTimerManager().SetTimer(CollisionCooldownTimer, this, &AUnitBase::ResetCollisionCooldown, CollisionCooldown, false);
}

void AUnitBase::ResetCollisionCooldown()
{
	// Re-enable collision processing
	/*
	UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(GetRootComponent());
	if (PrimitiveComponent)
	{
		PrimitiveComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	}*/
	bCanProcessCollision = true;
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


		SpawnSelectedIcon();
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

	SetCollisionCooldown();

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

	DOREPLIFETIME(AUnitBase, EvadeDistance); // Added for Build
	DOREPLIFETIME(AUnitBase, EvadeDistanceChase); // Added for Build
	DOREPLIFETIME(AUnitBase, CastTime); // Added for Build
	DOREPLIFETIME(AUnitBase, ReduceCastTime); // Added for Build
	DOREPLIFETIME(AUnitBase, ReduceRootedTime); // Added for Build
	DOREPLIFETIME(AUnitBase, UnitTags);
	DOREPLIFETIME(AUnitBase, AbilitySelectionTag);
	DOREPLIFETIME(AUnitBase, TalentTag);
	DOREPLIFETIME(AUnitBase, UnitToChase);
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
			GetMesh()->SetMaterial(0, NewMaterial);
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
	ARTSGameModeBase* RTSGameMode = Cast<ARTSGameModeBase>(GetWorld()->GetAuthGameMode());
	if((OldHealth > NewHealth || NewHealth > 0.f) && (RTSGameMode && RTSGameMode->AllUnits.Num() <= HideHealthBarUnitCount))
	{
		OpenHealthWidget = true;
	}
	GetWorld()->GetTimerManager().SetTimer(HealthWidgetTimerHandle, this, &AUnitBase::HideHealthWidget, HealthWidgetDisplayDuration, false);
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
	ARTSGameModeBase* RTSGameMode = Cast<ARTSGameModeBase>(GetWorld()->GetAuthGameMode());
	if(NewShield <= OldShield && (RTSGameMode && RTSGameMode->AllUnits.Num() <= HideHealthBarUnitCount))
	{
		OpenHealthWidget = true;
	}

	GetWorld()->GetTimerManager().SetTimer(HealthWidgetTimerHandle, this, &AUnitBase::HideHealthWidget, HealthWidgetDisplayDuration, false);
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
	if(SelectedIcon)
		SelectedIcon->IconMesh->bHiddenInGame = false;

	Selected();
}

void AUnitBase::SetDeselected()
{
	if (SelectedIcon)
	{
		SelectedIcon->IconMesh->bHiddenInGame = true;
		SelectedIcon->ChangeMaterialColour(FVector4d(5.f, 40.f, 30.f, 0.5f));
	}

	Deselected();
}


void AUnitBase::SpawnSelectedIcon()
{
	FActorSpawnParameters SpawnParams;
	SpawnParams.bNoFail = true;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	FTransform SpellTransform;
	SpellTransform.SetLocation(FVector(500, 0, 0));
	SpellTransform.SetRotation(FQuat(FRotator::ZeroRotator));
	
	SelectedIcon = GetWorld()->SpawnActor<ASelectedIcon>(SelectedIconBaseClass, SpellTransform, SpawnParams);
	if (SelectedIcon) {
		SelectedIcon->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetIncludingScale, FName("rootSocket"));
		SelectedIcon->ChangeMaterialColour(FVector4d(5.f, 40.f, 30.f, 0.5f));
	}
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

	if(ShootingUnit)
	{
		FTransform Transform;
		Transform.SetLocation(GetActorLocation() + Attributes->GetProjectileScaleActorDirectionOffset()*GetActorForwardVector() + ProjectileSpawnOffset);

		FVector Direction = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal();
		FRotator InitialRotation = Direction.Rotation() + ProjectileRotationOffset;

		Transform.SetRotation(FQuat(InitialRotation));
		Transform.SetScale3D(ShootingUnit->ProjectileScale);
		
		const auto MyProjectile = Cast<AProjectile>
							(UGameplayStatics::BeginDeferredActorSpawnFromClass
							(this, ProjectileBaseClass, Transform,  ESpawnActorCollisionHandlingMethod::AlwaysSpawn));
		if (MyProjectile != nullptr)
		{
			//MyProjectile->TargetLocation = Target->GetActorLocation();
			MyProjectile->Init(Target, Attacker);
			
			MyProjectile->Mesh->OnComponentBeginOverlap.AddDynamic(MyProjectile, &AProjectile::OnOverlapBegin);
			
			if(!MyProjectile->IsOnViewport) MyProjectile->SetVisibility(false);
			
		
			UGameplayStatics::FinishSpawningActor(MyProjectile, Transform);
		}
	}
}

void AUnitBase::SpawnProjectileFromClass_Implementation(AActor* Aim, AActor* Attacker, TSubclassOf<class AProjectile> ProjectileClass, int MaxPiercedTargets, bool FollowTarget, int ProjectileCount, float Spread, bool IsBouncingNext, bool IsBouncingBack, bool DisableAutoZOffset, float ZOffset, float Scale) // FVector TargetLocation
{

	if(!Aim || !Attacker || !ProjectileClass)
		return;

	AUnitBase* ShootingUnit = Cast<AUnitBase>(Attacker);
	AUnitBase* TargetUnit = Cast<AUnitBase>(Aim);
	

	FVector TargetBoxSize = TargetUnit->GetComponentsBoundingBox().GetSize();
	
	for(int Count = 0; Count < ProjectileCount; Count++){
		
		int  MultiAngle = (Count == 0) ? 0 : (Count % 2 == 0 ? -1 : 1);
		FVector ShootDirection = UKismetMathLibrary::GetDirectionUnitVector(ShootingUnit->GetActorLocation(), TargetUnit->GetActorLocation());
		FVector ShootOffset = FRotator(0.f,MultiAngle*90.f,0.f).RotateVector(ShootDirection);
		
		FVector LocationToShoot = Aim->GetActorLocation()+ShootOffset*Spread;
		
		if(!DisableAutoZOffset)LocationToShoot.Z += TargetBoxSize.Z/2;
		
		LocationToShoot.Z += ZOffset;
		
		if(ShootingUnit)
		{
			FTransform Transform;
			Transform.SetLocation(GetActorLocation() + Attributes->GetProjectileScaleActorDirectionOffset()*GetActorForwardVector() + ProjectileSpawnOffset);

			FVector Direction = (LocationToShoot - GetActorLocation()).GetSafeNormal(); // Target->GetActorLocation()
			FRotator InitialRotation = Direction.Rotation() + ProjectileRotationOffset;

			Transform.SetRotation(FQuat(InitialRotation));
			Transform.SetScale3D(ShootingUnit->ProjectileScale*Scale);
			
			const auto MyProjectile = Cast<AProjectile>
								(UGameplayStatics::BeginDeferredActorSpawnFromClass
								(this, ProjectileClass, Transform,  ESpawnActorCollisionHandlingMethod::AlwaysSpawn));
			if (MyProjectile != nullptr)
			{

				MyProjectile->TargetLocation = LocationToShoot;
				MyProjectile->InitForAbility(Aim, Attacker);
				MyProjectile->Mesh->OnComponentBeginOverlap.AddDynamic(MyProjectile, &AProjectile::OnOverlapBegin);
				MyProjectile->MaxPiercedTargets = MaxPiercedTargets;
				MyProjectile->FollowTarget = FollowTarget;
				MyProjectile->IsBouncingNext = IsBouncingNext;
				MyProjectile->IsBouncingBack = IsBouncingBack;

				if(!MyProjectile->IsOnViewport) MyProjectile->SetVisibility(false);
				
				UGameplayStatics::FinishSpawningActor(MyProjectile, Transform);
			}
		}
	}
}

void AUnitBase::SpawnProjectileFromClassWithAim_Implementation(FVector Aim,
	TSubclassOf<class AProjectile> ProjectileClass, int MaxPiercedTargets, int ProjectileCount, float Spread,
	bool IsBouncingNext, bool IsBouncingBack, float ZOffset, float Scale)
{

	if( !ProjectileClass)
		return;


	for(int Count = 0; Count < ProjectileCount; Count++){
		int  MultiAngle = (Count == 0) ? 0 : (Count % 2 == 0 ? -1 : 1);
		FVector ShootDirection = UKismetMathLibrary::GetDirectionUnitVector(GetActorLocation(), Aim);
		FVector ShootOffset = FRotator(0.f,MultiAngle*90.f,0.f).RotateVector(ShootDirection);
		
		FVector LocationToShoot = Aim+ShootOffset*Spread;
		
		LocationToShoot.Z += GetActorLocation().Z;
		LocationToShoot.Z += ZOffset;
		
	
			FTransform Transform;
			Transform.SetLocation(GetActorLocation() + Attributes->GetProjectileScaleActorDirectionOffset()*GetActorForwardVector() + ProjectileSpawnOffset);


			FVector Direction = (LocationToShoot - GetActorLocation()).GetSafeNormal(); // Target->GetActorLocation()
			FRotator InitialRotation = Direction.Rotation() + ProjectileRotationOffset;

			Transform.SetRotation(FQuat(InitialRotation));
			Transform.SetScale3D(ProjectileScale*Scale);
			
			const auto MyProjectile = Cast<AProjectile>
								(UGameplayStatics::BeginDeferredActorSpawnFromClass
								(this, ProjectileClass, Transform,  ESpawnActorCollisionHandlingMethod::AlwaysSpawn));
			if (MyProjectile != nullptr)
			{

				MyProjectile->TargetLocation = LocationToShoot;
				MyProjectile->InitForLocationPosition(LocationToShoot, this);
			
				MyProjectile->Mesh->OnComponentBeginOverlap.AddDynamic(MyProjectile, &AProjectile::OnOverlapBegin);
				MyProjectile->MaxPiercedTargets = MaxPiercedTargets;
				MyProjectile->IsBouncingNext = IsBouncingNext;
				MyProjectile->IsBouncingBack = IsBouncingBack;

				if(!MyProjectile->IsOnViewport) MyProjectile->SetVisibility(false);
				UGameplayStatics::FinishSpawningActor(MyProjectile, Transform);
			}
		
	}
}

bool AUnitBase::SetNextUnitToChase()
{
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

	// Remove dead units in-place using the RemoveAll function which is more efficient.
	UnitsToChase.RemoveAll([](const AUnitBase* Unit) -> bool {
		return !Unit || Unit->GetUnitState() == UnitData::Dead;
	});

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

	
		if(SpawnParameter.UnitControllerBaseClass)
		{
			AAIController* UnitController = GetWorld()->SpawnActor<AAIController>(SpawnParameter.UnitControllerBaseClass, FTransform());
			if(!UnitController) return;
			APawn* PawnBase = Cast<APawn>(UnitBase);
			if(PawnBase)
			{
				UnitController->Possess(PawnBase);
			}
		}
	
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
			SetReplicates(true);
			UnitBase->GetMesh()->SetIsReplicated(true);

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


	/*
	// Clear any existing timer first
	GetWorld()->GetTimerManager().ClearTimer(NavigationUpdateTimer);

	// Schedule the update to happen once after 1 second
	GetWorld()->GetTimerManager().SetTimer(
		NavigationUpdateTimer,
		this,
		&AUnitBase::UpdateUnitNavigation,
		1.0f,    // 1-second delay
		false     // Do NOT loop
	);*/

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
