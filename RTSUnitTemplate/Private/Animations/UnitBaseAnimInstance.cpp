// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


#include "Animations/UnitBaseAnimInstance.h"

#include "Characters/Unit/SpeakingUnit.h"
#include "Characters/Unit/UnitBase.h"
#include "Characters/Unit/SpeakingUnit.h"
#include "Core/UnitData.h"
#include "Net/UnrealNetwork.h"
#include "MassEntitySubsystem.h"
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
			// SetBlendPoints(UnitBase, Deltaseconds); // Processor übernimmt das jetzt

			if(LastAnimState != CharAnimState)
			{
				SoundTimer = 0.f;
				LastAnimState = CharAnimState;

				// Wenn wir nicht über Mass laufen, oder Sound vom Processor nicht gesetzt wird, 
				// müssen wir Sound hier aktualisieren. Da Sound nur bei Zustandswechsel wichtig ist:
				SetBlendPoints(UnitBase, Deltaseconds);
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
