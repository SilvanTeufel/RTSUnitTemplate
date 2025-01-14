// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GAS/GAS.h"
#include "Core/UnitData.h"
#include "AbilitySystemInterface.h"
#include <GameplayEffectTypes.h>

#include "SpawnerUnit.h"
#include "GAS/AbilitySystemComponentBase.h"
#include "GAS/AttributeSetBase.h"
#include "GAS/GameplayAbilityBase.h"
#include "Containers/Queue.h"
#include "GASUnit.generated.h"

USTRUCT(BlueprintType)
struct FQueuedAbility
{
	GENERATED_BODY()
    
	// The actual GameplayAbility class to activate
	UPROPERTY()
	TSubclassOf<UGameplayAbilityBase> AbilityClass;

	// If your abilities need HitResult data (mouse hit, etc.), store it here
	UPROPERTY()
	FHitResult HitResult;
    
	// You can add anything else you need to queue, e.g. InputID, cost, etc.
};

UCLASS()
class RTSUNITTEMPLATE_API AGASUnit : public ASpawnerUnit, public IAbilitySystemInterface
{
	GENERATED_BODY()
	
public:
	//AGASUnit();
	
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category=RTSUnitTemplate, meta=(AllowPrivateAccess=true))
	class UAbilitySystemComponentBase* AbilitySystemComponent;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category=RTSUnitTemplate, meta=(AllowPrivateAccess=true))
	class UAttributeSetBase* Attributes;


	// A queue to store "next" abilities if the current one can't be activated or is still running
	TQueue<FQueuedAbility> AbilityQueue;
	
	// Callback when an ability ends so we can pop the next one from the queue
	UFUNCTION()
	void OnAbilityEnded(UGameplayAbility* EndedAbility);

	UFUNCTION()
	void ActivateNextQueuedAbility();
//protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
	virtual class UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintCallable, Category=RTSUnitTemplate)
	virtual void InitializeAttributes();

	UFUNCTION(BlueprintCallable, Category=RTSUnitTemplate)
	void CreateOwnerShip();
	
	UFUNCTION(BlueprintCallable, Category=RTSUnitTemplate)
	virtual void GiveAbilities();

	virtual void PossessedBy(AController* NewController) override;
	
	virtual void OnRep_PlayerState() override;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool ToggleUnitDetection = false;

	UFUNCTION(Server, Reliable, BlueprintCallable, meta = (DisplayName = "CreateCameraComp", Keywords = "RTSUnitTemplate CreateCameraComp"), Category = RTSUnitTemplate)
	void SetToggleUnitDetection(bool ToggleTo);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	bool GetToggleUnitDetection();
	
	UFUNCTION(BlueprintCallable, Category=RTSUnitTemplate)
	void ActivateAbilityByInputID(EGASAbilityInputID InputID, const TArray<TSubclassOf<UGameplayAbilityBase>>& AbilitiesArray, const FHitResult& HitResult = FHitResult());

	//UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	//void ServerActivateAbilityByInputID(EGASAbilityInputID InputID, const TArray<TSubclassOf<UGameplayAbilityBase>>& AbilitiesArray);


		
	UFUNCTION(BlueprintCallable, Category=RTSUnitTemplate)
	TSubclassOf<UGameplayAbility> GetAbilityForInputID(EGASAbilityInputID InputID, const TArray<TSubclassOf<UGameplayAbilityBase>>& AbilitiesArray);
	
	UPROPERTY(Replicated,BlueprintReadWrite, EditDefaultsOnly, Category=RTSUnitTemplate)
	TSubclassOf<class UGameplayEffect>DefaultAttributeEffect;

	UPROPERTY(Replicated, BlueprintReadWrite, EditDefaultsOnly, Category=Ability)
	TArray<TSubclassOf<UGameplayAbilityBase>>DefaultAbilities;

	/** A second set of abilities, could be unlocked later or via some condition */
	UPROPERTY(Replicated, BlueprintReadWrite , EditDefaultsOnly, Category=Ability)
	TArray<TSubclassOf<UGameplayAbilityBase>> SecondAbilities;

	/** A third set of abilities, potentially rare or harder to obtain */
	UPROPERTY(Replicated, BlueprintReadWrite, EditDefaultsOnly, Category=Ability)
	TArray<TSubclassOf<UGameplayAbilityBase>> ThirdAbilities;

	/** A fourth set of abilities, maybe ultimate or endgame powers */
	UPROPERTY(Replicated, BlueprintReadWrite, EditDefaultsOnly, Category=Ability)
	TArray<TSubclassOf<UGameplayAbilityBase>> FourthAbilities;
	
protected:
	// Function called when an ability is activated
	void OnAbilityActivated(UGameplayAbility* ActivatedAbility);

	// Setup delegates for ability system
	void SetupAbilitySystemDelegates();

public:
	// Reference to the activated ability instance
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category=Ability)
	UGameplayAbilityBase* ActivatedAbilityInstance;
};
