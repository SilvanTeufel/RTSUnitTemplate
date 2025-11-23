// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GAS.h"
#include "Abilities/GameplayAbility.h"
#include "Actors/WorkArea.h"
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
	bool AbilityCanBeCanceled = true;
	
	// New: Ability can be globally disabled via this flag (per ability asset)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool bDisabled = false;

	// New: Unique key to group abilities, default "None"
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FString AbilityKey = "None";
	
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

	// Override to prevent activation when disabled by flag or by team/key
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	bool UseAbilityQue = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	bool RotateToAbilityClick = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FString AbilityName = "Ability X: \n\n";
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FBuildingCost ConstructionCost = FBuildingCost{0, 0, 0, 0, 0, 0};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FString KeyboardKey = "X";

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FText ToolTipText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FText Type;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FText Description;
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void UpdateTooltipText();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int ClickCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool RotateToMouseWithMouseEvent = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float Range = 0.f;

	// Enable/Disable abilities by AbilityKey for the owner team (uses Owner from ActorInfo)
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetAbilitiesEnabledForTeamByKey(const FString& Key, bool bEnable);

	// Enable/Disable only this owner's ability by key (call from within an ability instance)
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetAbilityEnabledByKey(const FString& Key, bool bEnable);

		
	// Static helpers to toggle/query by key/team id
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	static void SetAbilitiesEnabledForTeamByKey_Static(const FString& Key, int32 TeamId, bool bEnable);
		
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	static bool IsAbilityKeyDisabledForTeam(const FString& Key, int32 TeamId);

	// Force-enable abilities by AbilityKey for the owner team (overrides bDisabled and disabled-by-key)
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetAbilitiesForceEnabledForTeamByKey(const FString& Key, bool bForceEnable);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	static void SetAbilitiesForceEnabledForTeamByKey_Static(const FString& Key, int32 TeamId, bool bForceEnable);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	static bool IsAbilityKeyForceEnabledForTeam(const FString& Key, int32 TeamId);

	// Owner-level queries (per AbilitySystemComponent)
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	static bool IsAbilityKeyDisabledForOwner(class UAbilitySystemComponent* OwnerASC, const FString& Key);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	static bool IsAbilityKeyForceEnabledForOwner(class UAbilitySystemComponent* OwnerASC, const FString& Key);

	// Apply owner-scoped ability key toggle on the local machine (client/UI side)
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	static void ApplyOwnerAbilityKeyToggle_Local(class UAbilitySystemComponent* OwnerASC, const FString& Key, bool bEnable);
		
	// Debug: dump disabled/force-enabled keys per team to log
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	static void Debug_DumpDisabledAbilityKeys();
	 
private:
	FText CreateTooltipText() const;
};
