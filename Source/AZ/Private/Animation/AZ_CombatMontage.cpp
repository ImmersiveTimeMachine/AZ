// Copyright Artur. AZ project.

#include "Animation/AZ_CombatMontage.h"

#include "Animation/AnimMontage.h"

float FAZ_CombatMontage::ClipSeconds() const
{
	return Montage ? Montage->GetPlayLength() : 0.f;
}

float FAZ_CombatMontage::ResolveBeat() const
{
	const float Clip = ClipSeconds();
	return (ActiveSeconds > 0.f) ? FMath::Min(ActiveSeconds, Clip) : Clip;
}

float FAZ_CombatMontage::ResolveRootMotion() const
{
	const float Clip = ClipSeconds();
	return (RootMotionSeconds > 0.f) ? FMath::Min(RootMotionSeconds, Clip) : ResolveBeat();
}

bool FAZ_CombatMontage::IsCutEarly() const
{
	return ResolveBeat() < ClipSeconds() - UE_KINDA_SMALL_NUMBER;
}

float FAZ_CombatMontage::ResolveGate() const
{
	return ResolveBeat() + (IsCutEarly() ? BlendOutTime : 0.f);
}

float FAZ_CombatMontage::ResolveStaggerHold() const
{
	return ResolveGate() + FMath::Max(0.f, StaggerRecoverSeconds);
}
