// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


#include "Characters/Unit/GASUnit.h"
#include "GAS/AttributeSetBase.h"
#include "GAS/AbilitySystemComponentBase.h"
#include "GAS/GameplayAbilityBase.h"
#include <GameplayEffectTypes.h>

/*
AGASUnit::AGASUnit()
{
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponentBase>("AbilitySystemComp");
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
	
	Attributes = CreateDefaultSubobject<UAttributeSetBase>("Attributes");
}
*/
// Called when the game starts or when spawned
void AGASUnit::BeginPlay()
{
	Super::BeginPlay();

}

// Called every frame
void AGASUnit::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AGASUnit::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// I guess this has to be done in the CameraBase
	/*
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
	}*/
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

void AGASUnit::ActivateAbilityByInputID(EGASAbilityInputID InputID)
{
	UE_LOG(LogTemp, Log, TEXT("ActivateAbilityByInputID! %d"), static_cast<int32>(InputID));
	if(AbilitySystemComponent)
	{
		TSubclassOf<UGameplayAbility> AbilityToActivate = GetAbilityForInputID(InputID);
		if(AbilityToActivate != nullptr)
		{
			UE_LOG(LogTemp, Log, TEXT("Activating ability for InputID %d"), static_cast<int32>(InputID));
			AbilitySystemComponent->TryActivateAbilityByClass(AbilityToActivate);
		}
	}
}

TSubclassOf<UGameplayAbility> AGASUnit::GetAbilityForInputID(EGASAbilityInputID InputID)
{
	UE_LOG(LogTemp, Log, TEXT("GetAbilityForInputID! %d"), static_cast<int32>(InputID));
	int32 AbilityIndex = static_cast<int32>(InputID) - static_cast<int32>(EGASAbilityInputID::None);

	// Log the calculated AbilityIndex
	UE_LOG(LogTemp, Log, TEXT("Calculated AbilityIndex: %d"), AbilityIndex);

	// Check if the AbilityIndex is valid in the DefaultAbilities array
	if (DefaultAbilities.IsValidIndex(AbilityIndex))
	{
		UE_LOG(LogTemp, Log, TEXT("Found ability at index %d for InputID %d"), AbilityIndex, static_cast<int32>(InputID));
		return DefaultAbilities[AbilityIndex];
	}
	else
	{
		// Log a warning if the index is not valid
		UE_LOG(LogTemp, Warning, TEXT("Invalid index: %d for InputID %d"), AbilityIndex, static_cast<int32>(InputID));
	}

	return nullptr;
}

