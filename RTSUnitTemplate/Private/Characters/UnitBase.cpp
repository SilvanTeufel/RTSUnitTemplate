// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


#include "Characters/Unit/UnitBase.h"

#include "AbilitySystemComponent.h"
#include "NavCollision.h"
#include "Widgets/UnitBaseHealthBar.h"
#include "Components/CapsuleComponent.h"
#include "Actors/SelectedIcon.h"
#include "Actors/Projectile.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

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

	//Health = SpawnHealth;
	//Shield = SpawnShield;

	SetReplicates(true);
	GetMesh()->SetIsReplicated(true);
	/*
	if (HasAuthority())
	{
		bReplicates = true;
		SetReplicateMovement(true);
		GetMesh()->SetIsReplicated(true);
	}*/

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponentBase>("AbilitySystemComp");
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
	
	Attributes = CreateDefaultSubobject<UAttributeSetBase>("Attributes");
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
		
		FRotator NewRotation = FRotator(0, -90, 0);
		FQuat QuatRotation = FQuat(NewRotation);
		
		GetMesh()->SetRelativeRotation(QuatRotation, false, 0, ETeleportType::None);
		GetMesh()->SetRelativeLocation(FVector(0, 0, -75), false, 0, ETeleportType::None);
		SpawnSelectedIcon();
		//Health = SpawnHealth;
		//Shield = SpawnShield;
		//Health = MaxHealth;

	
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

	DOREPLIFETIME(AUnitBase, UnitState);
	DOREPLIFETIME(AUnitBase, UnitStatePlaceholder);
	DOREPLIFETIME(AUnitBase, TeamId)
	//DOREPLIFETIME(AUnitBase, Shield);
	//DOREPLIFETIME(AUnitBase, MaxShield);
	//DOREPLIFETIME(AUnitBase, SpawnShield);
	//DOREPLIFETIME(AUnitBase, Health);
	//DOREPLIFETIME(AUnitBase, MaxHealth);
	//DOREPLIFETIME(AUnitBase, SpawnHealth);
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

// HUDBase related //////////////////////////////////////////////////
void AUnitBase::IsAttacked(AActor* AttackingCharacter) // AActor* SelectedCharacter
{
	SetUnitState(UnitData::IsAttacked);
	SetWalkSpeed(Attributes->GetIsAttackedSpeed());

}
void AUnitBase::SetRunLocation_Implementation(FVector Location)
{
	RunLocation = Location;
}

void AUnitBase::SetWalkSpeed(float Speed)
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
/////////////////////////////////////////////////////////////////////


void AUnitBase::SetWaypoint(AWaypoint* NewNextWaypoint)
{
	NextWaypoint = NewNextWaypoint;
}

void AUnitBase::SetUnitState(TEnumAsByte<UnitData::EState> NewUnitState)
{
	UnitState = NewUnitState;
}

TEnumAsByte<UnitData::EState> AUnitBase::GetUnitState()
{
	return UnitState;
}

/*
float AUnitBase::GetHealth()
{
	return Health;
}

void AUnitBase::SetHealth_Implementation(float NewHealth)
{
	if(NewHealth > MaxHealth)
	{
		Health = MaxHealth;
	}else
	{
		Health = NewHealth;
	}

	if(NewHealth <= 0.f)
	{
		SetWalkSpeed(0);
		GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SetActorEnableCollision(false);
		SetDeselected();
		SetUnitState(UnitData::Dead);
		UnitControlTimer = 0.f;
	}
}

float AUnitBase::GetMaxHealth()
{
	return MaxHealth;
}*/

void AUnitBase::SetHealth_Implementation(float NewHealth)
{
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
}

/*
float AUnitBase::GetShield()
{
	return Shield;
}

void AUnitBase::SetShield(float NewShield)
{
	if(NewShield <= 0)
	{
		Shield = 0;
		SetHealth(GetHealth()+NewShield);
	}else if(NewShield > MaxShield)
	{
		Shield = MaxShield;
	}else
	{
		Shield = NewShield;
	}
}

float AUnitBase::GetMaxShield()
{
	return MaxShield;
}*/
/////////////////////////////////////////////////////////////////////


// HUDBase related //////////////////////////////////////////////////
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
	SelectedIcon = GetWorld()->SpawnActor<ASelectedIcon>(ASelectedIcon::StaticClass(), SpellTransform, SpawnParams);
	if (SelectedIcon) {
		SelectedIcon->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetIncludingScale, FName("rootSocket"));
		SelectedIcon->ChangeMaterialColour(FVector4d(5.f, 40.f, 30.f, 0.5f));
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
		
			MyProjectile->Init(Target, Attacker);
			MyProjectile->Mesh->OnComponentBeginOverlap.AddDynamic(MyProjectile, &AProjectile::OnOverlapBegin);


			
		
			UGameplayStatics::FinishSpawningActor(MyProjectile, Transform);
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
}
/////////////////////////////////////////////////////////////////////
