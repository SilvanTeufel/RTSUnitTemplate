// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Core/CollisionUtils.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
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

FVector FCollisionUtils::ComputeImpactSurfaceXY(const AActor* Attacker, const AActor* Target, TOptional<FVector> OverrideIncomingLocation)
{
    UE_LOG(LogTemp, Warning, TEXT("ENTERING ComputeImpactSurfaceXY (Utils) - Attacker: %s, Target: %s"), 
        Attacker ? *Attacker->GetName() : TEXT("NULL"), Target ? *Target->GetName() : TEXT("NULL"));

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
            UE_LOG(LogTemp, Warning, TEXT("ComputeImpactSurfaceXY (Utils) - Point was INSIDE box, pushing to edge."));
        }

        // 4. Transform back to world space
        Surface = BoxTransform.TransformPosition(LocalSurface);
        UE_LOG(LogTemp, Warning, TEXT("ComputeImpactSurfaceXY (Utils) - BOX Surface: %s (Local: %s, Extent: %s)"), 
            *Surface.ToString(), *LocalSurface.ToString(), *Extent.ToString());
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
            UE_LOG(LogTemp, Warning, TEXT("ComputeImpactSurfaceXY (Utils) - Using Capsule Radius: %f"), Radius2D);
        }
        else
        {
            FVector Origin, Extent;
            Target->GetActorBounds(false, Origin, Extent);
            Radius2D = FMath::Max(Extent.X, Extent.Y);
            UE_LOG(LogTemp, Warning, TEXT("ComputeImpactSurfaceXY (Utils) - Using Actor Bounds Radius: %f"), Radius2D);
        }

        Dir2D.Normalize();
        Surface = TargetCenter - Dir2D * Radius2D;
        UE_LOG(LogTemp, Warning, TEXT("ComputeImpactSurfaceXY (Utils) - CAPSULE/OTHER Surface: %s"), *Surface.ToString());
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
        Surface.Z = TargetCenter.Z;
    }
    else
    {
        Surface.Z = bAttackerFlying ? TargetCenter.Z : IncomingLoc.Z;
    }

    return Surface;
}
