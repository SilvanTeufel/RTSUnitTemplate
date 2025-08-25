// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


#include "Actors/Projectile.h"

#include "MassSignalSubsystem.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Characters/Unit/UnitBase.h"
#include "Controller/AIController/UnitControllerBase.h"
#include "Mass/Signals/MySignals.h"
#include "Net/UnrealNetwork.h"
#include "Widgets/UnitBaseHealthBar.h"

// Sets default values
AProjectile::AProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = TickInterval; 

	// Create a new scene component as the root
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot); // Set it as the root component

	ISMComponent = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ISMComponent"));
	ISMComponent->SetupAttachment(RootComponent);
	ISMComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision); // Disable collision on the ISM
	ISMComponent->SetIsReplicated(true);
	ISMComponent->SetVisibility(false, true);
	
	Niagara_A = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Niagara_A"));
	Niagara_A->SetupAttachment(SceneRoot);
	
	Niagara_B = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Niagara_B"));
	Niagara_B->SetupAttachment(SceneRoot);
	
	// Collision settings for Mesh
	//Mesh_A->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	//Mesh_A->SetCollisionProfileName(TEXT("Trigger"));
	//Mesh_A->SetGenerateOverlapEvents(true);

	// Optionally, initialize Niagara properties here
	SceneRoot->SetVisibility(false, true);

	bReplicates = true;
	InstanceIndex = INDEX_NONE; // Initialize the instance index
}

void AProjectile::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	InitISMComponent(Transform);
	
	if (Niagara_A)
	{
		Niagara_A_Start_Transform = Niagara_A->GetRelativeTransform();
	}

	if (Niagara_B)
	{
		Niagara_B_Start_Transform = Niagara_B->GetRelativeTransform();
	}
	
}

void AProjectile::InitISMComponent(FTransform Transform)
{
	// Create a single instance for this projectile actor in its own ISM component
	if (ISMComponent && ISMComponent->GetStaticMesh())
	{
		//ISMComponent->ClearInstances();

		/*
		// Create a new transform based on the actor's spawn transform, but with our custom scale.
		FTransform NewInstanceTransform = Transform;
		NewInstanceTransform.SetScale3D(ScaleISM); // Use the scale variable

		if (InstanceIndex == INDEX_NONE)
		{
			// This is the first time; add a new instance.
			InstanceIndex = ISMComponent->AddInstance(NewInstanceTransform, true);
		}
		else
		{
			FTransform MyTransform;
			ISMComponent->GetInstanceTransform(InstanceIndex,MyTransform, true);
			// An instance already exists; just update its transform.
			ISMComponent->UpdateInstanceTransform(InstanceIndex, MyTransform, true, true, true);
		}
		*/

		const FTransform LocalIdentityTransform = FTransform::Identity;

		if (InstanceIndex == INDEX_NONE)
		{
			// Add a new instance at the component's local origin.
			InstanceIndex = ISMComponent->AddInstance(LocalIdentityTransform, /*bWorldSpace=*/false);
		}
		else
		{
			// Update the existing instance to be at the component's local origin.
			ISMComponent->UpdateInstanceTransform(InstanceIndex, LocalIdentityTransform, /*bWorldSpace=*/false, /*bMarkRenderStateDirty=*/true, /*bTeleport=*/true);
		}

		const FVector StartLocation = Transform.GetLocation();
		FlightDirection = (TargetLocation - StartLocation).GetSafeNormal();
	}
}


void AProjectile::Init(AActor* TargetActor, AActor* ShootingActor)
{
	Target = TargetActor;
	Shooter = ShootingActor;
	SetOwner(Shooter);
	
	if (Target)
	{
		if (AUnitBase* UnitTarget = Cast<AUnitBase>(Target))
		{
			if (UnitTarget->bUseSkeletalMovement)
			{
				// Aim at the actor’s root location
				TargetLocation = UnitTarget->GetActorLocation();
			}
			else
			{
				// Aim at the ISM instance’s world location
				FTransform InstanceXform;
				UnitTarget->ISMComponent->GetInstanceTransform( UnitTarget->InstanceIndex, /*out*/ InstanceXform, /*worldSpace=*/ true );
				TargetLocation = InstanceXform.GetLocation();
			}
		}
	}
	
	
	AUnitBase* ShootingUnit = Cast<AUnitBase>(Shooter);
	if(ShootingUnit)
	{
		Damage = ShootingUnit->Attributes->GetAttackDamage();
		TeamId = ShootingUnit->TeamId;
		MovementSpeed = ShootingUnit->Attributes->GetProjectileSpeed();


		if(ShootingUnit->bUseSkeletalMovement)
			ShooterLocation = ShootingUnit->GetActorLocation();
		else
		{
			FTransform InstanceXform;
			ShootingUnit->ISMComponent->GetInstanceTransform( ShootingUnit->InstanceIndex, /*out*/ InstanceXform, /*worldSpace=*/ true );
			ShooterLocation = InstanceXform.GetLocation();
		}
	}
	
}

void AProjectile::InitForAbility(AActor* TargetActor, AActor* ShootingActor)
{
	Target = TargetActor;
	Shooter = ShootingActor;
	SetOwner(Shooter);
	
	if (Target)
	{
		if (AUnitBase* UnitTarget = Cast<AUnitBase>(Target))
		{
			if (UnitTarget->bUseSkeletalMovement)
			{
				// Aim at the actor’s root location
				TargetLocation = UnitTarget->GetActorLocation();
			}
			else if (UnitTarget->ISMComponent)
			{
				// Aim at the ISM instance’s world location
				FTransform InstanceXform;
				UnitTarget->ISMComponent->GetInstanceTransform( UnitTarget->InstanceIndex, /*out*/ InstanceXform, /*worldSpace=*/ true );
				TargetLocation = InstanceXform.GetLocation();
			}
			else
			{
				// Fallback
				TargetLocation = UnitTarget->GetActorLocation();
			}
		}
		else
		{
			// Non‐unit targets just give their root location
			TargetLocation = Target->GetActorLocation();
		}
	}
	
	if(ShootingActor)
		ShooterLocation = ShootingActor->GetActorLocation();
	
	
	AUnitBase* ShootingUnit = Cast<AUnitBase>(Shooter);
	if(ShootingUnit)
	{
		//Damage = ShootingUnit->Attributes->GetAttackDamage();
		UseAttributeDamage = false;
		TeamId = ShootingUnit->TeamId;
	}
	
}

void AProjectile::InitForLocationPosition(FVector Aim, AActor* ShootingActor)
{

	Shooter = ShootingActor;
	TargetLocation = Aim;
	SetOwner(Shooter);

	/*
	FVector FlyDir = (TargetLocation - GetActorLocation()).GetSafeNormal();
	if (!FlyDir.IsNearlyZero())
	{
		// assume you have a FRotator ProjectileRotationOffset member
		FRotator AimRot = FlyDir.Rotation() + RotationOffset;
		SetActorRotation(AimRot);
	}*/
	
	if(ShootingActor)
		ShooterLocation = ShootingActor->GetActorLocation();
	
	AUnitBase* ShootingUnit = Cast<AUnitBase>(Shooter);
	if(ShootingUnit)
	{
		//Damage = ShootingUnit->Attributes->GetAttackDamage();
		UseAttributeDamage = false;
		TeamId = ShootingUnit->TeamId;
	}


	FTransform InstanceTransform;
	if (ISMComponent && ISMComponent->IsValidInstance(InstanceIndex))
	{
		ISMComponent->GetInstanceTransform(InstanceIndex, InstanceTransform, true);
	}
	else
	{
		// Fallback to actor location if instance is somehow invalid
		InstanceTransform = GetActorTransform();
	}
    
	const FVector StartLocation = InstanceTransform.GetLocation();

	// Calculate the one-time flight direction
	FlightDirection = (TargetLocation - StartLocation).GetSafeNormal();

	// Check for a zero vector to prevent getting stuck
	if (FlightDirection.IsNearlyZero())
	{
		// The target is the same as the start, destroy the projectile to prevent errors
		Destroy();
		return;
	}


	bIsInitialized = true;
}


void AProjectile::SetProjectileVisibility_Implementation()
{

	AUnitBase* ShootingUnit = Cast<AUnitBase>(Shooter);
	AUnitBase* TargetUnit = Cast<AUnitBase>(Target);

	bool bShootingVisible = ShootingUnit ? ((ShootingUnit->IsVisibleEnemy || ShootingUnit->IsMyTeam) ? true : (!ShootingUnit->EnableFog)) : false;
	bool bTargetVisible   = TargetUnit ? ((TargetUnit->IsVisibleEnemy  || TargetUnit->IsMyTeam)  ? true : (!TargetUnit->EnableFog))   : false;
	bool bFinalVisibility = bShootingVisible || bTargetVisible;
	
	//SceneRoot->SetVisibility(bFinalVisibility, true);
	ISMComponent->SetVisibility(bFinalVisibility, true);
}

// Called when the game starts or when spawned
void AProjectile::BeginPlay()
{
	Super::BeginPlay();
	SetReplicateMovement(true);
}

void AProjectile::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AProjectile, Target);
	DOREPLIFETIME(AProjectile, Shooter);

	DOREPLIFETIME(AProjectile, ImpactSound);
	DOREPLIFETIME(AProjectile, ImpactVFX);
	
	//DOREPLIFETIME(AProjectile, Material);
	DOREPLIFETIME(AProjectile, Damage);
	DOREPLIFETIME(AProjectile, LifeTime);
	DOREPLIFETIME(AProjectile, MaxLifeTime);
	DOREPLIFETIME(AProjectile, TeamId);
	DOREPLIFETIME(AProjectile, MovementSpeed);
	DOREPLIFETIME(AProjectile, DestructionDelayTime);
	DOREPLIFETIME(AProjectile, RotateMesh);
	DOREPLIFETIME(AProjectile, RotationSpeed);
	DOREPLIFETIME(AProjectile, TargetLocation);
	DOREPLIFETIME(AProjectile, ShooterLocation);
	DOREPLIFETIME(AProjectile, FollowTarget);
	DOREPLIFETIME(AProjectile, MaxPiercedTargets);
	DOREPLIFETIME(AProjectile, PiercedTargets);
	DOREPLIFETIME(AProjectile, IsHealing);
	DOREPLIFETIME(AProjectile, IsBouncingBack);
	DOREPLIFETIME(AProjectile, IsBouncingNext);
	DOREPLIFETIME(AProjectile, BouncedBack);
	DOREPLIFETIME(AProjectile, ProjectileEffect); // Added for Build
	DOREPLIFETIME(AProjectile, UseAttributeDamage);

	DOREPLIFETIME(AProjectile, ScaleImpactVFX);
	DOREPLIFETIME(AProjectile, ScaleImpactSound);

	DOREPLIFETIME(AProjectile, InstanceIndex);
	DOREPLIFETIME(AProjectile, ISMComponent);
	
	DOREPLIFETIME(AProjectile, Niagara_A);
	DOREPLIFETIME(AProjectile, Niagara_B);

	DOREPLIFETIME(AProjectile, Niagara_A_Start_Transform);
	DOREPLIFETIME(AProjectile, Niagara_B_Start_Transform);
}

// Implement the new multicast function to update clients
void AProjectile::Multicast_UpdateISMTransform_Implementation(const FTransform& NewTransform)
{
	if (ISMComponent && ISMComponent->IsValidInstance(InstanceIndex))
	{
		ISMComponent->UpdateInstanceTransform(InstanceIndex, NewTransform, true, false, false);
	}
	//SetActorTransform(NewTransform, false, nullptr, ETeleportType::TeleportPhysics)

	// Manually move Niagara_A, preserving its relative offset
	if (Niagara_A)
	{
		// Get the local offset vector (e.g., FVector(-750, 0, 0)) from the start transform
		const FVector LocalOffset = Niagara_A_Start_Transform.GetLocation();

		// Rotate this local offset by the new instance's world rotation
		const FVector WorldOffset = NewTransform.GetRotation().RotateVector(LocalOffset);
    
		// Add the correctly rotated offset to the instance's location
		const FVector FinalWorldLocation = NewTransform.GetLocation() + WorldOffset;

		// Combine the instance's rotation with the component's relative rotation
		const FQuat FinalWorldRotation = NewTransform.GetRotation() * Niagara_A_Start_Transform.GetRotation();
       
		// Create the final transform
		const FTransform FinalWorldTransform(FinalWorldRotation, FinalWorldLocation, Niagara_A_Start_Transform.GetScale3D());

		// Set the final transform on the Niagara component
		Niagara_A->SetWorldTransform(FinalWorldTransform, false, nullptr, ETeleportType::TeleportPhysics);
	}

	// Manually move Niagara_B to the new location
	if (Niagara_B)
	{
		// Get the local offset vector (e.g., FVector(-750, 0, 0)) from the start transform
		const FVector LocalOffset = Niagara_B_Start_Transform.GetLocation();

		// Rotate this local offset by the new instance's world rotation
		const FVector WorldOffset = NewTransform.GetRotation().RotateVector(LocalOffset);
    
		// Add the correctly rotated offset to the instance's location
		const FVector FinalWorldLocation = NewTransform.GetLocation() + WorldOffset;

		// Combine the instance's rotation with the component's relative rotation
		const FQuat FinalWorldRotation = NewTransform.GetRotation() * Niagara_B_Start_Transform.GetRotation();
       
		// Create the final transform
		const FTransform FinalWorldTransform(FinalWorldRotation, FinalWorldLocation, Niagara_B_Start_Transform.GetScale3D());

		// Set the final transform on the Niagara component
		Niagara_B->SetWorldTransform(FinalWorldTransform, false, nullptr, ETeleportType::TeleportPhysics);
	}
}

// Called every frame
void AProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	CheckViewport();
	LifeTime += DeltaTime;
	
	if(RotateMesh)
	{
		// We only want the server to calculate this and replicate it
		if (HasAuthority())
		{
			// 1. Get the current transform of the instance. It already includes this frame's movement.
			FTransform CurrentTransform;
			ISMComponent->GetInstanceTransform(InstanceIndex, /*out*/ CurrentTransform, /*worldSpace=*/true);

			// 2. Calculate the rotation amount (the delta) for this frame.
			FRotator RotationDelta = FRotator(RotationSpeed.X * DeltaTime, RotationSpeed.Y * DeltaTime, RotationSpeed.Z * DeltaTime);

			// 3. Add the rotation delta to the instance's current rotation.
			CurrentTransform.ConcatenateRotation(RotationDelta.Quaternion());
			
			// 4. Replicate this final transform to all clients.
			Multicast_UpdateISMTransform(CurrentTransform);
		}
	}
	if(LifeTime > MaxLifeTime && !FollowTarget)
	{
		Destroy(true, false);
	}else if(LifeTime > MaxLifeTime && FollowTarget)
	{
		//Impact(Target);
		Destroy(true, false);
	}else if(Target)
	{
		FlyToUnitTarget();
	}else
	{
		FlyToLocationTarget(DeltaTime);
	}
}

void AProjectile::CheckViewport()
{
	if (IsInViewport(GetActorLocation(), VisibilityOffset))
	{
		IsOnViewport = true;
	}
	else
	{
		IsOnViewport = false;
	}
}

bool AProjectile::IsInViewport(FVector WorldPosition, float Offset)
{
	if (APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
	{
		FVector2D ScreenPosition;
		UGameplayStatics::ProjectWorldToScreen(PlayerController, WorldPosition, ScreenPosition);

		int32 ViewportSizeX, ViewportSizeY;
		PlayerController->GetViewportSize(ViewportSizeX, ViewportSizeY);

		return ScreenPosition.X >= -Offset && ScreenPosition.X <= ViewportSizeX + Offset && ScreenPosition.Y >= -Offset && ScreenPosition.Y <= ViewportSizeY + Offset;
	}

	return false;
}


void AProjectile::FlyToUnitTarget()
{
    if (!HasAuthority()) return;

    // --- 1. Get the INSTANCE'S current transform. This is the projectile's true location. ---
    FTransform CurrentTransform;
    if (!ISMComponent || !ISMComponent->IsValidInstance(InstanceIndex))
    {
        return; // Safety check
    }
    ISMComponent->GetInstanceTransform(InstanceIndex, /*out*/ CurrentTransform, /*worldSpace=*/ true);
    const FVector CurrentLocation = CurrentTransform.GetLocation();


    // --- 2. Update target's location and calculate flight direction ---
    if (FollowTarget)
    {
        // This part is correct: you get the target's latest position.
        AUnitBase* UnitTarget = Cast<AUnitBase>(Target);
        if (UnitTarget && UnitTarget->GetUnitState() != UnitData::Dead)
        {
           FVector CurrentTargetLoc;
           if (UnitTarget->bUseSkeletalMovement)
           {
              CurrentTargetLoc = UnitTarget->GetActorLocation();
           }
           else if (UnitTarget->ISMComponent)
           {
              FTransform InstanceXform;
              UnitTarget->ISMComponent->GetInstanceTransform(UnitTarget->InstanceIndex, /*out*/ InstanceXform, /*worldSpace=*/ true);
              CurrentTargetLoc = InstanceXform.GetLocation();
           }
           TargetLocation = CurrentTargetLoc;
        }

        // ***** THE FIX IS HERE *****
        // Calculate direction from the INSTANCE'S current location, not the actor's spawn point.
        FlightDirection  = (TargetLocation - CurrentLocation).GetSafeNormal();
    }
    // If FollowTarget is false, FlightDirection remains the initial direction set on spawn.


    // --- 3. Calculate this frame's movement ---
    const float FrameSpeed = MovementSpeed * GetWorld()->GetDeltaSeconds() * 10.f; // The * 10.f is very fast, ensure this is intended.
    const FVector FrameMovement = FlightDirection * FrameSpeed;


    // --- 4. Apply movement and replicate to clients ---
    FTransform NewTransform = CurrentTransform;
    NewTransform.AddToTranslation(FrameMovement);
    Multicast_UpdateISMTransform(NewTransform); // You don't need the "if valid instance" check here, we already did it at the top.


    // --- 5. Check for impact ---
    const float Distance = FVector::Dist(NewTransform.GetLocation(), TargetLocation);
    if (Distance <= FrameSpeed + CollisionRadius)
    {
       Impact(Target);
       Destroy(true, false);
    }
}

void AProjectile::FlyToLocationTarget(float DeltaSeconds)
{
	if (!bIsInitialized) return;
    if (!HasAuthority()) return;

	OverlapCheckTimer += DeltaSeconds;
    // --- 1. Get current transform FROM THE INSTANCE ---
    // This is the true current location of our projectile in the world.
    FTransform CurrentTransform;
    if (!ISMComponent || !ISMComponent->IsValidInstance(InstanceIndex))
    {
        return; // Safety check
    }
    ISMComponent->GetInstanceTransform(InstanceIndex, CurrentTransform, true /* bWorldSpace */);

    const FVector CurrentLocation = CurrentTransform.GetLocation();
	
	// Check for a zero vector to prevent getting stuck
	if (FlightDirection.IsNearlyZero(0.1f))
	{
		// The target is the same as the start, destroy the projectile to prevent errors
		FlightDirection = (TargetLocation - CurrentLocation).GetSafeNormal();
	}
	
    const FVector FrameMovement = FlightDirection * MovementSpeed * GetWorld()->GetDeltaSeconds() * 10.f;

    // The potential new location after this frame's movement
    const FVector EndLocation = CurrentLocation + FrameMovement;

    // --- 3. Perform a Sphere Trace for Collision ---
    FHitResult HitResult;
	bool bHit = false;
	TArray<AActor*> OutActors;
    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(this);
    ActorsToIgnore.Add(Shooter);

    TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypesToQuery;
    ObjectTypesToQuery.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_Pawn));

	if (OverlapCheckTimer >= OverlapCheckInterval)
	{
		OverlapCheckTimer = 0.f;
		/// THIS SHOULD HAPPEN ONLY ONCE EVERY 0.5sec
		// 1. Visualize the overlap area first for debugging
		if (DebugTargetLocation)
			DrawDebugSphere(
				GetWorld(),
				EndLocation,         // Center of the sphere is at the end of the intended path
				CollisionRadius,
				24,                  // Segments for a smooth sphere
				FColor::Red,      // A distinct color for the overlap check
				true,
				5.0f                 // Lifetime of 5 seconds
			);

		// 2. Perform the overlap check
		UKismetSystemLibrary::SphereOverlapActors(
			GetWorld(),
			EndLocation,
			CollisionRadius,
			ObjectTypesToQuery,
			nullptr,             // No specific actor class to filter
			ActorsToIgnore,
			OutActors            // Array to be filled with found actors
		);


		for (AActor* HitActor : OutActors)
		{
			// An overlap doesn't provide a detailed FHitResult, so we create one manually.
			FHitResult ManualHitResult;
			ManualHitResult.ImpactPoint = HitActor->GetActorLocation(); // Approximating impact point
			ManualHitResult.bBlockingHit = true;   // Best guess for the component
			// Call the overlap function for each actor found in the sphere
			OnOverlapBegin_Implementation(
				nullptr,          // We don't have an "OverlappedComp" from the projectile
				HitActor,         // The actor we hit
				nullptr, // The component we hit
				-1,               // OtherBodyIndex, not available from an overlap
				false,            // This was not from a sweep
				ManualHitResult   // The manually created HitResult
			);
		}
	}
    // --- 4. Update Transform and Multicast ---
    // Calculate the new final transform
    FTransform NewTransform = CurrentTransform;
    NewTransform.AddToTranslation(FrameMovement);

    // This is the key: We update the instance directly since we aren't using the Actor's transform as the source of truth.
    // The next frame will correctly read the updated instance transform.
    if (ISMComponent->IsValidInstance(InstanceIndex))
    {
       // Update the ISM directly
       ISMComponent->UpdateInstanceTransform(InstanceIndex, NewTransform, true, true, true);
       
       // You still need to tell clients to do the same! Your multicast function is essential here.
       // The implementation of this function should call UpdateInstanceTransform on the clients.
       Multicast_UpdateISMTransform(NewTransform); 
    }
}

void AProjectile::Impact(AActor* ImpactTarget)
{
	if (!HasAuthority()) return;
	
	AUnitBase* ShootingUnit = Cast<AUnitBase>(Shooter);
	AUnitBase* UnitToHit = Cast<AUnitBase>(ImpactTarget);

	if(UnitToHit && ShootingUnit)
	{
		float NewDamage = Damage;
		if (UseAttributeDamage)
		{
			NewDamage = ShootingUnit->Attributes->GetAttackDamage() - UnitToHit->Attributes->GetArmor();
			
			if(ShootingUnit->IsDoingMagicDamage)
				NewDamage = ShootingUnit->Attributes->GetAttackDamage() - UnitToHit->Attributes->GetMagicResistance();
		}else
		{
			NewDamage = Damage - UnitToHit->Attributes->GetArmor();
			
			if(ShootingUnit->IsDoingMagicDamage)
				NewDamage = Damage - UnitToHit->Attributes->GetMagicResistance();
		}
		
		if(UnitToHit->Attributes->GetShield() <= 0)
			UnitToHit->SetHealth_Implementation(UnitToHit->Attributes->GetHealth()-NewDamage);
		else
			UnitToHit->SetShield_Implementation(UnitToHit->Attributes->GetShield()-NewDamage);
			//UnitToHit->Attributes->SetAttributeShield(UnitToHit->Attributes->GetShield()-NewDamage);


		if(UnitToHit && UnitToHit->GetUnitState() == UnitData::Dead)
		{
			// Do Nothing
		}else if(UnitToHit && UnitToHit->TeamId != TeamId)
		{
			UnitToHit->ApplyInvestmentEffect(ProjectileEffect);
		}
		
		ShootingUnit->IncreaseExperience();
		UnitToHit->ActivateAbilityByInputID(UnitToHit->DefensiveAbilityID, UnitToHit->DefensiveAbilities);
		UnitToHit->FireEffects(ImpactVFX, ImpactSound, ScaleImpactVFX, ScaleImpactSound);
		SetNextBouncing(ShootingUnit, UnitToHit);
		SetBackBouncing(ShootingUnit);
		DestroyWhenMaxPierced();
	}			
}


void AProjectile::ImpactHeal(AActor* ImpactTarget)
{
	if (!HasAuthority()) return;
	
	AUnitBase* ShootingUnit = Cast<AUnitBase>(Shooter);
	AUnitBase* UnitToHit = Cast<AUnitBase>(ImpactTarget);
	//UE_LOG(LogTemp, Warning, TEXT("Projectile ShootingUnit->Attributes->GetAttackDamage()! %f"), ShootingUnit->Attributes->GetAttackDamage());
	if(UnitToHit && UnitToHit->TeamId == TeamId && ShootingUnit)
	{
		float NewDamage = Damage;
		
		if (UseAttributeDamage)
			NewDamage = ShootingUnit->Attributes->GetAttackDamage();
		
		if(UnitToHit->Attributes->GetShield() <= 0)
			UnitToHit->SetHealth_Implementation(UnitToHit->Attributes->GetHealth()+NewDamage);
		else
			UnitToHit->SetShield_Implementation(UnitToHit->Attributes->GetShield()+NewDamage);
			//UnitToHit->Attributes->SetAttributeShield(UnitToHit->Attributes->GetShield()+NewDamage);


		if(UnitToHit && UnitToHit->GetUnitState() == UnitData::Dead)
		{
			// Do Nothing
		}else if(UnitToHit && UnitToHit->TeamId == TeamId)
		{
			UnitToHit->ApplyInvestmentEffect(ProjectileEffect);
		}
		
		ShootingUnit->IncreaseExperience();
		UnitToHit->ActivateAbilityByInputID(UnitToHit->DefensiveAbilityID, UnitToHit->DefensiveAbilities);
		UnitToHit->FireEffects(ImpactVFX, ImpactSound, ScaleImpactVFX, ScaleImpactSound);
		SetNextBouncing(ShootingUnit, UnitToHit);
		SetBackBouncing(ShootingUnit);
		DestroyWhenMaxPierced();
	}			
}


void AProjectile::OnOverlapBegin_Implementation(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{

	// Prüfen, ob wir NICHT im Game Thread sind
	if ( !IsInGameThread() )
	{
		// Sicherere Erfassung mit Weak Pointers für UObjects
		TWeakObjectPtr<AProjectile> WeakThis(this);
		TWeakObjectPtr<UPrimitiveComponent> WeakOverlappedComp(OverlappedComp);
		TWeakObjectPtr<AActor> WeakOtherActor(OtherActor);
		TWeakObjectPtr<UPrimitiveComponent> WeakOtherComp(OtherComp);
		// HitResult und primitive Typen per Wert kopieren ist sicher
		FHitResult CapturedSweepResult = SweepResult;

		AsyncTask(ENamedThreads::GameThread,
			[WeakThis, WeakOverlappedComp, WeakOtherActor, WeakOtherComp, OtherBodyIndex, bFromSweep, CapturedSweepResult]() // Erfasste Variablen
		{
		   // Starke Pointer im Game Thread holen
		   AProjectile* StrongThis = WeakThis.Get();
		   UPrimitiveComponent* StrongOverlappedComp = WeakOverlappedComp.Get();
		   AActor* StrongOtherActor = WeakOtherActor.Get();
		   UPrimitiveComponent* StrongOtherComp = WeakOtherComp.Get();

		   // Prüfen, ob 'this' und ggf. andere kritische Objekte noch gültig sind
		   if (StrongThis /* && StrongOverlappedComp && StrongOtherActor */) // Prüfe nach Bedarf
		   {
			  // Funktion erneut aufrufen, jetzt garantiert im Game Thread
			  // Verwende die starken Pointer oder kopierten Werte
			  StrongThis->OnOverlapBegin(StrongOverlappedComp, StrongOtherActor, StrongOtherComp, OtherBodyIndex, bFromSweep, CapturedSweepResult);
			  // Oder ggf. direkt die _Implementation aufrufen:
			  // StrongThis->OnOverlapBegin_Implementation(StrongOverlappedComp, StrongOtherActor, StrongOtherComp, OtherBodyIndex, bFromSweep, CapturedSweepResult);
		   }
		});
		// Ursprüngliche Ausführung beenden
		return;
	}

	AUnitBase* UnitToHit = nullptr;

	if (OtherActor)
	{
		// 1. First, try to get the unit by casting the actor we directly hit.
		UnitToHit = Cast<AUnitBase>(OtherActor);

		// 2. If the direct cast fails, it might be because we hit a component (like an ISM).
		//    Let's check if the component's OWNER is a unit.
		if (!UnitToHit && OtherComp)
		{
			AActor* OwningActor = OtherComp->GetOwner();

			if (OwningActor)
			{
				// Now, cast the OwningActor to a UnitBase to see if it's what we're looking for.
				UnitToHit = Cast<AUnitBase>(OwningActor);
			}
		}
	}
	
	if(UnitToHit)
	{
		//AUnitBase* UnitToHit = Cast<AUnitBase>(OtherActor);
		AUnitBase* ShootingUnit = Cast<AUnitBase>(Shooter);
		if(UnitToHit && UnitToHit->GetUnitState() == UnitData::Dead)
		{
			ImpactEvent();
			UnitToHit->FireEffects(ImpactVFX, ImpactSound, ScaleImpactVFX, ScaleImpactSound);
			DestroyProjectileWithDelay();
		}else if(UnitToHit && UnitToHit->TeamId == TeamId && BouncedBack && IsHealing)
		{
			ImpactEvent();
			ImpactHeal(UnitToHit);
		}else if(UnitToHit && UnitToHit->TeamId == TeamId && BouncedBack && !IsHealing)
		{
			ImpactEvent();
			UnitToHit->FireEffects(ImpactVFX, ImpactSound, ScaleImpactVFX, ScaleImpactSound);
			DestroyProjectileWithDelay();
		}else if(UnitToHit && UnitToHit->TeamId != TeamId && !IsHealing)
		{
			ImpactEvent();
			Impact(UnitToHit);
			SetIsAttacked(UnitToHit);
		}else if(UnitToHit && UnitToHit->TeamId == TeamId && IsHealing)
		{
			ImpactEvent();
			ImpactHeal(UnitToHit);
		}
			
	}
}

void AProjectile::DestroyWhenMaxPierced()
{
	PiercedTargets += 1;
	if(PiercedTargets >= MaxPiercedTargets)
	{
		DestroyProjectileWithDelay();
	}
}

void AProjectile::DestroyProjectileWithDelay()
{
	FTimerHandle TimerHandle;
	GetWorld()->GetTimerManager().SetTimer(TimerHandle, this, &AProjectile::DestroyProjectile, DestructionDelayTime, false);
}

void AProjectile::DestroyProjectile()
{
	Destroy(true, false);
}


void AProjectile::SetIsAttacked(AUnitBase* UnitToHit)
{
	if(UnitToHit->GetUnitState() != UnitData::Run &&
		UnitToHit->GetUnitState() != UnitData::Attack &&
		UnitToHit->GetUnitState() != UnitData::Casting &&
		UnitToHit->GetUnitState() != UnitData::Rooted &&
		UnitToHit->GetUnitState() != UnitData::Pause)
	{
		UnitToHit->UnitControlTimer = 0.f;
		UnitToHit->SetUnitState( UnitData::IsAttacked );
		
		UWorld* World = GetWorld();
				
		if (!World) return;
		
		UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(World);

		if (!SignalSubsystem) return;
		
		if (UnitToHit && UnitToHit->MassActorBindingComponent)
		{
			FMassEntityHandle MassEntityHandle =  UnitToHit->MassActorBindingComponent->GetMassEntityHandle();
		
			if (MassEntityHandle.IsValid())
				SignalSubsystem->SignalEntity(UnitSignals::IsAttacked, MassEntityHandle);
		}
	}else if(UnitToHit->GetUnitState() == UnitData::Casting)
	{
		UnitToHit->UnitControlTimer -= UnitToHit->ReduceCastTime;
	}else if(UnitToHit->GetUnitState() == UnitData::Rooted)
	{
		UnitToHit->UnitControlTimer -= UnitToHit->ReduceRootedTime;
	}
}

void AProjectile::SetBackBouncing(AUnitBase* ShootingUnit)
{
	if(IsBouncingBack && IsBouncingNext && PiercedTargets == (MaxPiercedTargets-1))
	{
		Target = ShootingUnit;
		TargetLocation = ShootingUnit->GetActorLocation();
		BouncedBack = true;
	}else if(IsBouncingBack && PiercedTargets < MaxPiercedTargets)
	{
		Target = ShootingUnit;
		TargetLocation = ShootingUnit->GetActorLocation();
		BouncedBack = true;
	}
}

void AProjectile::SetNextBouncing(AUnitBase* ShootingUnit, AUnitBase* UnitToHit)
{
	if(IsBouncingNext)
	{
		AUnitBase* NewTarget = GetNextUnitInRange(ShootingUnit, UnitToHit);

		if(!NewTarget)
		{
			DestroyProjectileWithDelay();
			return;
		}
		
		Target = NewTarget;
		TargetLocation = NewTarget->GetActorLocation();
	}
}

AUnitBase* AProjectile::GetNextUnitInRange(AUnitBase* ShootingUnit, AUnitBase* UnitToHit)
{
	float Range = 9999999.f;
	AUnitBase* RUnit = nullptr; 
	for (AUnitBase* Unit : ShootingUnit->UnitsToChase)
	{
		if (Unit && Unit != UnitToHit)
		{
			float Distance = FVector::Dist(Unit->GetActorLocation(), ShootingUnit->GetActorLocation());
			if (Distance <= Range)
			{
				Range = Distance;
				RUnit = Unit;
			}else
			{
				
			}
		}
	}
	
	return RUnit;
}

void AProjectile::SetVisibility(bool Visible)
{
	//GetMesh()->SetVisibility(Visible);
	
	if (ISMComponent && ISMComponent->IsValidInstance(InstanceIndex))
	{
		FTransform InstanceTransform;
		ISMComponent->GetInstanceTransform(InstanceIndex, InstanceTransform, /*bWorldSpace=*/true);

		// Hide the instance by scaling it to zero, show it by scaling it back to one.
		InstanceTransform.SetScale3D(Visible ? FVector(1.0f) : FVector::ZeroVector);
        
		ISMComponent->UpdateInstanceTransform(InstanceIndex, InstanceTransform, true, true);
	}
}

