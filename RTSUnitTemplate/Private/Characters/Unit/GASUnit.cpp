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
#include "Controller/PlayerController/ControllerBase.h"
#include "Net/UnrealNetwork.h"


// Called when the game starts or when spawned
void AGASUnit::BeginPlay()
{
	Super::BeginPlay();
	CreateOwnerShip();
	AbilitySystemComponent->SetIsReplicated(true);
	//bReplicates = true;
}

void AGASUnit::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGASUnit, AbilitySystemComponent);
	DOREPLIFETIME(AGASUnit, Attributes);
	DOREPLIFETIME(AGASUnit, ToggleUnitDetection); // Added for BUild
	DOREPLIFETIME(AGASUnit, DefaultAttributeEffect);
	DOREPLIFETIME(AGASUnit, DefaultAbilities);

}


void AGASUnit::CreateOwnerShip()
{

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

	SetupAbilitySystemDelegates();
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


		AbilitySystemComponent->BindAbilityActivationToInputComponent(InputComponent, Binds);


	}
}


void AGASUnit::SetupAbilitySystemDelegates()
{
	//UE_LOG(LogTemp, Warning, TEXT("SetupAbilitySystemDelegates!"));
	if (AbilitySystemComponent)
	{
		// Register a delegate to be called when an ability is activated
		AbilitySystemComponent->AbilityActivatedCallbacks.AddUObject(this, &AGASUnit::OnAbilityActivated);
		//UE_LOG(LogTemp, Log, TEXT("SetupAbilitySystemDelegates: Delegate for ability activation registered successfully."));
	}
	else
	{
		// Log error if AbilitySystemComponent is null
		//UE_LOG(LogTemp, Warning, TEXT("SetupAbilitySystemDelegates: AbilitySystemComponent is null."));
	}
}

// This is your handler for when an ability is activated
void AGASUnit::OnAbilityActivated(UGameplayAbility* ActivatedAbility)
{
	//UE_LOG(LogTemp, Warning, TEXT("OnAbilityActivated! Here we set ActivatedAbiltiyInstance!"));
	// Assuming ActivatedAbilityInstance is a class member
	// Cast to UGameplayAbilityBase if necessary, depending on your class hierarchy
	ActivatedAbilityInstance = Cast<UGameplayAbilityBase>(ActivatedAbility);

	// Do something with the ActivatedAbilityInstance...
}

void AGASUnit::SetToggleUnitDetection_Implementation(bool ToggleTo)
{
	ToggleUnitDetection = ToggleTo;
}

bool AGASUnit::GetToggleUnitDetection()
{
	return ToggleUnitDetection;
}

void AGASUnit::ActivateAbilityByInputID(EGASAbilityInputID InputID, const TArray<TSubclassOf<UGameplayAbilityBase>>& AbilitiesArray)
{
	// Log the beginning of the function and which input was used
	FString CurrentTime = FDateTime::Now().ToString();
	UE_LOG(LogTemp, Warning, TEXT("[%s] ActivateAbilityByInputID called with InputID: %d"), *CurrentTime, static_cast<int32>(InputID));

	//if (HasAuthority())  // Ensure it only runs if the actor has server authority
	//{
		UE_LOG(LogTemp, Warning, TEXT("[%s] Running on server: HasAuthority() is true."), *CurrentTime);

		if (AbilitySystemComponent)
		{
			UE_LOG(LogTemp, Warning, TEXT("[%s] AbilitySystemComponent is valid."), *CurrentTime);

			TSubclassOf<UGameplayAbility> AbilityToActivate = GetAbilityForInputID(InputID, AbilitiesArray);
			if (AbilityToActivate != nullptr)
			{
				UE_LOG(LogTemp, Warning, TEXT("[%s] AbilityToActivate is valid. Trying to activate ability."), *CurrentTime);

				if (GetWorld() && GetWorld()->IsNetMode(NM_Client))
				{
					UE_LOG(LogTemp, Warning, TEXT("[%s] Running on client. Trying to activate ability by class."), *CurrentTime);
				}
				bool bActivated = AbilitySystemComponent->TryActivateAbilityByClass(AbilityToActivate);
				if (bActivated)
				{
					UE_LOG(LogTemp, Warning, TEXT("[%s] Ability activated successfully."), *CurrentTime);
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("[%s] Failed to activate ability."), *CurrentTime);
				}
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[%s] AbilityToActivate is nullptr. Unable to activate ability."), *CurrentTime);
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[%s] AbilitySystemComponent is nullptr. Cannot activate ability."), *CurrentTime);
		}
	//}
	//else  // Client sends RPC to the server to activate the ability
	//{
		//UE_LOG(LogTemp, Warning, TEXT("[%s] Running on client: HasAuthority() is false. Sending RPC to server."), *CurrentTime);
		//ServerActivateAbilityByInputID(InputID, AbilitiesArray);
	//}
}

void AGASUnit::ServerActivateAbilityByInputID_Implementation(EGASAbilityInputID InputID, const TArray<TSubclassOf<UGameplayAbilityBase>>& AbilitiesArray)
{
	// Log the beginning of the function and which input was used
	FString CurrentTime = FDateTime::Now().ToString();
	UE_LOG(LogTemp, Warning, TEXT("[%s] Server ActivateAbilityByInputID called with InputID: %d"), *CurrentTime, static_cast<int32>(InputID));

	if (HasAuthority())  // Ensure it only runs if the actor has server authority
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] Running on server: HasAuthority() is true."), *CurrentTime);

		if (AbilitySystemComponent)
		{
			UE_LOG(LogTemp, Warning, TEXT("[%s] AbilitySystemComponent is valid."), *CurrentTime);

			TSubclassOf<UGameplayAbility> AbilityToActivate = GetAbilityForInputID(InputID, AbilitiesArray);
			if (AbilityToActivate != nullptr)
			{
				UE_LOG(LogTemp, Warning, TEXT("[%s] AbilityToActivate is valid. Trying to activate ability."), *CurrentTime);

				if (GetWorld() && GetWorld()->IsNetMode(NM_Client))
				{
					UE_LOG(LogTemp, Warning, TEXT("[%s] Running on client. Trying to activate ability by class."), *CurrentTime);
				}
				bool bActivated = AbilitySystemComponent->TryActivateAbilityByClass(AbilityToActivate);
				if (bActivated)
				{
					UE_LOG(LogTemp, Warning, TEXT("[%s] Ability activated successfully."), *CurrentTime);
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("[%s] Failed to activate ability."), *CurrentTime);
				}
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[%s] AbilityToActivate is nullptr. Unable to activate ability."), *CurrentTime);
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[%s] AbilitySystemComponent is nullptr. Cannot activate ability."), *CurrentTime);
		}
	}
	else  // Client sends RPC to the server to activate the ability
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] Running on client: HasAuthority() is false. Sending RPC to server."), *CurrentTime);
		//ServerActivateAbilityByInputID(InputID, AbilitiesArray);
	}
}

/*
void AGASUnit::ActivateAbilityByInputID(EGASAbilityInputID InputID, const TArray<TSubclassOf<UGameplayAbilityBase>>& AbilitiesArray)
{
	if (HasAuthority())  // Ensure it only runs if the actor has server authority
	{
		if(AbilitySystemComponent)
		{
			TSubclassOf<UGameplayAbility> AbilityToActivate = GetAbilityForInputID(InputID, AbilitiesArray);
			if(AbilityToActivate != nullptr)
			{
				if (GetWorld() && GetWorld()->IsNetMode(NM_Client))UE_LOG(LogTemp, Warning, TEXT("TryActivateAbilityByClass!"));
				AbilitySystemComponent->TryActivateAbilityByClass(AbilityToActivate);
			}
		}
	}else  // Client sends RPC to the server to activate the ability
	{
		ServerActivateAbilityByInputID(InputID, AbilitiesArray);
	}
}

void AGASUnit::ServerActivateAbilityByInputID_Implementation(EGASAbilityInputID InputID, const TArray<TSubclassOf<UGameplayAbilityBase>>& AbilitiesArray)
{
	ActivateAbilityByInputID(InputID, AbilitiesArray);  // Execute on server
}
*/

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