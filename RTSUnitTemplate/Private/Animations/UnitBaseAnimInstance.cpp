// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


#include "Animations/UnitBaseAnimInstance.h"

#include "Characters/Unit/SpeakingUnit.h"
#include "Characters/Unit/UnitBase.h"
#include "Characters/Unit/SpeakingUnit.h"
#include "Core/UnitData.h"
#include "Net/UnrealNetwork.h"
#include "MassEntitySubsystem.h"
#include "MassMovementFragments.h" // LUX-ANPASSUNG (16.08.2026): FMassVelocityFragment fuer MassSpeed
#include "Mass/MassActorBindingComponent.h"
#include "Animations/UnitAnimationProcessor.h"
#include "MassExecutionContext.h"
#include "Components/SkeletalMeshComponent.h"

UUnitBaseAnimInstance::UUnitBaseAnimInstance() {
	CharAnimState = UnitData::Idle;
}


void UUnitBaseAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
}

void UUnitBaseAnimInstance::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UUnitBaseAnimInstance, AnimDataTable);
}

void UUnitBaseAnimInstance::NativeUpdateAnimation(float Deltaseconds)
{
	Super::NativeUpdateAnimation(Deltaseconds);
	/*
	ControlTimer += Deltaseconds;
	if(ControlTimer < UpdateTime) return;
	ControlTimer = 0.f;
	*/
	bMassSpeedValid = false;

	AActor* OwningActor = GetOwningActor();

	if (OwningActor != nullptr) {
		AUnitBase* UnitBase = Cast<AUnitBase>(OwningActor);
		if (UnitBase != nullptr && UnitBase->IsOnViewport) {

			if (UnitBase->MassActorBindingComponent)
			{
				const FMassEntityHandle Entity = UnitBase->MassActorBindingComponent->GetEntityHandle();
				if (Entity.IsValid())
				{
					UWorld* World = UnitBase->GetWorld();
					UMassEntitySubsystem* EntitySubsystem = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
					if (EntitySubsystem)
					{
						const FMassEntityManager& EntityManager = EntitySubsystem->GetEntityManager();
						if (EntityManager.IsEntityValid(Entity))
						{
							FUnitAnimationFragment* AnimFrag = EntityManager.GetFragmentDataPtr<FUnitAnimationFragment>(Entity);
							if (AnimFrag)
							{
								BlendPoint_1 = AnimFrag->TargetBlendPoint_1;
								BlendPoint_2 = AnimFrag->TargetBlendPoint_2;
								CurrentBlendPoint_1 = AnimFrag->CurrentBlendPoint_1;
								CurrentBlendPoint_2 = AnimFrag->CurrentBlendPoint_2;
								TransitionRate_1 = AnimFrag->TransitionRate_1;
								TransitionRate_2 = AnimFrag->TransitionRate_2;
								Resolution_1 = AnimFrag->Resolution_1;
								Resolution_2 = AnimFrag->Resolution_2;
								Sound = AnimFrag->Sound;
								ContinuousPlayRate = AnimFrag->PlayRate;
								ContinuousAnimationPosition = AnimFrag->AnimationPosition;

								// ============================================================
								// LUX-ANPASSUNG (16.08.2026) â€” Mass-Geschwindigkeit fuer den
								// AnimBP. Siehe Kommentar an MassSpeed in der .h.
								//
								// Bewusst HIER und nicht im UnitAnimationProcessor: dessen
								// AnimInstance-Zugriff steht in einem Zweig, der nur bei
								// ZUSTANDSWECHSELN laeuft (LastProcessedState != CurrentState).
								// Gemessen blieb MassSpeed dort auf 0, obwohl die Einheit lief.
								// Pro Frame ginge es dort nur mit einem zusaetzlichen
								// GetAnimInstance() + Cast je Entity - teurer als der eine
								// Fragment-Lookup hier, der zudem nur fuer sichtbare Einheiten
								// laeuft (IsOnViewport). Rein additiv.
								// ============================================================
								if (const FMassVelocityFragment* VelFrag =
									EntityManager.GetFragmentDataPtr<FMassVelocityFragment>(Entity))
								{
									MassVelocity = VelFrag->Value;
									MassSpeed = MassVelocity.Size2D();
									bMassSpeedValid = true;
								}
								// ===================== ENDE LUX-ANPASSUNG ===================

								/*
								if (World && World->GetNetMode() == NM_Client)
								{
									UE_LOG(LogTemp, VeryVerbose, TEXT("[AnimInstance] %s: Updated from Mass. CBP1: %.2f"), *UnitBase->GetName(), CurrentBlendPoint_1);
								}
								*/
							}
							else
							{
								UE_LOG(LogTemp, Warning, TEXT("[AnimInstance] %s: No AnimFrag found on Entity!"), *UnitBase->GetName());
							}
						}
					}
				}
			}

			CharAnimState = UnitBase->GetUnitState();

			// Steht die Einheit, sieht sie auch stehend aus - selbst wenn ihr Zustand Laufen sagt.
			//
			// Run/Chase/PatrolRandom und die GoTo-Zustaende bleiben aktiv, waehrend die Einheit
			// stillsteht: die Pfadsuche laeuft, das Ziel ist erreicht, oder der Weg ist versperrt.
			// Das AnimBP zeigte trotzdem die Laufanimation - Laufen auf der Stelle. Geaendert wird
			// nur die an das AnimBP gemeldete Anzeige, der Zustand der Einheit bleibt unberuehrt.
			if (bMassSpeedValid && MassSpeed <= IdleAnimSpeedThreshold)
			{
				switch (CharAnimState.GetValue())
				{
				case UnitData::Run:
				case UnitData::Chase:
				case UnitData::Patrol:
				case UnitData::PatrolRandom:
				case UnitData::GoToBase:
				case UnitData::GoToBuild:
				case UnitData::GoToResourceExtraction:
					CharAnimState = UnitData::Idle;
					break;
				default:
					break;
				}
			}
			// SetBlendPoints(UnitBase, Deltaseconds); // Processor übernimmt das jetzt

			if(LastAnimState != CharAnimState)
			{
				SoundTimer = 0.f;
				LastAnimState = CharAnimState;

				// Sound wird jetzt über den UnitAnimationProcessor im Fragment gesetzt.
			}
			
			if(Sound && UnitBase)
			{
				if(SoundTimer == 0.f)
					UGameplayStatics::PlaySoundAtLocation(UnitBase, Sound, UnitBase->GetActorLocation(), 1.f);

				SoundTimer += Deltaseconds;
				
			}
		}
	}
}


void UUnitBaseAnimInstance::SetBlendPoints(AUnitBase* Unit, float Deltaseconds)
{
	TEnumAsByte<UnitData::EState> AnimState = Unit->GetUnitState();
	
	if (AnimDataTable)
	{
		for(auto it : AnimDataTable->GetRowMap())
		{
			FString Key = it.Key.ToString();
			UnitAnimData = reinterpret_cast<FUnitAnimData*>(it.Value);
			if(UnitAnimData->AnimState == AnimState && UnitAnimData->AnimState != UnitData::Speaking)
			{
				BlendPoint_1 = UnitAnimData->BlendPoint_1;
				BlendPoint_2 = UnitAnimData->BlendPoint_2;
				TransitionRate_1 = UnitAnimData->TransitionRate_1;
				TransitionRate_2 = UnitAnimData->TransitionRate_2;
				Resolution_1 = UnitAnimData->Resolution_1;
				Resolution_2 = UnitAnimData->Resolution_2;
				Sound = UnitAnimData->Sound;
			}else
			{
				ASpeakingUnit* SpeakingUnit = Cast<ASpeakingUnit>(Unit);

				if(SpeakingUnit && SpeakingUnit->SpeechBubble)
				{
					SpeakingUnit->SpeechBubble->AnimationTime += Deltaseconds;
					if(SpeakingUnit->SpeechBubble->AnimationTime <= SpeakingUnit->SpeechBubble->MaxAnimationTime)
					{
						BlendPoint_1 = SpeakingUnit->SpeechBubble->BlendPoint_1;
						BlendPoint_2 = SpeakingUnit->SpeechBubble->BlendPoint_2;
					}else
					{
						BlendPoint_1 = UnitAnimData->BlendPoint_1;
						BlendPoint_2 = UnitAnimData->BlendPoint_2;
					}
					

					TransitionRate_1 = UnitAnimData->TransitionRate_1;
					TransitionRate_2 = UnitAnimData->TransitionRate_2;
					Resolution_1 = UnitAnimData->Resolution_1;
					Resolution_2 = UnitAnimData->Resolution_2;
				}
			}
		}
		
		
	}

}
