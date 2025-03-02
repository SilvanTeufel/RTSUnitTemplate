// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Characters/Unit/GASUnit.h"
#include "GameModes/RTSGameModeBase.h"
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
#include "Engine/ActorChannel.h"


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
	DOREPLIFETIME(AGASUnit, ToggleUnitDetection); // Added for BUild
	DOREPLIFETIME(AGASUnit, DefaultAttributeEffect);
	DOREPLIFETIME(AGASUnit, DefaultAbilities);
	DOREPLIFETIME(AGASUnit, SecondAbilities);
	DOREPLIFETIME(AGASUnit, ThirdAbilities);
	DOREPLIFETIME(AGASUnit, FourthAbilities);
	DOREPLIFETIME(AGASUnit, QueSnapshot);
	DOREPLIFETIME(AGASUnit, CurrentSnapshot);
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
			StartupAbility.GetDefaultObject()->UpdateTooltipText();
			AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(StartupAbility, 1, static_cast<int32>(StartupAbility.GetDefaultObject()->AbilityInputID), this));
		}

		for(TSubclassOf<UGameplayAbilityBase>& StartupAbility :SecondAbilities)
		{
			StartupAbility.GetDefaultObject()->UpdateTooltipText();
			AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(StartupAbility, 1, static_cast<int32>(StartupAbility.GetDefaultObject()->AbilityInputID), this));
		}
		
		for(TSubclassOf<UGameplayAbilityBase>& StartupAbility :ThirdAbilities)
		{
			StartupAbility.GetDefaultObject()->UpdateTooltipText();
			AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(StartupAbility, 1, static_cast<int32>(StartupAbility.GetDefaultObject()->AbilityInputID), this));
		}

		for(TSubclassOf<UGameplayAbilityBase>& StartupAbility :FourthAbilities)
		{
			StartupAbility.GetDefaultObject()->UpdateTooltipText();
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

void AGASUnit::OnRep_ToggleUnitDetection()
{
	UE_LOG(LogTemp, Warning, TEXT("OnRep_ToggleUnitDetection: %d"), ToggleUnitDetection);
}


void AGASUnit::SetupAbilitySystemDelegates()
{
	//UE_LOG(LogTemp, Warning, TEXT("SetupAbilitySystemDelegates!"));
	if (AbilitySystemComponent)
	{
		// Register a delegate to be called when an ability is activated
		AbilitySystemComponent->AbilityActivatedCallbacks.AddUObject(this, &AGASUnit::OnAbilityActivated);
		AbilitySystemComponent->AbilityEndedCallbacks.AddUObject(this, &AGASUnit::OnAbilityEnded);
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
	//UE_LOG(LogTemp, Warning, TEXT("OnAbilityActivated: %s"), *ActivatedAbility->GetName());

	ActivatedAbilityInstance = Cast<UGameplayAbilityBase>(ActivatedAbility);

}

void AGASUnit::SetToggleUnitDetection_Implementation(bool ToggleTo)
{
	ToggleUnitDetection = ToggleTo;
}

bool AGASUnit::GetToggleUnitDetection()
{
	return ToggleUnitDetection;
}


void AGASUnit::ActivateAbilityByInputID(
	EGASAbilityInputID InputID,
	const TArray<TSubclassOf<UGameplayAbilityBase>>& AbilitiesArray,
	const FHitResult& HitResult)
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	TSubclassOf<UGameplayAbility> AbilityToActivate = GetAbilityForInputID(InputID, AbilitiesArray);

	if (!AbilityToActivate)
	{
		return;
	}
	
	UGameplayAbilityBase* Ability;


	if (UGameplayAbilityBase* AbilityB = AbilityToActivate->GetDefaultObject<UGameplayAbilityBase>())
		Ability = AbilityB;
	else
	{
		return;
	}
	
	if (ActivatedAbilityInstance)
	{
		if (Ability->UseAbilityQue)
		{
			// ASC is busy, so let's queue the ability
			FQueuedAbility Queued;
			Queued.AbilityClass = AbilityToActivate;
			Queued.HitResult    = HitResult;
			QueSnapshot.Add(Queued);
			AbilityQueue.Enqueue(Queued);
		}
	}
	else
	{
		// 2) Try to activate
		bool bIsActivated = AbilitySystemComponent->TryActivateAbilityByClass(AbilityToActivate);
		if (bIsActivated)
		{
			// If you have a pointer to the active ability instance:
			FQueuedAbility Queued;
			Queued.AbilityClass = AbilityToActivate;
			Queued.HitResult    = HitResult;
			CurrentSnapshot = Queued;
		}
		if (bIsActivated && HitResult.IsValidBlockingHit())
		{
			if (ActivatedAbilityInstance) 
			{
				ActivatedAbilityInstance->OnAbilityMouseHit(HitResult);
				//CurrentAbilityCanBeCanceled = ActivatedAbilityInstance->AbilityCanBeCanceled;
			}
		}
		else if (!bIsActivated)
		{
			if (Ability->UseAbilityQue)
			{
				// Optionally queue the ability if activation fails 
				// (e.g. on cooldown). Depends on your desired flow.
				FQueuedAbility Queued;
				Queued.AbilityClass = AbilityToActivate;
				Queued.HitResult    = HitResult;
				QueSnapshot.Add(Queued);
				AbilityQueue.Enqueue(Queued);
			}
		}
	}
}

void AGASUnit::OnAbilityEnded(UGameplayAbility* EndedAbility)
{
	//UE_LOG(LogTemp, Warning, TEXT("OnAbilityEnded: %s"), *EndedAbility->GetName());
		// Example: delay by half a second
		const float DelayTime = 0.1f; 
		FTimerHandle TimerHandle;
		GetWorld()->GetTimerManager().SetTimer(
			TimerHandle,
			this, 
			&AGASUnit::ActivateNextQueuedAbility, 
			DelayTime, 
			/*bLoop=*/false
		);


}

void AGASUnit::ActivateNextQueuedAbility()
{
	// 1) Check if there's something waiting in the queue
	if (!AbilityQueue.IsEmpty())
	{
		FQueuedAbility Next;
		bool bDequeued = AbilityQueue.Dequeue(Next);
		if (bDequeued && AbilitySystemComponent)
		{
			//UE_LOG(LogTemp, Warning, TEXT("Dequeued next ability: %s"), *Next.AbilityClass->GetName());
			
			CurrentSnapshot = Next;


			
			QueSnapshot.Remove(Next);
			ActivatedAbilityInstance = nullptr;
			// 2) Activate the next queued ability
			bool bIsActivated = AbilitySystemComponent->TryActivateAbilityByClass(Next.AbilityClass);
			//if (bIsActivated) UE_LOG(LogTemp, Warning, TEXT("Activation Succeeded : %s"), *Next.AbilityClass->GetName());
			if (bIsActivated && Next.HitResult.IsValidBlockingHit())
			{
				
				if (ActivatedAbilityInstance)
				{
					ActivatedAbilityInstance->OnAbilityMouseHit(Next.HitResult);
					//CurrentAbilityCanBeCanceled = ActivatedAbilityInstance->AbilityCanBeCanceled;
				}
			}
			else
			{
				ActivatedAbilityInstance = nullptr;
				CurrentSnapshot = FQueuedAbility();
				// Optionally: If we fail again (cooldown, cost, or still "busy"), 
				// do you want to re-queue it or just discard?
				// AbilityQueue.Enqueue(Next); // if you want to retry
			}
		}
	}else
	{
		ActivatedAbilityInstance = nullptr;
		CurrentSnapshot = FQueuedAbility();
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

bool AGASUnit::DequeueAbility(int Index)
{
	TArray<FQueuedAbility> TempArray;
	FQueuedAbility TempItem;

	// 1) Transfer all items from the queue into the array
	while (AbilityQueue.Dequeue(TempItem))
	{
		TempArray.Add(TempItem);
	}

	// 2) Remove item at 'Index'
	bool bRemoved = false;
	if (TempArray.IsValidIndex(Index))
	{
		TempArray.RemoveAt(Index);
		bRemoved = true;
	}

	// 3) Rebuild the queue without the removed item
	for (const FQueuedAbility& Item : TempArray)
	{
		AbilityQueue.Enqueue(Item);
	}

	// Also update your 'QueSnapshot' if you’re mirroring
	QueSnapshot = TempArray;

	return bRemoved;
}

const TArray<FQueuedAbility>& AGASUnit::GetQueuedAbilities()
{
	return QueSnapshot;
}


const FQueuedAbility AGASUnit::GetCurrentSnapshot()
{
	return CurrentSnapshot;
}

void AGASUnit::CancelCurrentAbility()
{
    // Check if this code is executing on a client.
    if (!HasAuthority())
    {
        return;
    }

	if (ActivatedAbilityInstance)
	{
		// Check if the active ability can be canceled.
		if (ActivatedAbilityInstance->AbilityCanBeCanceled)
		{
			ActivatedAbilityInstance->K2_CancelAbility();
			ActivatedAbilityInstance = nullptr;
			CurrentSnapshot = FQueuedAbility();
		}
	}
}
