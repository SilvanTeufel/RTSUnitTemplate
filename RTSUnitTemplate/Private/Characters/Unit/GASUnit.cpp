// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Characters/Unit/GASUnit.h"
#include "GameModes/RTSGameModeBase.h"
#include "GAS/AttributeSetBase.h"
#include "GAS/AbilitySystemComponentBase.h"
#include "GAS/GameplayAbilityBase.h"
#include "GAS/Gas.h"
#include "Abilities/GameplayAbilityTypes.h"
#include <GameplayEffectTypes.h>

#include "Characters/Unit/BuildingBase.h"
#include "Engine/Engine.h"
#include "Characters/Unit/LevelUnit.h"
#include "Controller/PlayerController/ControllerBase.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Engine/ActorChannel.h"
#include "GameModes/ResourceGameMode.h"


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
	DOREPLIFETIME(AGASUnit, AbilityQueueSize);
	DOREPLIFETIME(AGASUnit, MaxAbilityQueueSize);
}


// Called every frame
void AGASUnit::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (HasAuthority())
	{
		QueueFallbackTimer += DeltaTime;
		if (QueueFallbackTimer >= 1.0f)
		{
			QueueFallbackTimer = 0.f;
			if (!ActivatedAbilityInstance && !AbilityQueue.IsEmpty())
			{
				ActivateNextQueuedAbility();
			}
		}
	}
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
	UGameplayAbilityBase::ApplyActiveUpgradesToUnit(Cast<AUnitBase>(this));
}

void AGASUnit::GiveAbilities()
{
    // Ensure we are on the server and have a valid Ability System Component
    if (!HasAuthority() || !AbilitySystemComponent)
    {
        return;
    }

    // Grant abilities from all lists using our secure helper function
    GrantAbilitiesFromList(DefaultAbilities);
    GrantAbilitiesFromList(SecondAbilities);
    GrantAbilitiesFromList(ThirdAbilities);
    GrantAbilitiesFromList(FourthAbilities);
}

void AGASUnit::GrantAbilitiesFromList(const TArray<TSubclassOf<UGameplayAbilityBase>>& AbilityList)
{
    // Loop through the provided list of ability classes
    for (const TSubclassOf<UGameplayAbilityBase>& AbilityClass : AbilityList)
    {
        // 1. --- CRITICAL NULL CHECK ---
        // First, check if the AbilityClass itself is valid. This prevents crashes if an
        // array element is set to "None" in the Blueprint.
        if (!AbilityClass)
        {
            UE_LOG(LogTemp, Warning, TEXT("Found a null AbilityClass in an ability list for %s. Please check the Blueprint defaults."), *this->GetName());
            continue; // Skip to the next item in the list
        }

        // 2. --- GET CDO SAFELY ---
        // Get the Class Default Object to read properties like AbilityInputID.
        const UGameplayAbilityBase* AbilityCDO = AbilityClass->GetDefaultObject<UGameplayAbilityBase>();
        if (!AbilityCDO)
        {
            UE_LOG(LogTemp, Error, TEXT("Could not get CDO for AbilityClass %s on %s."), *AbilityClass->GetName(), *this->GetName());
            continue; // Skip if we can't get the CDO for some reason
        }
        
        // 3. --- CONSTRUCT SPEC and GIVE ABILITY ---
        // The tooltip text generation has been removed from the server code.
        FGameplayAbilitySpec AbilitySpec(
            AbilityClass,
            1, // Level
            static_cast<int32>(AbilityCDO->AbilityInputID),
            this // Source Object
        );
        
        AbilitySystemComponent->GiveAbility(AbilitySpec);
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
   FTopLevelAssetPath(GetPathNameSafe(UClass::TryFindTypeSlow<UEnum>(TEXT("EGASAbilityInputID")))), 
			static_cast<int32>(EGASAbilityInputID::Confirm),
			static_cast<int32>(EGASAbilityInputID::Cancel));


		AbilitySystemComponent->BindAbilityActivationToInputComponent(InputComponent, Binds);


	}
}

void AGASUnit::OnRep_ToggleUnitDetection()
{
	//UE_LOG(LogTemp, Warning, TEXT("OnRep_ToggleUnitDetection: %d"), ToggleUnitDetection);
}


void AGASUnit::SetupAbilitySystemDelegates()
{
	//UE_LOG(LogTemp, Warning, TEXT("SetupAbilitySystemDelegates!"));
	if (AbilitySystemComponent)
	{
		// Register a delegate to be called when an ability is activated
		AbilitySystemComponent->AbilityActivatedCallbacks.AddUObject(this, &AGASUnit::OnAbilityActivated);
		AbilitySystemComponent->AbilityEndedCallbacks.AddUObject(this, &AGASUnit::OnAbilityEnded);
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
	ActivatedAbilityInstance = Cast<UGameplayAbilityBase>(ActivatedAbility);
}

void AGASUnit::SetHealth_Implementation(float NewHealth)
{
	if (Attributes)
	{
		Attributes->SetAttributeHealth(NewHealth);
	}
}

void AGASUnit::SetShield_Implementation(float NewShield)
{
	if (Attributes)
	{
		Attributes->SetAttributeShield(NewShield);
	}
}

void AGASUnit::SetToggleUnitDetection_Implementation(bool ToggleTo)
{
	ToggleUnitDetection = ToggleTo;
}

bool AGASUnit::GetToggleUnitDetection()
{
	return ToggleUnitDetection;
}


bool AGASUnit::ActivateAbilityByInputID(
	EGASAbilityInputID InputID,
	const TArray<TSubclassOf<UGameplayAbilityBase>>& AbilitiesArray,
	const FHitResult& HitResult,
	APlayerController* InstigatorPC)
{
	
	if (!AbilitySystemComponent)
	{
		return false;
	}

	TSubclassOf<UGameplayAbility> AbilityToActivate = GetAbilityForInputID(InputID, AbilitiesArray);

	if (!AbilityToActivate)
	{
		return false;
	}
	
	UGameplayAbilityBase* Ability;


	if (UGameplayAbilityBase* AbilityB = AbilityToActivate->GetDefaultObject<UGameplayAbilityBase>())
		Ability = AbilityB;
	else
	{
		return false;
	}
	
	if (ActivatedAbilityInstance)
	{
		if (Ability->UseAbilityQue && AbilityQueueSize < MaxAbilityQueueSize)
		{
			// ASC is busy, so let's queue the ability
			FQueuedAbility Queued;
			Queued.AbilityClass = AbilityToActivate;
			Queued.HitResult    = HitResult;
			Queued.InstigatorPC = InstigatorPC;
			QueSnapshot.Add(Queued);
			AbilityQueue.Enqueue(Queued);
			AbilityQueueSize = QueSnapshot.Num();
		}else
		{
			FireMouseHitAbility(HitResult);
		}
		return false;
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
			Queued.InstigatorPC = InstigatorPC;
			CurrentSnapshot = Queued;
			CurrentInstigatorPC = InstigatorPC;
		}
		if (bIsActivated && HitResult.IsValidBlockingHit())
		{
			if (ActivatedAbilityInstance) 
			{
				FireMouseHitAbility(HitResult);
			}
		}
		else if (!bIsActivated)
		{
			if (Ability->UseAbilityQue && AbilityQueueSize < MaxAbilityQueueSize)
			{
				// Optionally queue the ability if activation fails 
				// (e.g. on cooldown). Depends on your desired flow.
				FQueuedAbility Queued;
				Queued.AbilityClass = AbilityToActivate;
				Queued.HitResult    = HitResult;
				Queued.InstigatorPC = InstigatorPC;
				QueSnapshot.Add(Queued);
				AbilityQueue.Enqueue(Queued);
				AbilityQueueSize = QueSnapshot.Num();

				if (!ActivatedAbilityInstance)
				{
					const float DelayTime = 0.1f;
					FTimerHandle TimerHandle;
					GetWorld()->GetTimerManager().SetTimer(
						TimerHandle,
						this,
						&AGASUnit::ActivateNextQueuedAbility,
						DelayTime,
						false
					);
				}
			}
		}

		return bIsActivated;
	}
}

void AGASUnit::OnAbilityEnded(UGameplayAbility* EndedAbility)
{
	if (ActivatedAbilityInstance == EndedAbility)
	{
		ActivatedAbilityInstance = nullptr;
	}

	if (CurrentSnapshot.AbilityClass == EndedAbility->GetClass())
	{
		CurrentSnapshot = FQueuedAbility();
		CurrentInstigatorPC = nullptr;
	}

	// Example: delay by half a second
	if (ActivatedAbilityInstance == nullptr)
	{
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

	if (UGameplayAbilityBase* AbilityBase = Cast<UGameplayAbilityBase>(EndedAbility))
	{
		AbilityBase->ClickCount = 0;
	}
}

void AGASUnit::ActivateNextQueuedAbility()
{
	if (!HasAuthority()) return;

	if (ActivatedAbilityInstance)
	{
		return;
	}

	// 1) Check if there's something waiting in the queue
	if (!AbilityQueue.IsEmpty())
	{
		FQueuedAbility Next;
		bool bDequeued = AbilityQueue.Dequeue(Next);

		if (bDequeued && AbilitySystemComponent)
		{
			if (QueSnapshot.Num() > 0)
			{
				QueSnapshot.RemoveAt(0);
			}
			AbilityQueueSize = QueSnapshot.Num();
			
			// 2) Activate the next queued ability
			bool bIsActivated = AbilitySystemComponent->TryActivateAbilityByClass(Next.AbilityClass);

			if (bIsActivated)
			{
				CurrentSnapshot = Next;
				CurrentInstigatorPC = Next.InstigatorPC.Get();

				if (Next.HitResult.IsValidBlockingHit())
				{
					if (ActivatedAbilityInstance)
					{
						FireMouseHitAbility(Next.HitResult);
					}
					else
					{
						CancelCurrentAbility();
					}
				}
			}
			else
			{
				// If activation failed (e.g. cooldown), we drop it and try the next one
				CancelCurrentAbility();

				if (!AbilityQueue.IsEmpty())
				{
					const float DelayTime = 0.1f;
					FTimerHandle TimerHandle;
					GetWorld()->GetTimerManager().SetTimer(
						TimerHandle,
						this,
						&AGASUnit::ActivateNextQueuedAbility,
						DelayTime,
						false
					);
				}
			}
		}
		else
		{
			DequeueAbility(0);
		}
	}
	else
	{
		CancelCurrentAbility();
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

FVector AGASUnit::GetMassActorLocation() const
{
	return GetActorLocation();
}

void AGASUnit::FireMouseHitAbility(const FHitResult& InHitResult)
{
	if (ActivatedAbilityInstance)
	{
		if (const UWorld* World = GetWorld())
		{
			if (UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>())
			{
				FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
				AUnitBase* ThisUnit = Cast<AUnitBase>(this);
				const FMassEntityHandle EntityHandle = ThisUnit->MassActorBindingComponent->GetEntityHandle();

				if (EntityManager.IsEntityValid(EntityHandle))
				{
					FMassAITargetFragment* TargetFragment = EntityManager.GetFragmentDataPtr<FMassAITargetFragment>(EntityHandle);
					if (TargetFragment)
					{
						TargetFragment->AbilityTargetLocation = InHitResult.Location;
					}
				}
			}
		}

		FVector ALocation = GetMassActorLocation();
		
		float Distance = FVector::Dist(InHitResult.Location, ALocation);

		if (ActivatedAbilityInstance->Range == 0.f || Distance <= ActivatedAbilityInstance->Range || ActivatedAbilityInstance->ClickCount == 0)
		{
			ActivatedAbilityInstance->ClickCount++;
			ActivatedAbilityInstance->OnAbilityMouseHit(InHitResult);
		}else
		{
			CancelCurrentAbility();
		}
	}
	
}

bool AGASUnit::DequeueAbility(int Index)
{
	if (!HasAuthority())
	{
		return false;
	}

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
	AbilityQueueSize = QueSnapshot.Num();
	
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
		{
			if(CurrentDraggedAbilityIndicator)
			{
				CurrentDraggedAbilityIndicator->Destroy(true, true);
				CurrentDraggedAbilityIndicator = nullptr;
			}

			ActivatedAbilityInstance->ClickCount = 0;
			ActivatedAbilityInstance->K2_CancelAbility();
			ActivatedAbilityInstance = nullptr;
		}
	}
	CurrentSnapshot = FQueuedAbility();
	CurrentInstigatorPC = nullptr;
}