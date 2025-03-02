// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "GAS/GameplayAbilityBase.h"

#include "Characters/Unit/UnitBase.h"



UGameplayAbilityBase::UGameplayAbilityBase()
{
	UpdateTooltipText();
}

void UGameplayAbilityBase::SpawnProjectileFromClass(FVector Aim, AActor* Attacker, TSubclassOf<class AProjectile> ProjectileClass, int MaxPiercedTargets, int ProjectileCount, float Spread, bool IsBouncingNext, bool IsBouncingBack, float ZOffset, float Scale) // FVector TargetLocation
{
	if(!Attacker || !ProjectileClass)
		return;

	AUnitBase* ShootingUnit = Cast<AUnitBase>(Attacker);

	for(int Count = 0; Count < ProjectileCount; Count++){
		int  MultiAngle = (Count == 0) ? 0 : (Count % 2 == 0 ? -1 : 1);
		FVector ShootDirection = UKismetMathLibrary::GetDirectionUnitVector(ShootingUnit->GetActorLocation(), Aim);
		FVector ShootOffset = FRotator(0.f,MultiAngle*90.f,0.f).RotateVector(ShootDirection);
		
		FVector LocationToShoot = Aim+ShootOffset*Spread;
		
		LocationToShoot.Z += ShootingUnit->GetActorLocation().Z;
		LocationToShoot.Z += ZOffset;
		
		if(ShootingUnit)
		{
			FTransform Transform;
			Transform.SetLocation(ShootingUnit->GetActorLocation() + ShootingUnit->Attributes->GetProjectileScaleActorDirectionOffset()*ShootingUnit->GetActorForwardVector() + ShootingUnit->ProjectileSpawnOffset);



			FVector Direction = (LocationToShoot - ShootingUnit->GetActorLocation()).GetSafeNormal(); // Target->GetActorLocation()
			FRotator InitialRotation = Direction.Rotation() + ShootingUnit->ProjectileRotationOffset;

			Transform.SetRotation(FQuat(InitialRotation));
			Transform.SetScale3D(ShootingUnit->ProjectileScale*Scale);
			
			const auto MyProjectile = Cast<AProjectile>
								(UGameplayStatics::BeginDeferredActorSpawnFromClass
								(this, ProjectileClass, Transform,  ESpawnActorCollisionHandlingMethod::AlwaysSpawn));
			if (MyProjectile != nullptr)
			{

				//MyProjectile->TargetLocation = LocationToShoot;
				MyProjectile->InitForLocationPosition(LocationToShoot, Attacker);
			
				MyProjectile->Mesh->OnComponentBeginOverlap.AddDynamic(MyProjectile, &AProjectile::OnOverlapBegin);
				MyProjectile->MaxPiercedTargets = MaxPiercedTargets;
				MyProjectile->IsBouncingNext = IsBouncingNext;
				MyProjectile->IsBouncingBack = IsBouncingBack;

				//if(!MyProjectile->IsOnViewport) MyProjectile->SetProjectileVisibility(false);
				//MyProjectile->SetProjectileVisibility();
				UGameplayStatics::FinishSpawningActor(MyProjectile, Transform);

				//ShootingUnit->ProjectileAndEffectsVisibility(MyProjectile);
			}
		}
	}
}


// In cpp file
FText UGameplayAbilityBase::CreateTooltipText() const
{
	const FString costString = ConstructionCost.ToFormattedString();
	const FString fullText = FString::Printf(TEXT("%s\n\n%s\n\nKeyboard %s"),
		*AbilityName,
		*costString, 
		*KeyboardKey);

	return FText::FromString(fullText);
}

void UGameplayAbilityBase::UpdateTooltipText()
{
	ToolTipText = CreateTooltipText();
}