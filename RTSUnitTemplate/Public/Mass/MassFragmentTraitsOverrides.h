// Copyright 2025 Teufel-Engineering
#pragma once

// UE 5.7 migration: Opt-in traits for fragments that are not trivially copyable.
// This acknowledges that the author accepts these fragments not being trivially copyable.
// Keep this header lightweight and include it early (e.g., from the module public header).

#include "MassEntityTypes.h"

// Forward declarations of project fragments
struct FMassAITargetFragment;
struct FMassSightFragment;
struct FMassGameplayEffectFragment;
struct FUnitNavigationPathFragment;
struct FMassUnitPathFragment;
struct FMassProjectileFragment;
struct FMassProjectileVisualFragment;

// These are explicit specializations of the Mass fragment traits.
// They must be visible to translation units before templates like AddRequirement/GetFragmentDataPtr are instantiated.

template<>
struct TMassFragmentTraits<FMassAITargetFragment>
{
	static constexpr bool AuthorAcceptsItsNotTriviallyCopyable = true;
};

template<>
struct TMassFragmentTraits<FMassSightFragment>
{
	static constexpr bool AuthorAcceptsItsNotTriviallyCopyable = true;
};

template<>
struct TMassFragmentTraits<FMassGameplayEffectFragment>
{
	static constexpr bool AuthorAcceptsItsNotTriviallyCopyable = true;
};

template<>
struct TMassFragmentTraits<FUnitNavigationPathFragment>
{
	static constexpr bool AuthorAcceptsItsNotTriviallyCopyable = true;
};

template<>
struct TMassFragmentTraits<FMassUnitPathFragment>
{
	static constexpr bool AuthorAcceptsItsNotTriviallyCopyable = true;
};

template<>
struct TMassFragmentTraits<FMassProjectileFragment>
{
	static constexpr bool AuthorAcceptsItsNotTriviallyCopyable = true;
};

template<>
struct TMassFragmentTraits<FMassProjectileVisualFragment>
{
	static constexpr bool AuthorAcceptsItsNotTriviallyCopyable = true;
};
