// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GAS.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayAbilityBase.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API UGameplayAbilityBase : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGameplayAbilityBase();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	UTexture2D* AbilityIcon;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category= RTSUnitTemplate)
	EGASAbilityInputID AbilityInputID = EGASAbilityInputID::None;

	UFUNCTION(BlueprintImplementableEvent, Category = RTSUnitTemplate)
	void OnAbilityCastComplete( const FHitResult& InHitResult = FHitResult());
	
	UFUNCTION(BlueprintImplementableEvent, Category = RTSUnitTemplate)
	void OnAbilityMouseHit(const FHitResult& InHitResult);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SpawnProjectileFromClass(FVector Aim, AActor* Attacker, TSubclassOf<class AProjectile> ProjectileClass, int MaxPiercedTargets, int ProjectileCount, float Spread, bool IsBouncingNext, bool IsBouncingBack, float ZOffset, float Scale = 1.f);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Ability)
	bool UseAbilityQue = false;
};
