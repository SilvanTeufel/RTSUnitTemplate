// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "GAS/GameplayAbilityBase.h"

#include "Characters/Unit/UnitBase.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

DEFINE_LOG_CATEGORY_STATIC(LogAbilityKeyGate, Log, All);

// Static registry of disabled ability keys per team
static TMap<int32, TSet<FString>> GDisabledAbilityKeysByTeam;
// Static registry of FORCE-enabled ability keys per team (overrides per-ability bDisabled)
static TMap<int32, TSet<FString>> GForceEnabledAbilityKeysByTeam;

// Helper: normalize keys for consistent matching/logging
static FString NormalizeAbilityKey(const FString& InKey)
{
	FString Out = InKey;
	Out.TrimStartAndEndInline();
	Out = Out.ToLower();
	if (Out.IsEmpty() || Out == TEXT("none"))
	{
		return FString();
	}
	return Out;
}

UGameplayAbilityBase::UGameplayAbilityBase()
{
	UpdateTooltipText();
}

bool UGameplayAbilityBase::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	// Resolve team and normalized key first
	int32 TeamId = INDEX_NONE;
	if (ActorInfo && ActorInfo->OwnerActor.IsValid())
	{
		if (const AUnitBase* Unit = Cast<AUnitBase>(ActorInfo->OwnerActor.Get()))
		{
			TeamId = Unit->TeamId;
		}
	}
	const FString NormalizedKey = NormalizeAbilityKey(AbilityKey);

	// If this key is force-enabled for the team, allow regardless of asset flag or disabled-by-key
	if (TeamId != INDEX_NONE && !NormalizedKey.IsEmpty())
	{
		if (UGameplayAbilityBase::IsAbilityKeyForceEnabledForTeam(NormalizedKey, TeamId))
		{
			UE_LOG(LogAbilityKeyGate, VeryVerbose, TEXT("CanActivateAbility FORCE-ALLOWED: Key='%s' TeamId=%d Ability=%s"), *NormalizedKey, TeamId, *GetNameSafe(this));
			return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
		}
	}

	// Disallow if the ability asset is flagged disabled
	if (bDisabled)
	{
		UE_LOG(LogAbilityKeyGate, Verbose, TEXT("CanActivateAbility blocked: bDisabled=true on %s (no force-enable)"), *GetNameSafe(this));
		return false;
	}

	// Check team-based disable by key
	if (TeamId != INDEX_NONE && !NormalizedKey.IsEmpty())
	{
		const bool bIsDisabled = UGameplayAbilityBase::IsAbilityKeyDisabledForTeam(NormalizedKey, TeamId);
		if (bIsDisabled)
		{
			UE_LOG(LogAbilityKeyGate, Verbose, TEXT("CanActivateAbility blocked by team-key: Key='%s' TeamId=%d Ability=%s (no force-enable)"), *NormalizedKey, TeamId, *GetNameSafe(this));
			return false;
		}
		else
		{
			UE_LOG(LogAbilityKeyGate, VeryVerbose, TEXT("CanActivateAbility allowed by team-key: Key='%s' TeamId=%d Ability=%s"), *NormalizedKey, TeamId, *GetNameSafe(this));
		}
	}
	else
	{
		UE_LOG(LogAbilityKeyGate, VeryVerbose, TEXT("CanActivateAbility (no key/team gate): TeamId=%d Key='%s' Ability=%s"), TeamId, *NormalizedKey, *GetNameSafe(this));
	}

	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UGameplayAbilityBase::Debug_DumpDisabledAbilityKeys()
{
	UE_LOG(LogAbilityKeyGate, Log, TEXT("==== Disabled Ability Keys Dump ===="));
	for (const TPair<int32, TSet<FString>>& Pair : GDisabledAbilityKeysByTeam)
	{
		const int32 TeamId = Pair.Key;
		const FString Keys = FString::Join(Pair.Value.Array(), TEXT(","));
		UE_LOG(LogAbilityKeyGate, Log, TEXT("Team %d: {%s}"), TeamId, *Keys);
	}
	if (GDisabledAbilityKeysByTeam.Num() == 0)
	{
		UE_LOG(LogAbilityKeyGate, Log, TEXT("<empty>"));
	}
	UE_LOG(LogAbilityKeyGate, Log, TEXT("==== FORCE-Enabled Ability Keys Dump ===="));
	for (const TPair<int32, TSet<FString>>& Pair : GForceEnabledAbilityKeysByTeam)
	{
		const int32 TeamId = Pair.Key;
		const FString Keys = FString::Join(Pair.Value.Array(), TEXT(","));
		UE_LOG(LogAbilityKeyGate, Log, TEXT("Team %d: {%s}"), TeamId, *Keys);
	}
	if (GForceEnabledAbilityKeysByTeam.Num() == 0)
	{
		UE_LOG(LogAbilityKeyGate, Log, TEXT("<empty>"));
	}
}

void UGameplayAbilityBase::SetAbilitiesEnabledForTeamByKey(const FString& Key, bool bEnable)
{
	const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo();
	if (!Info || !Info->OwnerActor.IsValid())
	{
		UE_LOG(LogAbilityKeyGate, Warning, TEXT("SetAbilitiesEnabledForTeamByKey called but ActorInfo/Owner invalid. Key='%s' Enable=%s"), *Key, bEnable ? TEXT("true") : TEXT("false"));
		return;
	}
	const AUnitBase* Unit = Cast<AUnitBase>(Info->OwnerActor.Get());
	if (!Unit)
	{
		UE_LOG(LogAbilityKeyGate, Warning, TEXT("SetAbilitiesEnabledForTeamByKey owner is not AUnitBase. Key='%s'"), *Key);
		return;
	}
	const FString NormalizedKey = NormalizeAbilityKey(Key);
	if (NormalizedKey.IsEmpty())
	{
		UE_LOG(LogAbilityKeyGate, Warning, TEXT("SetAbilitiesEnabledForTeamByKey ignored empty/none key. Raw='%s'"), *Key);
		return;
	}
	UE_LOG(LogAbilityKeyGate, Log, TEXT("SetAbilitiesEnabledForTeamByKey: TeamId=%d Key='%s' Enable=%s (CallerAbility=%s)"), Unit->TeamId, *NormalizedKey, bEnable ? TEXT("true") : TEXT("false"), *GetNameSafe(this));
	UGameplayAbilityBase::SetAbilitiesEnabledForTeamByKey_Static(NormalizedKey, Unit->TeamId, bEnable);
}

void UGameplayAbilityBase::SetAbilitiesEnabledForTeamByKey_Static(const FString& Key, int32 TeamId, bool bEnable)
{
	if (TeamId == INDEX_NONE)
	{
		UE_LOG(LogAbilityKeyGate, Warning, TEXT("SetAbilitiesEnabledForTeamByKey_Static: invalid TeamId (INDEX_NONE). RawKey='%s'"), *Key);
		return;
	}
	const FString NormalizedKey = NormalizeAbilityKey(Key);
	if (NormalizedKey.IsEmpty())
	{
		UE_LOG(LogAbilityKeyGate, Warning, TEXT("SetAbilitiesEnabledForTeamByKey_Static: ignored empty/none key. RawKey='%s'"), *Key);
		return;
	}
	TSet<FString>& DisabledRef = GDisabledAbilityKeysByTeam.FindOrAdd(TeamId);
	const bool bWasInDisabled = DisabledRef.Contains(NormalizedKey);
	// Log before state
	UE_LOG(LogAbilityKeyGate, Log, TEXT("[Before] TeamId=%d DisabledKeys={%s}"), TeamId, *FString::Join(DisabledRef.Array(), TEXT(",")));

	if (bEnable)
	{
		DisabledRef.Remove(NormalizedKey);
		// Also force-enable so this key works even if assets are bDisabled
		TSet<FString>& ForceRef = GForceEnabledAbilityKeysByTeam.FindOrAdd(TeamId);
		const bool bAlreadyForced = ForceRef.Contains(NormalizedKey);
		if (!bAlreadyForced)
		{
			ForceRef.Add(NormalizedKey);
		}
		// Clean empty force set: not needed since we just added
	}
	else
	{
		DisabledRef.Add(NormalizedKey);
		// When disabling, remove any force-enable override for consistency
		if (TSet<FString>* ForceRef = GForceEnabledAbilityKeysByTeam.Find(TeamId))
		{
			ForceRef->Remove(NormalizedKey);
			if (ForceRef->Num() == 0)
			{
				GForceEnabledAbilityKeysByTeam.Remove(TeamId);
			}
		}
	}
	// Clean up empty disabled set
	if (DisabledRef.Num() == 0)
	{
		GDisabledAbilityKeysByTeam.Remove(TeamId);
	}

	UE_LOG(LogAbilityKeyGate, Log, TEXT("SetAbilitiesEnabledForTeamByKey_Static: TeamId=%d Key='%s' Enable=%s (wasInDisabled=%s)"), TeamId, *NormalizedKey, bEnable ? TEXT("true") : TEXT("false"), bWasInDisabled ? TEXT("true") : TEXT("false"));
	if (const TSet<FString>* AfterSet = GDisabledAbilityKeysByTeam.Find(TeamId))
	{
		UE_LOG(LogAbilityKeyGate, Log, TEXT("[After] TeamId=%d DisabledKeys={%s}"), TeamId, *FString::Join(AfterSet->Array(), TEXT(",")));
	}
	else
	{
		UE_LOG(LogAbilityKeyGate, Log, TEXT("[After] TeamId=%d DisabledKeys={}"), TeamId);
	}
	if (const TSet<FString>* ForceAfter = GForceEnabledAbilityKeysByTeam.Find(TeamId))
	{
		UE_LOG(LogAbilityKeyGate, Log, TEXT("[After] TeamId=%d ForceEnabledKeys={%s}"), TeamId, *FString::Join(ForceAfter->Array(), TEXT(",")));
	}
	else
	{
		UE_LOG(LogAbilityKeyGate, Log, TEXT("[After] TeamId=%d ForceEnabledKeys={}"), TeamId);
	}
}

bool UGameplayAbilityBase::IsAbilityKeyDisabledForTeam(const FString& Key, int32 TeamId)
{
	if (TeamId == INDEX_NONE)
	{
		return false;
	}
	const FString NormalizedKey = NormalizeAbilityKey(Key);
	if (NormalizedKey.IsEmpty())
	{
		return false;
	}
	if (const TSet<FString>* SetPtr = GDisabledAbilityKeysByTeam.Find(TeamId))
	{
		const bool bContains = SetPtr->Contains(NormalizedKey);
		UE_LOG(LogAbilityKeyGate, VeryVerbose, TEXT("IsAbilityKeyDisabledForTeam? TeamId=%d Key='%s' -> %s"), TeamId, *NormalizedKey, bContains ? TEXT("true") : TEXT("false"));
		return bContains;
	}
	UE_LOG(LogAbilityKeyGate, VeryVerbose, TEXT("IsAbilityKeyDisabledForTeam? TeamId=%d has no disabled keys"), TeamId);
	return false;
}

void UGameplayAbilityBase::SetAbilitiesForceEnabledForTeamByKey(const FString& Key, bool bForceEnable)
{
	const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo();
	if (!Info || !Info->OwnerActor.IsValid())
	{
		UE_LOG(LogAbilityKeyGate, Warning, TEXT("SetAbilitiesForceEnabledForTeamByKey called but ActorInfo/Owner invalid. Key='%s' ForceEnable=%s"), *Key, bForceEnable ? TEXT("true") : TEXT("false"));
		return;
	}
	const AUnitBase* Unit = Cast<AUnitBase>(Info->OwnerActor.Get());
	if (!Unit)
	{
		UE_LOG(LogAbilityKeyGate, Warning, TEXT("SetAbilitiesForceEnabledForTeamByKey owner is not AUnitBase. Key='%s'"), *Key);
		return;
	}
	const FString NormalizedKey = NormalizeAbilityKey(Key);
	if (NormalizedKey.IsEmpty())
	{
		UE_LOG(LogAbilityKeyGate, Warning, TEXT("SetAbilitiesForceEnabledForTeamByKey ignored empty/none key. Raw='%s'"), *Key);
		return;
	}
	UE_LOG(LogAbilityKeyGate, Log, TEXT("SetAbilitiesForceEnabledForTeamByKey: TeamId=%d Key='%s' ForceEnable=%s (CallerAbility=%s)"), Unit->TeamId, *NormalizedKey, bForceEnable ? TEXT("true") : TEXT("false"), *GetNameSafe(this));
	UGameplayAbilityBase::SetAbilitiesForceEnabledForTeamByKey_Static(NormalizedKey, Unit->TeamId, bForceEnable);
}

void UGameplayAbilityBase::SetAbilitiesForceEnabledForTeamByKey_Static(const FString& Key, int32 TeamId, bool bForceEnable)
{
	if (TeamId == INDEX_NONE)
	{
		UE_LOG(LogAbilityKeyGate, Warning, TEXT("SetAbilitiesForceEnabledForTeamByKey_Static: invalid TeamId (INDEX_NONE). RawKey='%s'"), *Key);
		return;
	}
	const FString NormalizedKey = NormalizeAbilityKey(Key);
	if (NormalizedKey.IsEmpty())
	{
		UE_LOG(LogAbilityKeyGate, Warning, TEXT("SetAbilitiesForceEnabledForTeamByKey_Static: ignored empty/none key. RawKey='%s'"), *Key);
		return;
	}
	TSet<FString>& SetRef = GForceEnabledAbilityKeysByTeam.FindOrAdd(TeamId);
	const bool bWasInSet = SetRef.Contains(NormalizedKey);
	UE_LOG(LogAbilityKeyGate, Log, TEXT("[Before] TeamId=%d ForceEnabledKeys={%s}"), TeamId, *FString::Join(SetRef.Array(), TEXT(",")));

	if (bForceEnable)
	{
		SetRef.Add(NormalizedKey);
	}
	else
	{
		SetRef.Remove(NormalizedKey);
	}
	if (SetRef.Num() == 0)
	{
		GForceEnabledAbilityKeysByTeam.Remove(TeamId);
	}

	UE_LOG(LogAbilityKeyGate, Log, TEXT("SetAbilitiesForceEnabledForTeamByKey_Static: TeamId=%d Key='%s' ForceEnable=%s (wasInSet=%s)"), TeamId, *NormalizedKey, bForceEnable ? TEXT("true") : TEXT("false"), bWasInSet ? TEXT("true") : TEXT("false"));
	if (const TSet<FString>* AfterSet = GForceEnabledAbilityKeysByTeam.Find(TeamId))
	{
		UE_LOG(LogAbilityKeyGate, Log, TEXT("[After] TeamId=%d ForceEnabledKeys={%s}"), TeamId, *FString::Join(AfterSet->Array(), TEXT(",")));
	}
	else
	{
		UE_LOG(LogAbilityKeyGate, Log, TEXT("[After] TeamId=%d ForceEnabledKeys={}"), TeamId);
	}
}

bool UGameplayAbilityBase::IsAbilityKeyForceEnabledForTeam(const FString& Key, int32 TeamId)
{
	if (TeamId == INDEX_NONE)
	{
		return false;
	}
	const FString NormalizedKey = NormalizeAbilityKey(Key);
	if (NormalizedKey.IsEmpty())
	{
		return false;
	}
	if (const TSet<FString>* SetPtr = GForceEnabledAbilityKeysByTeam.Find(TeamId))
	{
		const bool bContains = SetPtr->Contains(NormalizedKey);
		UE_LOG(LogAbilityKeyGate, VeryVerbose, TEXT("IsAbilityKeyForceEnabledForTeam? TeamId=%d Key='%s' -> %s"), TeamId, *NormalizedKey, bContains ? TEXT("true") : TEXT("false"));
		return bContains;
	}
	UE_LOG(LogAbilityKeyGate, VeryVerbose, TEXT("IsAbilityKeyForceEnabledForTeam? TeamId=%d has no force-enabled keys"), TeamId);
	return false;
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
			
				//MyProjectile->Mesh_A->OnComponentBeginOverlap.AddDynamic(MyProjectile, &AProjectile::OnOverlapBegin);
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