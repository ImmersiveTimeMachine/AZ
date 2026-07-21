// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "AZ_GE_Damage.generated.h"

/**
 * The canonical instant damage GE: no modifiers of its own — one execution (UAZ_DamageExecCalc) that
 * moves the spec's SetByCaller.Damage magnitude into the target's Vitals IncomingDamage. Every damage
 * source (melee, gunfire, obstacle impacts, fire) applies THIS (or a BP child for typed damage later)
 * with its own magnitude. C++ class so the spine works with zero editor assets.
 */
UCLASS()
class AZ_API UAZ_GE_Damage : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UAZ_GE_Damage();
};
