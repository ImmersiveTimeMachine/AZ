#pragma once

#include "CoreMinimal.h"
#include "MoverTypes.h"
#include "AZ_MoverInputCmd.generated.h"

/**
 * FAZ_MoverInputCmd: Custom ability input data block for AZ's Mover-based pawn.
 * Extends the Mover input collection with GAS-driven flags (dash, crouch, aim, sprint).
 * These flags are set during ProduceInput based on active GameplayTags on the ASC.
 */
USTRUCT(BlueprintType)
struct AZ_API FAZ_MoverInputCmd : public FMoverDataStructBase
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "AZ|Mover")
	bool bIsDashing = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "AZ|Mover")
	bool bIsCrouching = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "AZ|Mover")
	bool bIsAiming = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "AZ|Mover")
	bool bIsSprinting = false;

	// --- FMoverDataStructBase overrides ---

	virtual bool ShouldReconcile(const FMoverDataStructBase& AuthorityState) const override
	{
		const FAZ_MoverInputCmd& Auth = static_cast<const FAZ_MoverInputCmd&>(AuthorityState);
		return (Auth.bIsDashing != bIsDashing)
			|| (Auth.bIsCrouching != bIsCrouching)
			|| (Auth.bIsAiming != bIsAiming)
			|| (Auth.bIsSprinting != bIsSprinting);
	}

	virtual void Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float LerpFactor) override
	{
		const FAZ_MoverInputCmd& Source = static_cast<const FAZ_MoverInputCmd&>((LerpFactor < 0.5f) ? From : To);
		bIsDashing = Source.bIsDashing;
		bIsCrouching = Source.bIsCrouching;
		bIsAiming = Source.bIsAiming;
		bIsSprinting = Source.bIsSprinting;
	}

	virtual void Merge(const FMoverDataStructBase& From) override
	{
		const FAZ_MoverInputCmd& TypedFrom = static_cast<const FAZ_MoverInputCmd&>(From);
		bIsDashing |= TypedFrom.bIsDashing;
		bIsCrouching |= TypedFrom.bIsCrouching;
		bIsAiming |= TypedFrom.bIsAiming;
		bIsSprinting |= TypedFrom.bIsSprinting;
	}

	virtual FMoverDataStructBase* Clone() const override
	{
		return new FAZ_MoverInputCmd(*this);
	}

	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override
	{
		Super::NetSerialize(Ar, Map, bOutSuccess);
		Ar.SerializeBits(&bIsDashing, 1);
		Ar.SerializeBits(&bIsCrouching, 1);
		Ar.SerializeBits(&bIsAiming, 1);
		Ar.SerializeBits(&bIsSprinting, 1);
		bOutSuccess = true;
		return true;
	}

	virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }

	virtual void ToString(FAnsiStringBuilderBase& Out) const override
	{
		Super::ToString(Out);
		Out.Appendf("bIsDashing: %i\n", bIsDashing);
		Out.Appendf("bIsCrouching: %i\n", bIsCrouching);
		Out.Appendf("bIsAiming: %i\n", bIsAiming);
		Out.Appendf("bIsSprinting: %i\n", bIsSprinting);
	}

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override { Super::AddReferencedObjects(Collector); }
};
