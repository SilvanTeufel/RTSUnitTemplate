// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Characters/Unit/GASUnit.h"
#include "GameModes/RTSGameModeBase.h"
#include "GAS/AttributeSetBase.h"
#include "GAS/AbilitySystemComponentBase.h"
#include "GAS/GameplayAbilityBase.h"
#include "GAS/Gas.h"
#include "GameplayAbilitySpec.h"
#include "Abilities/GameplayAbilityTypes.h"
#include <GameplayEffectTypes.h>

#include "Characters/Unit/BuildingBase.h"
#include "Characters/Unit/MassUnitBase.h"
#include "Mass/UnitMassTag.h"
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
	DOREPLIFETIME(AGASUnit, ReplicatedAbilityCosts);
}


// Called every frame
void AGASUnit::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (HasAuthority())
	{
		CastInvariantTimer += DeltaTime;
		if (CastInvariantTimer >= 0.25f)
		{
			CastInvariantTimer = 0.f;
			EnforceCastingInvariant();
		}

		QueueFallbackTimer += DeltaTime;
		if (QueueFallbackTimer >= 1.0f)
		{
			QueueFallbackTimer = 0.f;
			ClearStaleActivatedAbility();
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
            continue; // Skip to the next item in the list
        }

        // 2. --- GET CDO SAFELY ---
        // Get the Class Default Object to read properties like AbilityInputID.
        const UGameplayAbilityBase* AbilityCDO = AbilityClass->GetDefaultObject<UGameplayAbilityBase>();
        if (!AbilityCDO)
        {
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
	LastAbilitySafetyWindowTime = 0.f;

	if (ActivatedAbilityInstance)
	{
		ActivatedAbilityInstance->ClickCount = 0;
	}

	// If on server and CurrentSnapshot hasn't been set yet (e.g., for AI or auto-abilities),
	// we set the AbilityClass so clients know an ability is active.
	if (HasAuthority() && ActivatedAbilityInstance && !CurrentSnapshot.AbilityClass)
	{
		CurrentSnapshot.AbilityClass = ActivatedAbilityInstance->GetClass();
		CurrentSnapshot.ClickCount = 0;
	}
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

void AGASUnit::SetMana_Implementation(float NewMana)
{
	if (Attributes)
	{
		Attributes->SetAttributeMana(NewMana);
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


bool AGASUnit::IsAbilityOnCooldownByClass(TSubclassOf<UGameplayAbilityBase> AbilityClass) const
{
	if (!AbilitySystemComponent || !AbilityClass)
	{
		return false;
	}

	const UGameplayAbilityBase* AbilityCDO = AbilityClass.GetDefaultObject();
	if (!AbilityCDO)
	{
		return false;
	}

	// Try to find the spec for this ability
	FGameplayAbilitySpec* Spec = AbilitySystemComponent->FindAbilitySpecFromClass(AbilityClass);
	if (Spec)
	{
		const FGameplayAbilityActorInfo* ActorInfo = AbilitySystemComponent->AbilityActorInfo.Get();
		if (ActorInfo)
		{
			// Check if there are instances of this ability
			TArray<UGameplayAbility*> Instances = Spec->GetAbilityInstances();
			if (Instances.Num() > 0)
			{
				// Use the instance for the cooldown check (especially important for instanced per actor abilities)
				// CheckCooldown returns true if the ability can be activated (i.e., not on cooldown)
				return !Instances[0]->CheckCooldown(Spec->Handle, ActorInfo);
			}

			// Fallback to calling CheckCooldown on the CDO, which is safe for ShootAbility as it uses ActorInfo
			return !AbilityCDO->CheckCooldown(Spec->Handle, ActorInfo);
		}
	}

	// Last resort: standard tag check if no spec or actor info is available
	const FGameplayTagContainer* CooldownTags = AbilityCDO->GetCooldownTags();
	if (CooldownTags && CooldownTags->Num() > 0)
	{
		if (AbilitySystemComponent->HasAnyMatchingGameplayTags(*CooldownTags))
		{
			return true;
		}
	}

	return false;
}


bool AGASUnit::IsAnyAbilityActive() const
{
	// ActivatedAbilityInstance works on Server.
	// CurrentSnapshot.AbilityClass works on Clients because it is replicated.
	if (ActivatedAbilityInstance != nullptr || CurrentSnapshot.AbilityClass != nullptr)
	{
		return true;
	}

	// Tolerance window for clients waiting for replication
	if (GetWorld() && (GetWorld()->GetTimeSeconds() - LastAbilitySafetyWindowTime < AbilityReplicationTolerance))
	{
		return true;
	}

	return false;
}


void AGASUnit::EnforceCastingInvariant()
{
	AUnitBase* SelfUnit = Cast<AUnitBase>(this);
	AMassUnitBase* MassSelf = Cast<AMassUnitBase>(this);
	if (!SelfUnit || !MassSelf) return;

	// bBlueprintCastActive zaehlt gleichwertig: Abilities, die ihren Cast selbst im Blueprint starten
	// (erst laufen, dann casten), melden sich ueber AddCastingFallback genau in dem Moment an, in dem
	// der Cast beginnt. Ohne das galt ihr Cast als verwaist und wurde nach zwei Takten geloest -
	// GA_Mine_AH starb dadurch reproduzierbar bei etwa 20 Prozent der Cast-Zeit.
	const bool bCastAbilityActive =
		ActivatedAbilityInstance
		&& (ActivatedAbilityInstance->bUseCastingFallbackProcessor
			|| ActivatedAbilityInstance->bBlueprintCastActive)
		&& ActivatedAbilityInstance->IsActive();

	// Tote Einheiten sind ausgenommen. Ohne diese Pruefung wird eine Ability, die den Tod ihres
	// Traegers ueberlebt, alle 0,5 s erneut "nachgezogen": SwitchEntityTag(Casting) auf einem toten
	// Aktor. Gemessen am 19.08. an einem einzelnen Singularianer-DataCenter - 4094 Korrekturen in
	// EINER Partie, alle mit Zustand=36 (Dead), waehrend andere Partien bei 50 lagen. Der Zustand in
	// der Logzeile war der Hinweis; ohne ihn sah es nach Rauschen aus.
	if (SelfUnit->GetUnitState() == UnitData::Dead)
	{
		CastInvariantStrikes = 0;
		return;
	}

	// Laeuft ueberhaupt eine Ability? Unabhaengig von den Fallback-Kennzeichen.
	//
	// Regel 2 loeste den Cast bisher auf, sobald WEDER bUseCastingFallbackProcessor NOCH
	// bBlueprintCastActive gesetzt war - auch wenn die Ability laut GAS lief. Abilities, die ihren
	// Cast im Eltern-Blueprint starten und sich nicht ueber AddCastingFallback anmelden, wurden
	// dadurch nach zwei Durchlaeufen (rund 1 s) aus dem Casting geworfen. Gemessen am 20.08. an
	// GA_Projectile_Snipershot_Hero_AH: castTime 1,0 stirbt genau daran, waehrend Multishot mit 0,5
	// vorher fertig ist - deshalb sah es nach einem Unterschied zwischen den Abilities aus.
	//
	// Regel 1 (INS Casting zwingen) bleibt bewusst an den Kennzeichen: nur wer sich anmeldet, wird
	// nachgezogen. Hier geht es allein darum, einen laufenden Cast nicht abzuwuergen.
	const bool bIrgendeineAbilityAktiv = ActivatedAbilityInstance && ActivatedAbilityInstance->IsActive();

	const bool bImCasting = (SelfUnit->GetUnitState() == UnitData::Casting);

	// Regel 1: aktive Cast-Ability ohne Casting-Zustand.
	// Regel 2: kein aktiver Cast, Einheit haengt trotzdem im Casting.
	const bool bVerstoss = (bCastAbilityActive && !bImCasting) || (!bIrgendeineAbilityAktiv && bImCasting);

	if (!bVerstoss)
	{
		CastInvariantStrikes = 0;
		return;
	}

	// Ein einzelner Durchlauf kann ein legitimes Umschaltfenster sein (Aktivierung laeuft gerade,
	// EndCast ist unterwegs). Erst zwei Treffer in Folge gelten als Verstoss.
	if (++CastInvariantStrikes < 2) return;
	CastInvariantStrikes = 0;

	if (bCastAbilityActive)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CastInvariant] %s: '%s' laeuft, Einheit castet aber NICHT (Zustand=%d) - wird nachgezogen"),
			*GetName(), *GetNameSafe(ActivatedAbilityInstance), (int32)SelfUnit->GetUnitState());
		MassSelf->SwitchEntityTag(FMassStateCastingTag::StaticStruct());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[CastInvariant] %s: keine Cast-Ability aktiv, Einheit haengt im Casting - wird geloest"),
			*GetName());
		MassSelf->SwitchEntityTag(FMassStateIdleTag::StaticStruct());
	}
}

void AGASUnit::ClearStaleActivatedAbility()
{
	// Self-heal for a stranded ActivatedAbilityInstance.
	//
	// The pointer is normally cleared by OnAbilityEnded. That callback only fires when GAS really ends
	// the ability - and UGameplayAbilityBase::EndAbility returns early via IsEndAbilityValid() when the
	// Blueprint ends itself DURING activation. GA_BuildUnit_Parent_AH does exactly that on its failure
	// paths ("NOT ENOUGH RESOURCES", "already casting", "cannot build while flying"). The instance was
	// then finished but still registered here, and ActivateAbilityByInputID refuses every further
	// ability while one is registered - so a single failed press locked the unit out of ALL abilities
	// for the rest of the match. Measured on BP_BuildingBase_Singularian_DataCenter_C_1: four presses
	// in a row logged "busy, 'GA_BuildUnit_EchoNode_AH_C_0' still active" with the queue filling up.
	if (ActivatedAbilityInstance && !ActivatedAbilityInstance->IsActive())
	{
		ActivatedAbilityInstance = nullptr;
	}
}

bool AGASUnit::ActivateAbilityByInputID(
	EGASAbilityInputID InputID,
	const TArray<TSubclassOf<UGameplayAbilityBase>>& AbilitiesArray,
	const FHitResult& HitResult,
	APlayerController* InstigatorPC)
{
	
	// [AbilityAktivierung] Diese Funktion hatte fuenf stille Ausstiege. Gemessen am 18.08.: von 32
	// gefeuerten LaravalPod-Regeln wurden in einer Verlustpartie nur 20 zu einem Bauplatz, in der
	// Siegpartie alle 34 - die Differenz verschwand genau hier, ohne eine einzige Logzeile. Der
	// Kommentar weiter unten ("Name it instead of failing silently") zeigt, dass die Zeilen
	// vorgesehen waren; geschrieben wurden sie nie.
	if (!AbilitySystemComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AbilityAktivierung] %s (Team %d) InputID=%d ABGELEHNT: kein AbilitySystemComponent"),
			*GetName(), TeamId, (int32)InputID);
		return false;
	}

	ClearStaleActivatedAbility();

	TSubclassOf<UGameplayAbility> AbilityToActivate = GetAbilityForInputID(InputID, AbilitiesArray);

	if (!AbilityToActivate)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AbilityAktivierung] %s (Team %d) InputID=%d ABGELEHNT: kein Eintrag an diesem Index (Array=%d)"),
			*GetName(), TeamId, (int32)InputID, AbilitiesArray.Num());
		return false;
	}
	
	UGameplayAbilityBase* Ability;


	if (UGameplayAbilityBase* AbilityB = AbilityToActivate->GetDefaultObject<UGameplayAbilityBase>())
		Ability = AbilityB;
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[AbilityAktivierung] %s (Team %d) InputID=%d ABGELEHNT: %s ist keine UGameplayAbilityBase"),
			*GetName(), TeamId, (int32)InputID, *GetNameSafe(AbilityToActivate));
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
			Queued.InputID      = InputID;
			QueSnapshot.Add(Queued);
			AbilityQueue.Enqueue(Queued);
			AbilityQueueSize = QueSnapshot.Num();
		}
		// A stale ActivatedAbilityInstance locks the unit out of EVERY ability, which looks exactly like
		// "casting does not start any more". Name it instead of failing silently.
		const AUnitBase* AlsUnitBusy = Cast<AUnitBase>(this);
		UE_LOG(LogTemp, Warning,
			TEXT("[AbilityAktivierung] %s (Team %d) InputID=%d ABGELEHNT: '%s' laeuft noch (Zustand=%d, Warteschlange=%d/%d)"),
			*GetName(), TeamId, (int32)InputID, *GetNameSafe(ActivatedAbilityInstance),
			AlsUnitBusy ? (int32)AlsUnitBusy->GetUnitState() : -1, AbilityQueueSize, MaxAbilityQueueSize);
		return false;
	}
	else
	{
		// 2) Try to activate
		CurrentInstigatorPC = InstigatorPC;

		bool bIsActivated = AbilitySystemComponent->TryActivateAbilityByClass(AbilityToActivate);
		if (!bIsActivated)
		{
			// GAS nennt keinen Grund. Der Zustand ist der aussagekraeftigste Hinweis, den es hier
			// gibt: steht er auf Casting (7), war es der Riegel in
			// UGameplayAbilityBase::CanActivateAbility, der waehrend eines laufenden Casts JEDE
			// weitere Aktivierung dieser Einheit ablehnt. Sonst bleiben Kosten und Abklingzeit.
			const AUnitBase* AlsUnitGas = Cast<AUnitBase>(this);
			UE_LOG(LogTemp, Warning,
				TEXT("[AbilityAktivierung] %s (Team %d) InputID=%d ABGELEHNT von GAS: %s (Zustand=%d)"),
				*GetName(), TeamId, (int32)InputID, *GetNameSafe(AbilityToActivate),
				AlsUnitGas ? (int32)AlsUnitGas->GetUnitState() : -1);
		}
		else
		{
			// Erfolgsfall benennen (02.09.2026): der Klassenname stand bisher nur im
			// Ablehnungszweig. Fuer die Auswertung "welche Faehigkeit erzeugt eine Baustelle"
			// wird er aber genau hier gebraucht - zusammen mit [NetzDruck] ... Geist=0/1.
			// Zwei geratene Maskenkriterien waren wirkungslos; das dritte soll aus dieser
			// Paarung folgen statt geraten zu werden.
			UE_LOG(LogTemp, Log,
				TEXT("[AbilityAktivierung] %s (Team %d) InputID=%d AKTIVIERT: %s"),
				*GetName(), TeamId, (int32)InputID, *GetNameSafe(AbilityToActivate));
		}
		if (bIsActivated && ActivatedAbilityInstance)
		{
			ActivatedAbilityInstance->AbilityInputID = InputID;

			if (HasAuthority())
			{
				// If you have a pointer to the active ability instance:
				FQueuedAbility Queued;
				Queued.AbilityClass = AbilityToActivate;
				Queued.HitResult    = HitResult;
				Queued.InstigatorPC = InstigatorPC;
				Queued.InputID      = InputID;
				CurrentSnapshot = Queued;
			}
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
				Queued.InputID      = InputID;
				QueSnapshot.Add(Queued);
				AbilityQueue.Enqueue(Queued);
				AbilityQueueSize = QueSnapshot.Num();

				if (!ActivatedAbilityInstance)
				{
					const float DelayTime = AbilityReactivationThrottle;
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

	// Failsafe: if no instance is active, the snapshot must be empty.
	if (HasAuthority() && !ActivatedAbilityInstance && CurrentSnapshot.AbilityClass)
	{
		CurrentSnapshot = FQueuedAbility();
		CurrentInstigatorPC = nullptr;
	}

	// Example: delay by half a second
	if (ActivatedAbilityInstance == nullptr)
	{
		const float DelayTime = AbilityReactivationThrottle;
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

	// Without this the queue drains never again once a finished instance is stranded - the entries
	// pile up (measured: queued=4) and nothing ever runs.
	ClearStaleActivatedAbility();

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
			CurrentInstigatorPC = Next.InstigatorPC.Get();

			bool bIsActivated = AbilitySystemComponent->TryActivateAbilityByClass(Next.AbilityClass);

			if (bIsActivated && ActivatedAbilityInstance)
			{
				ActivatedAbilityInstance->AbilityInputID = Next.InputID;
				CurrentSnapshot = Next;

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
				// Activation failed. By far the most common cause is the ability still being on
				// cooldown right at the moment the previous one finished - and Dequeue() above has
				// already taken this entry out of the queue, so the old "drop it" behaviour silently
				// threw the whole rest of the queue away one entry at a time. Symptom: queue several
				// upgrades on BP_BuildingBase_Singularian_NeuralArchive, the first runs and nothing
				// follows once it completes.
				// A cooldown-blocked entry therefore goes back to the FRONT and is retried. Any other
				// failure keeps the old drop behaviour, so an entry that can never activate (missing
				// resources, disabled ability) cannot stall the queue forever.
				// VORUEBERGEHENDE Hindernisse duerfen den Eintrag nicht kosten. Neben dem Cooldown
				// gehoert dazu ein gerade laufender Cast: der Riegel in
				// UGameplayAbilityBase::CanActivateAbility lehnt Cast-Abilities ab, solange die Einheit
				// castet - und unmittelbar nach einem Abbruch steht sie dort noch fuer den Bruchteil
				// einer Sekunde. Ohne diese Ausnahme wird der bereits entnommene Eintrag verworfen, der
				// Timer holt sofort den naechsten, der genauso scheitert, und die Warteschlange blutet
				// Eintrag fuer Eintrag aus. Von aussen sieht das aus, als haette ein einziges Abbrechen
				// die GANZE Queue geleert - vom Nutzer am 16.08.2026 genau so gemeldet.
				const AUnitBase* SelfUnit = Cast<AUnitBase>(this);
				const bool bNurVoruebergehend =
					IsAbilityOnCooldownByClass(Next.AbilityClass)
					|| (SelfUnit && SelfUnit->GetUnitState() == UnitData::Casting);

				if (bNurVoruebergehend)
				{
					TArray<FQueuedAbility> Pending;
					FQueuedAbility Item;
					while (AbilityQueue.Dequeue(Item))
					{
						Pending.Add(Item);
					}
					Pending.Insert(Next, 0);
					for (const FQueuedAbility& It : Pending)
					{
						AbilityQueue.Enqueue(It);
					}
					QueSnapshot = Pending;
					AbilityQueueSize = QueSnapshot.Num();
				}
				else
				{
					CancelCurrentAbility();
				}

				if (!AbilityQueue.IsEmpty())
				{
					const float DelayTime = AbilityReactivationThrottle;
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

FRotator AGASUnit::GetMassActorRotation() const
{
	return GetActorRotation();
}

FTransform AGASUnit::GetMassActorTransform() const
{
	return GetActorTransform();
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
			if (HasAuthority())
			{
				CurrentSnapshot.ClickCount = ActivatedAbilityInstance->ClickCount;
			}
			ActivatedAbilityInstance->OnAbilityMouseHit(InHitResult);
		}else
		{
			CancelCurrentAbility();
		}
	}
	else if (HasAuthority() && CurrentSnapshot.AbilityClass)
	{
		// Failsafe: If the snapshot is set but no ability instance exists on the server, clear it.
		CancelCurrentAbility();
	}
}

void AGASUnit::OnRep_CurrentSnapshot()
{
	// The server has confirmed the ability started.
	// We can now clear the safety window because we are no longer "waiting" for replication.
	if (CurrentSnapshot.AbilityClass != nullptr)
	{
		LastAbilitySafetyWindowTime = 0.f;
	}

	if (ActivatedAbilityInstance && CurrentSnapshot.AbilityClass == ActivatedAbilityInstance->GetClass())
	{
		ActivatedAbilityInstance->ClickCount = CurrentSnapshot.ClickCount;
		ActivatedAbilityInstance->AbilityInputID = CurrentSnapshot.InputID;
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

void AGASUnit::StopCurrentAbility(bool bWasCancelled)
{
	if (!HasAuthority() || !ActivatedAbilityInstance) return;

	if (bWasCancelled)
	{
		ActivatedAbilityInstance->K2_CancelAbility();
	}
	else
	{
		// Gracefully end the ability as a successful completion
		ActivatedAbilityInstance->EndAbility(
			ActivatedAbilityInstance->GetCurrentAbilitySpecHandle(),
			ActivatedAbilityInstance->GetCurrentActorInfo(),
			ActivatedAbilityInstance->GetCurrentActivationInfo(),
			true, // bReplicateEndAbility
			false // bWasCancelled
		);
	}
}

void AGASUnit::NotifyInputReleased(EGASAbilityInputID InputID)
{
	if (ActivatedAbilityInstance && ActivatedAbilityInstance->AbilityInputID == InputID)
	{
		ActivatedAbilityInstance->OnInputReleased();
	}
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
			ActivatedAbilityInstance->ClickCount = 0;
			ActivatedAbilityInstance->K2_CancelAbility();
			ActivatedAbilityInstance = nullptr;
		}
	}
	CurrentSnapshot = FQueuedAbility();
	CurrentInstigatorPC = nullptr;
}

void AGASUnit::UpdateReplicatedAbilityCost(TSubclassOf<UGameplayAbilityBase> AbilityClass, FBuildingCost NewCost) {
	if (!HasAuthority()) return;

	for (FAbilityCostData& Data : ReplicatedAbilityCosts) {
		if (Data.AbilityClass == AbilityClass) {
			Data.CurrentCost = NewCost;
			return;
		}
	}
	ReplicatedAbilityCosts.Add({AbilityClass, NewCost});
}

UGameplayAbilityBase* AGASUnit::GetAbilityDisplayObject(TSubclassOf<UGameplayAbilityBase> AbilityClass) {
	if (!AbilityClass) return nullptr;

	// 1. Try to find the real instance if we are the owner
	if (AbilitySystemComponent) {
		for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities()) {
			if (Spec.Ability && Spec.Ability->GetClass() == AbilityClass) {
				UGameplayAbilityBase* Instance = Cast<UGameplayAbilityBase>(Spec.GetPrimaryInstance());
				return Instance ? Instance : Cast<UGameplayAbilityBase>(Spec.Ability);
			}
		}
	}

	// 2. Otherwise, use a local Proxy object
	if (!AbilityProxyCache.Contains(AbilityClass)) {
		// Create a transient instance once
		AbilityProxyCache.Add(AbilityClass, NewObject<UGameplayAbilityBase>(this, AbilityClass));
	}

	UGameplayAbilityBase* Proxy = AbilityProxyCache[AbilityClass];
	
	// Default to CDO values
	const UGameplayAbilityBase* CDO = AbilityClass->GetDefaultObject<UGameplayAbilityBase>();
	Proxy->ConstructionCost = CDO->ConstructionCost;

	// Apply replicated override if exists
	for (const FAbilityCostData& Data : ReplicatedAbilityCosts) {
		if (Data.AbilityClass == AbilityClass) {
			Proxy->ConstructionCost = Data.CurrentCost;
			break;
		}
	}
	
	Proxy->UpdateTooltipText(); // Refresh text with current cost
	return Proxy;
}