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
#include "GameFramework/CharacterMovementComponent.h"
#include "GameModes/RTSGameModeBase.h"
#include "Net/UnrealNetwork.h"
#include "Widgets/UnitTimerWidget.h"

// Sets default values
AUnitBase::AUnitBase(const FObjectInitializer& ObjectInitializer):Super(ObjectInitializer)
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
	
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 600.0f, 0.0f);

	if (RootComponent == nullptr) {
		RootComponent = ObjectInitializer.CreateDefaultSubobject<USceneComponent>(this, TEXT("Root"));
	}
	
	HealthWidgetComp = ObjectInitializer.CreateDefaultSubobject<UWidgetComponent>(this, TEXT("Healthbar"));
	HealthWidgetComp->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);

	SetReplicates(true);
	GetMesh()->SetIsReplicated(true);

	bReplicates = true;
	
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponentBase>("AbilitySystemComp");
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Full);
	
	Attributes = CreateDefaultSubobject<UAttributeSetBase>("Attributes");
	SelectedIconBaseClass = ASelectedIcon::StaticClass();

	TimerWidgetComp = ObjectInitializer.CreateDefaultSubobject<UWidgetComponent>(this, TEXT("Timer"));
	TimerWidgetComp->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	/*
	if (HasAuthority())
	{
		bReplicates = true;
		SetReplicateMovement(true);
		GetMesh()->SetIsReplicated(true);
	}
	*/
}

void AUnitBase::NotifyHit(class UPrimitiveComponent* MyComp, class AActor* Other, class UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
	Super::NotifyHit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);
    
	AUnitBase* OtherUnit = Cast<AUnitBase>(Other);
	if (OtherUnit)
	{
		// Handle collision with another AUnitBase
		CollisionUnit = OtherUnit;
	}
}

// Called when the game starts or when spawned
void AUnitBase::BeginPlay()
{
	Super::BeginPlay();
	
		if (HealthWidgetComp) {

			HealthWidgetComp->SetRelativeLocation(HealthWidgetCompLocation, false, 0, ETeleportType::None);
			UUnitBaseHealthBar* Healthbar = Cast<UUnitBaseHealthBar>(HealthWidgetComp->GetUserWidgetObject());

			if (Healthbar) {
				Healthbar->SetOwnerActor(this);
			}
		}

		SetupTimerWidget();

		/*
		FRotator NewRotation = FRotator(0, -90, 0);
		FQuat QuatRotation = FQuat(NewRotation);
		
		GetMesh()->SetRelativeRotation(QuatRotation, false, 0, ETeleportType::None);
		GetMesh()->SetRelativeLocation(FVector(0, 0, -75), false, 0, ETeleportType::None);
		*/

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

}

void AUnitBase::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(AUnitBase, HealthWidgetComp);
	DOREPLIFETIME(AUnitBase, ToggleUnitDetection);
	DOREPLIFETIME(AUnitBase, RunLocation);
	DOREPLIFETIME(AUnitBase, MeshAssetPath);
	DOREPLIFETIME(AUnitBase, MeshMaterialPath);
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
	Attributes->SetAttributeHealth(NewHealth);

	if(NewHealth <= 0.f)
	{
		//if(UnitIndex > 0)
			//SaveLevelDataAndAttributes(FString::FromInt(UnitIndex));
		
		SetWalkSpeed(0);
		GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SetActorEnableCollision(false);
		SetDeselected();
		SetUnitState(UnitData::Dead);
		UnitControlTimer = 0.f;
	}
}

void AUnitBase::SetSelected()
{
	if(SelectedIcon)
		SelectedIcon->IconMesh->bHiddenInGame = false;

}

void AUnitBase::SetDeselected()
{
	if (SelectedIcon)
	{
		SelectedIcon->IconMesh->bHiddenInGame = true;
		SelectedIcon->ChangeMaterialColour(FVector4d(5.f, 40.f, 30.f, 0.5f));
	}
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

		TimerWidgetComp->SetRelativeLocation(TimerWidgetCompLocation, false, 0, ETeleportType::None);
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
			
				UGameplayStatics::FinishSpawningActor(MyProjectile, Transform);
			}
		}
	}
}

void AUnitBase::SetToggleUnitDetection_Implementation(bool ToggleTo)
{
		ToggleUnitDetection = ToggleTo;
}

bool AUnitBase::GetToggleUnitDetection()
{
	return ToggleUnitDetection;
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


int AUnitBase::SpawnUnitsFromParameters(
TSubclassOf<class AAIController> AIControllerBaseClass,
TSubclassOf<class AUnitBase> UnitBaseClass, UMaterialInstance* Material, USkeletalMesh* CharacterMesh, FRotator HostMeshRotation, FVector Location,
TEnumAsByte<UnitData::EState> UState,
TEnumAsByte<UnitData::EState> UStatePlaceholder,
int NewTeamId, AWaypoint* Waypoint, int UIndex)
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

	if(!GameMode) return 0;
	
	FTransform UnitTransform;
	
	UnitTransform.SetLocation(FVector(Location.X+SpawnParameter.UnitOffset.X, Location.Y+SpawnParameter.UnitOffset.Y, Location.Z+SpawnParameter.UnitOffset.Z));
		
		
	const auto UnitBase = Cast<AUnitBase>
		(UGameplayStatics::BeginDeferredActorSpawnFromClass
		(this, *SpawnParameter.UnitBaseClass, UnitTransform, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn));

	
	if(SpawnParameter.UnitControllerBaseClass)
	{
		AAIController* ControllerBase = GetWorld()->SpawnActor<AAIController>(SpawnParameter.UnitControllerBaseClass, FTransform());
		if(!ControllerBase) return 0;
		APawn* PawnBase = Cast<APawn>(UnitBase);
		if(PawnBase)
		{
			ControllerBase->Possess(PawnBase);
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

		if(Waypoint)
		UnitBase->NextWaypoint = Waypoint;
		
		if(UIndex == 0)
		{
			GameMode->AddUnitIndexAndAssignToAllUnitsArray(UnitBase);
			
			FUnitSpawnData UnitSpawnDataSet;
			UnitSpawnDataSet.Id = SpawnParameter.Id;
			UnitSpawnDataSet.UnitBase = UnitBase;
			UnitSpawnDataSet.SpawnParameter = SpawnParameter;
			SummonedUnitsDataSet.Add(UnitSpawnDataSet);
		}
		else
		{
			UnitBase->UnitIndex = UIndex;
			SetUnitBase(UIndex, UnitBase);
		}

		return UnitBase->UnitIndex;
	}

	return 0;
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


/*
bool AUnitBase::SetNextUnitToChase()
{
	if(!UnitsToChase.Num()) return false;
		
	float ShortestDistance = GetDistanceTo(UnitsToChase[0]);
	int IndexShortestDistance = 0;
	for(int i = 0; i < UnitsToChase.Num(); i++)
	{
		if(UnitsToChase[i] && UnitsToChase[i]->GetUnitState() != UnitData::Dead)
		{
			DistanceToUnitToChase = GetDistanceTo(UnitsToChase[i]);
						
			if(DistanceToUnitToChase < ShortestDistance)
			{
				ShortestDistance = DistanceToUnitToChase;
				IndexShortestDistance = i;
			}
		}
	}

	bool RValue = false;
	
	if(UnitsToChase[IndexShortestDistance] && UnitsToChase[IndexShortestDistance]->GetUnitState() != UnitData::Dead)
	{
		UnitToChase = UnitsToChase[IndexShortestDistance];
		RValue =  true;
	}

	TArray <AUnitBase*> UnitsToDelete = UnitsToChase;
	
	for(int i = 0; i < UnitsToDelete.Num(); i++)
	{
		if(UnitsToDelete[i] && UnitsToDelete[i]->GetUnitState() == UnitData::Dead)
		{
			UnitsToChase.Remove(UnitsToDelete[i]);
		}
	}

	return RValue;
}*/
