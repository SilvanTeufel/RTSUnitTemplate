/*
// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
*/

#include "Actors/RSVirtualTextureActor.h"
#include "Components/RuntimeVirtualTextureComponent.h"
#include "VT/RuntimeVirtualTexture.h"

ARSVirtualTextureActor::ARSVirtualTextureActor()
{
	PrimaryActorTick.bCanEverTick = false;

	VirtualTextureComponent = CreateDefaultSubobject<URuntimeVirtualTextureComponent>(TEXT("VirtualTextureComponent"));
	SetRootComponent(VirtualTextureComponent);
}

// Deliberately no BeginPlay override.
//
// The volume's bounds MUST be baked at edit time via the component's "Set Bounds" button
// (BoundsAlignActor + ExpandBounds), never set at runtime. Two reasons:
//
// 1. Any transform change on a URuntimeVirtualTextureComponent forces a full recreate of the
//    virtual texture -- see URuntimeVirtualTextureComponent::SendRenderTransform_Concurrent()
//    ("We do a full recreate of the URuntimeVirtualTexture here which can cause a visual glitch...
//    there is no way to only modify the transform and maintain the VT contents"). Doing it in
//    BeginPlay drops every resident page, so play always starts from a cold pool and the texture
//    has to re-render from scratch -- at 2 page uploads/frame in a cooked build (r.VT.MaxUploadsPerFrame).
//
// 2. Runtime code cannot reproduce what "Set Bounds" computes: RuntimeVirtualTexture::SetBounds
//    lives in the editor-only VirtualTexturingEditor module, and its inputs (GetBoundsAlignActor /
//    GetExpandBounds / GetSnapBoundsToLandscape) are all WITH_EDITOR-only. It also unions in every
//    primitive that writes to this RVT, not just the navmesh volume.
//
// Note the component's local box is (0,0,0)..(1,1,1) -- the actor location is the volume's MIN
// CORNER, not its center (URuntimeVirtualTextureComponent::CalcBounds). The previous BeginPlay
// used GetCenter(), which offset the volume by half its size and left ~3/4 of the map uncovered.

URuntimeVirtualTexture* ARSVirtualTextureActor::GetVirtualTexture() const
{
	return VirtualTextureComponent ? VirtualTextureComponent->GetVirtualTexture() : nullptr;
}
