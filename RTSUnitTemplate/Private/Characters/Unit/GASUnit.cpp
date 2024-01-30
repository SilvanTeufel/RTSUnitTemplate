// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Characters/Unit/GASUnit.h"
#include "GAS/AttributeSetBase.h"
#include "GAS/AbilitySystemComponentBase.h"
#include "GAS/GameplayAbilityBase.h"
#include "GAS/Gas.h"
#include "Abilities/GameplayAbilityTypes.h"
#include <GameplayEffectTypes.h>
#include "Engine/Engine.h"
#include "Characters/Unit/LevelUnit.h"
#include "Net/UnrealNetwork.h"


// Called when the game starts or when spawned
void AGASUnit::BeginPlay()
{
	Super::BeginPlay();
}

void AGASUnit::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGASUnit, AbilitySystemComponent);
	DOREPLIFETIME(AGASUnit, Attributes);
}

// Called every frame
void AGASUnit::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}


UAbilitySystemComponent* AGASUnit::GetAbilitySystemComponent() const
{
	return StaticCast<UAbilitySystemComponent*>(AbilitySystemComponent);
}

void AGASUnit::InitializeAttributes()
{
	if(AbilitySystemComponent && DefaultAttributeEffect)
	{
		FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
		EffectContext.AddSourceObject(this);

		// For level 1
		FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(DefaultAttributeEffect, 1, EffectContext);

		if(SpecHandle.IsValid())
		{
			FActiveGameplayEffectHandle GEHandle = AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		}
	}
}

void AGASUnit::GiveAbilities()
{
	if(HasAuthority() && AbilitySystemComponent)
	{
		for(TSubclassOf<UGameplayAbilityBase>& StartupAbility : DefaultAbilities)
		{
			AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(StartupAbility, 1, static_cast<int32>(StartupAbility.GetDefaultObject()->AbilityInputID), this));
		}
	}
}

void AGASUnit::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// Not sure if both is this
	AbilitySystemComponent->InitAbilityActorInfo(this, this);

	InitializeAttributes();
	GiveAbilities();
}


void AGASUnit::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	AbilitySystemComponent->InitAbilityActorInfo(this, this);

	InitializeAttributes();

	if (AbilitySystemComponent && InputComponent)
	{

		const FGameplayAbilityInputBinds Binds(
			"Confirm", 
			"Cancel", 
			FTopLevelAssetPath(GetPathNameSafe(UClass::TryFindTypeSlow<UEnum>("EGASAbilityInputID"))),
			static_cast<int32>(EGASAbilityInputID::Confirm),
			static_cast<int32>(EGASAbilityInputID::Cancel));
		/*
		const FGameplayAbilityInputBinds Binds(
			"Confirm", 
			"Cancel",
			"EGASAbilityInputID",
			static_cast<int32>(EGASAbilityInputID::Confirm),
			static_cast<int32>(EGASAbilityInputID::Cancel)
		);*/

		AbilitySystemComponent->BindAbilityActivationToInputComponent(InputComponent, Binds);


	}
}

/*
void AGASUnit::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// Not sure if both is this
	AbilitySystemComponent->InitAbilityActorInfo(this, this);

	InitializeAttributes();

	if(AbilitySystemComponent && InputComponent)
	{
		const FGameplayAbilityInputBinds Binds(
"Confirm", 
"Cancel",
"EGASAbilityInputID",
static_cast<int32>(EGASAbilityInputID::Confirm),
static_cast<int32>(EGASAbilityInputID::Cancel)
		);

		AbilitySystemComponent->BindAbilityActivationToInputComponent(InputComponent, Binds);
	}
	
}
*/

void AGASUnit::ActivateAbilityByInputID(EGASAbilityInputID InputID, const TArray<TSubclassOf<UGameplayAbilityBase>>& AbilitiesArray)
{
	if(AbilitySystemComponent)
	{
		TSubclassOf<UGameplayAbility> AbilityToActivate = GetAbilityForInputID(InputID, AbilitiesArray);
		if(AbilityToActivate != nullptr)
		{
			AbilitySystemComponent->TryActivateAbilityByClass(AbilityToActivate);
		}
	}
}

TSubclassOf<UGameplayAbility> AGASUnit::GetAbilityForInputID(EGASAbilityInputID InputID, const TArray<TSubclassOf<UGameplayAbilityBase>>& AbilitiesArray)
{
	int32 AbilityIndex = static_cast<int32>(InputID) - static_cast<int32>(EGASAbilityInputID::AbilityOne);

	// Check if the AbilityIndex is valid in the AbilitiesArray
	if (AbilitiesArray.IsValidIndex(AbilityIndex))
	{
		return AbilitiesArray[AbilityIndex];
	}

	return nullptr;
}