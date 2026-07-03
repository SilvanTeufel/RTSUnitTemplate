// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Core/CollisionUtils.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "Characters/Unit/UnitBase.h"
#include "Characters/Unit/MassUnitBase.h"
#include "Mass/MassActorBindingComponent.h"

UBoxComponent* FCollisionUtils::FindTaggedBoxComponent(const AActor* Actor)
{
    if (!Actor) return nullptr;

    TArray<UBoxComponent*> BoxComps;
    Actor->GetComponents<UBoxComponent>(BoxComps);
    for (UBoxComponent* Box : BoxComps)
    {
        if (Box->ComponentHasTag(AUnitBase::BoxCollisionTag))
        {
            return Box;
        }
    }
    return nullptr;
}

UBoxComponent* FCollisionUtils::FindTaggedBoxTemplateOnClass(UClass* ActorClass)
{
    if (!ActorClass || !ActorClass->IsChildOf(AActor::StaticClass()))
    {
        return nullptr;
    }

    // Native components live on the CDO and are found by the regular instance search.
    if (const AActor* CDO = ActorClass->GetDefaultObject<AActor>())
    {
        if (UBoxComponent* NativeBox = FindTaggedBoxComponent(CDO))
        {
            return NativeBox;
        }
    }

    // Blueprint-added components only exist as SCS templates on the generated class. A child
    // Blueprint's edits to an inherited component (changed extent, added/removed tag) live in
    // its InheritableComponentHandler, so resolve every node against the most-derived BPGC
    // (GetActualComponentTemplate) instead of reading the declaring class's template directly.
    UBlueprintGeneratedClass* MostDerivedBPGC = nullptr;
    for (UClass* Cls = ActorClass; Cls; Cls = Cls->GetSuperClass())
    {
        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Cls))
        {
            MostDerivedBPGC = BPGC;
            break;
        }
    }

    for (UClass* Cls = ActorClass; Cls; Cls = Cls->GetSuperClass())
    {
        const UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Cls);
        if (!BPGC || !BPGC->SimpleConstructionScript)
        {
            continue;
        }
        for (const USCS_Node* Node : BPGC->SimpleConstructionScript->GetAllNodes())
        {
            if (!Node)
            {
                continue;
            }
            UActorComponent* Template = MostDerivedBPGC
                ? Node->GetActualComponentTemplate(MostDerivedBPGC)
                : static_cast<UActorComponent*>(Node->ComponentTemplate);
            UBoxComponent* Box = Cast<UBoxComponent>(Template);
            if (Box && Box->ComponentHasTag(AUnitBase::BoxCollisionTag))
            {
                return Box;
            }
        }
    }
    return nullptr;
}

FVector FCollisionUtils::ComputeImpactSurfaceXY(const AActor* Attacker, const AActor* Target, TOptional<FVector> OverrideIncomingLocation)
{
    if (!Target)
    {
        return FVector::ZeroVector;
    }

    auto GetMassLoc = [](const AActor* Actor) -> FVector
    {
        if (const AMassUnitBase* MUB = Cast<AMassUnitBase>(Actor))
        {
            return MUB->GetMassActorLocation();
        }
        return Actor ? Actor->GetActorLocation() : FVector::ZeroVector;
    };

    const FVector IncomingLoc = OverrideIncomingLocation.IsSet() ? OverrideIncomingLocation.GetValue() : GetMassLoc(Attacker);
    const FVector TargetCenter = GetMassLoc(Target);

    FVector Dir2D(TargetCenter.X - IncomingLoc.X, TargetCenter.Y - IncomingLoc.Y, 0.f);
    if (Dir2D.IsNearlyZero())
    {
        return TargetCenter;
    }

    UBoxComponent* TargetedBox = nullptr;
    if (const AUnitBase* TargetUnit = Cast<AUnitBase>(Target))
    {
        TargetedBox = TargetUnit->BoxCollisionComponent;
        if (!TargetedBox)
        {
            // Fallback: try to find it via tag if the cached reference isn't set
            TargetedBox = FindTaggedBoxComponent(Target);
        }
    }
    else
    {
        // Even if it's not AUnitBase, it might have a tagged box component
        TargetedBox = FindTaggedBoxComponent(Target);
    }

    FVector Surface = TargetCenter;

    if (TargetedBox)
    {
        const FVector Extent = TargetedBox->GetUnscaledBoxExtent();
        const FTransform BoxTransform = TargetedBox->GetComponentTransform();

        // 1. Transform the incoming location into the box's local space
        FVector LocalIncoming = BoxTransform.InverseTransformPosition(IncomingLoc);
        LocalIncoming.Z = 0.f; // Only work on XY plane

        // 2. Clamp local X and Y to the box extents
        FVector LocalSurface = LocalIncoming;
        LocalSurface.X = FMath::Clamp(LocalSurface.X, -Extent.X, Extent.X);
        LocalSurface.Y = FMath::Clamp(LocalSurface.Y, -Extent.Y, Extent.Y);

        // 3. (Edge case) If incoming point is inside, push it to the nearest edge
        if (FMath::Abs(LocalIncoming.X) < Extent.X && FMath::Abs(LocalIncoming.Y) < Extent.Y)
        {
            float DistToX = Extent.X - FMath::Abs(LocalIncoming.X);
            float DistToY = Extent.Y - FMath::Abs(LocalIncoming.Y);

            if (DistToX < DistToY)
            {
                LocalSurface.X = (LocalIncoming.X >= 0) ? Extent.X : -Extent.X;
            }
            else
            {
                LocalSurface.Y = (LocalIncoming.Y >= 0) ? Extent.Y : -Extent.Y;
            }
        }

        // 4. Transform back to world space
        Surface = BoxTransform.TransformPosition(LocalSurface);
    }
    else
    {
        float Radius2D = 0.f;
        if (const ACharacter* Character = Cast<ACharacter>(Target))
        {
            Radius2D = Character->GetCapsuleComponent()->GetScaledCapsuleRadius();
            if (const UMassActorBindingComponent* BindingComponent = Target->FindComponentByClass<UMassActorBindingComponent>())
            {
                Radius2D += BindingComponent->AdditionalCapsuleRadius;
            }
        }
        else
        {
            FVector Origin, Extent;
            Target->GetActorBounds(false, Origin, Extent);
            Radius2D = FMath::Max(Extent.X, Extent.Y);
        }

        Dir2D.Normalize();
        Surface = TargetCenter - Dir2D * Radius2D;
    }

    // Keep Z-axis height logic (bTargetFlying, bAttackerFlying)
    bool bTargetFlying = false;
    bool bAttackerFlying = false;

    if (const AMassUnitBase* TUnit = Cast<AMassUnitBase>(Target))
    {
        bTargetFlying = TUnit->IsFlying;
    }

    if (const AMassUnitBase* AUnit = Cast<AMassUnitBase>(Attacker))
    {
        bAttackerFlying = AUnit->IsFlying;
    }

    if (bTargetFlying)
    {
        Surface.Z = OverrideIncomingLocation.IsSet() ? IncomingLoc.Z : TargetCenter.Z;
    }
    else
    {
        Surface.Z = bAttackerFlying ? TargetCenter.Z : IncomingLoc.Z;
    }

    return Surface;
}
