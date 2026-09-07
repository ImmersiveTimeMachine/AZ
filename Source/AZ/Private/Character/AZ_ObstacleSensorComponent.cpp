// Copyright Artur. AZ project.

#include "Character/AZ_ObstacleSensorComponent.h"

#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "MoverComponent.h"
#include "MoverDataModelTypes.h"   // FCharacterDefaultInputs, FMoverInputCmdContext
#include "Character/AZ_PawnMoverHeroCharacter.h"   // GetWorldMoveIntentRaw (pre-clamp intent source)

UAZ_ObstacleSensorComponent::UAZ_ObstacleSensorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	// Tick before physics/anim so the flags are fresh when the AnimInstance reads them this frame.
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UAZ_ObstacleSensorComponent::BeginPlay()
{
	Super::BeginPlay();
	if (AActor* Owner = GetOwner())
	{
		MoverComp = Owner->FindComponentByClass<UMoverComponent>();
		Capsule   = Owner->FindComponentByClass<UCapsuleComponent>();
	}
}

void UAZ_ObstacleSensorComponent::ClearSenseOutputs()
{
	bObstacleAhead       = false;
	CurrentReaction      = EAZ_ObstacleReaction::None;
	ObstacleDistance     = 0.f;
	ObstacleClosingSpeed = 0.f;
	ObstacleNormal       = FVector::ZeroVector;
	ObstacleActor        = nullptr;
	// Reaction state — reset so a fresh obstacle contact re-decides the entry (rising edge) cleanly.
	EntryReactionEndTime = -1.f;
	LatchedEntry         = EAZ_ObstacleReaction::None;
	bWasWithinTrigger    = false;
	ApproachClosingSpeed = 0.f;
}

void UAZ_ObstacleSensorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UWorld* World = GetWorld();
	UMoverComponent* Mover = MoverComp.Get();
	AActor* Owner = GetOwner();
	if (!World || !Mover || !Owner)
	{
		ClearSenseOutputs();
		return;
	}

	const float Now = World->GetTimeSeconds();

	// ---- Gate: grounded + forward movement INTENT (not speed — a pinned pawn has speed ~0 but intent held, and
	// we still need to keep sensing the obstacle to report Blocked + detect when you turn off it). ----
	const bool bGrounded = Mover->GetMovementModeName() == TEXT("Walking");
	FVector MoveIntent = FVector::ZeroVector;
	if (const AAZ_PawnMoverHeroCharacter* HeroPawn = Cast<AAZ_PawnMoverHeroCharacter>(Owner))
	{
		// RAW intent (captured in ProduceInput BEFORE the movement-capability clamp). Reading the clamped Mover
		// input cmd instead would self-blind us: a straight-in wall zeroes that cmd, so we'd lose the wall the
		// very frame we need to fire the impact flinch.
		MoveIntent = HeroPawn->GetWorldMoveIntentRaw();
	}
	else
	{
		const FMoverInputCmdContext& LastInput = Mover->GetLastInputCmd();
		if (const FCharacterDefaultInputs* Chr = LastInput.InputCollection.FindDataByType<FCharacterDefaultInputs>())
		{
			MoveIntent = Chr->GetMoveInput_WorldSpace();
		}
	}
	MoveIntent.Z = 0.f;
	const bool bHasIntent = MoveIntent.SizeSquared() > FMath::Square(0.1f);
	if (!bGrounded || !bHasIntent)
	{
		ClearSenseOutputs();   // released input / airborne -> no reaction
		return;
	}

	const FVector Dir = MoveIntent.GetSafeNormal();
	const FVector Loc = Owner->GetActorLocation();
	const float HalfHeight = Capsule.IsValid() ? Capsule->GetScaledCapsuleHalfHeight() : 90.f;
	const float Radius     = TraceRadius > 0.f ? TraceRadius
		: (Capsule.IsValid() ? Capsule->GetScaledCapsuleRadius() : 22.f);
	const float FootZ      = Loc.Z - HalfHeight;
	const FVector Vel      = Mover->GetVelocity();

	FCollisionQueryParams Params(SCENE_QUERY_STAT(AZ_ObstacleSensor), /*bTraceComplex*/ false, Owner);

	// ---- Step-over test: a vertical face is only an OBSTACLE if the pawn could not simply walk/step onto it.
	// The face test alone cannot tell them apart, because a sphere sweep that clips the TOP EDGE of a box reports
	// that box's SIDE-face normal — perfectly vertical — even though the "obstacle" is a step you are already
	// walking up. Measured 2026-09-06 on the L_001 ramp: SM_Ramp (27 deg) delivers onto SM_Cube whose top is Z=200;
	// with the pawn's feet at Z=191.4 the LOW sphere (centre foot+30, radius 22) reached down to Z=199.4, dipped
	// 0.6cm below the platform top, and reported n=(0,1,0) -> Stumble, 44cm out, closing 450 — the pawn was tripped
	// by the top edge of the platform it was about to walk onto, 8.6cm above its own feet.
	//
	// So measure the thing's height at ITS OWN BASE (the ground at its foot), NEVER relative to our feet: on stairs
	// the pawn stands a tread below, so a feet-relative height reads two 18cm risers as one 36cm barrier and would
	// flinch on every staircase; base-relative reads 18 and steps up. Two ways to still be a real obstacle:
	//   - it is TALLER than MinObstacleHeight at its base (a crate, a wall), or
	//   - we struck it well ABOVE the local ground (a knee-height bar with floor beneath it — its "height" measured
	//     past the face is the floor, so the height test alone would wrongly clear it).
	auto IsSteppable = [&](const FHitResult& H) -> bool
	{
		// Sampled just before / just after the face: far enough to clear it, close enough to stay on the same step.
		constexpr float Inset  = 8.f;
		constexpr float ProbeUp   = 250.f;   // start above head height; a wall taller than this starts the trace
		constexpr float ProbeDown = 120.f;   // inside it -> penetrating hit -> reads as tall -> treated as obstacle
		const FVector Up(0.f, 0.f, 1.f);
		const FVector Face = H.ImpactPoint;

		// Ground on OUR side, at the foot of the face = the obstacle's base.
		float BaseZ = FootZ;
		const FVector BasePt = Face - Dir * Inset;
		FHitResult G;
		if (World->LineTraceSingleByChannel(G, BasePt + Up * ProbeUp, BasePt - Up * ProbeDown,
			TraceChannel.GetValue(), Params))
		{
			BaseZ = G.ImpactPoint.Z;
		}

		// What is on TOP of it, just past the face.
		const FVector TopPt = Face + Dir * Inset;
		FHitResult T;
		if (!World->LineTraceSingleByChannel(T, TopPt + Up * ProbeUp, TopPt - Up * ProbeDown,
			TraceChannel.GetValue(), Params))
		{
			return false;   // nothing to land on (a drop, or taller than the probe) -> a real obstacle
		}

		const float ObstacleHeight   = T.ImpactPoint.Z - BaseZ;   // how tall the thing is, at its own base
		const float ContactAboveBase = Face.Z - BaseZ;            // how far up it we struck
		return ObstacleHeight <= MinObstacleHeight && ContactAboveBase <= MinObstacleHeight;
	};

	// ---- One forward sphere-sweep at a body height. Returns true on a VERTICAL, OPPOSING face that is also too
	// TALL to step onto (a real obstacle, not a floor/ramp/stair/back wall) and fills the band's distance / normal
	// / closing speed / hit actor. ----
	auto Probe = [&](float HeightAboveFoot, float& OutDist, FVector& OutN, float& OutClose, AActor*& OutActor) -> bool
	{
		const FVector O(Loc.X, Loc.Y, FootZ + HeightAboveFoot);
		const FVector E = O + Dir * TraceDistance;
		FHitResult H;
		const bool bH = World->SweepSingleByChannel(H, O, E, FQuat::Identity, TraceChannel.GetValue(),
			FCollisionShape::MakeSphere(Radius), Params);
		if (bH && H.bBlockingHit
			&& FMath::Abs(H.ImpactNormal.Z) <= MaxWallNormalZ                       // mostly-horizontal = vertical face
			&& FVector::DotProduct(H.ImpactNormal, Dir) < 0.f)                       // faces back toward us
		{
			if (IsSteppable(H))
			{
				// Seen and deliberately ignored — you walk onto this. Green so it reads apart from a real hit.
				if (bDrawDebug) { DrawDebugSphere(World, H.ImpactPoint, 6.f, 6, FColor::Green, false, 0.f); }
				return false;
			}
			OutDist  = H.Distance;
			OutN     = H.ImpactNormal;
			OutClose = FMath::Max(0.f, static_cast<float>(FVector::DotProduct(Vel, -H.ImpactNormal)));
			OutActor = H.GetActor();
			if (bDrawDebug) { DrawDebugSphere(World, H.ImpactPoint, 6.f, 6, FColor::Red, false, 0.f); }
			return true;
		}
		if (bDrawDebug) { DrawDebugLine(World, O, E, FColor::White, false, 0.f, 0, 0.5f); }
		return false;
	};

	float   LowD = 0.f, MidD = 0.f, HighD = 0.f, LowC = 0.f, MidC = 0.f, HighC = 0.f;
	FVector LowN = FVector::ZeroVector, MidN = FVector::ZeroVector, HighN = FVector::ZeroVector;
	AActor* LowA = nullptr; AActor* MidA = nullptr; AActor* HighA = nullptr;

	// Cap each band to the CURRENT capsule top so the bands SHRINK when crouched — otherwise the fixed HIGH band
	// (165 cm) still hits an overhead beam the crouched (shorter) character clears, blocking it. Standing (top ~180)
	// is unaffected (all bands sit below it); crouched (top ~100-120) drops HIGH/MID below the beam → you pass under.
	const float BandCap = FMath::Max(10.f, 2.f * HalfHeight - 5.f);   // just under the capsule top
	const bool bLow  = Probe(FMath::Min(LowProbeHeight,  BandCap), LowD,  LowN,  LowC,  LowA);
	const bool bMid  = Probe(FMath::Min(MidProbeHeight,  BandCap), MidD,  MidN,  MidC,  MidA);
	const bool bHigh = Probe(FMath::Min(HighProbeHeight, BandCap), HighD, HighN, HighC, HighA);

	// ---- Classify by WHICH body band is blocked (MID dominant > LOW > HIGH). MID (chest) blocked = a WALL even if
	// low/high also hit (a full wall blocks all three). Mid clear + low = a low barrier. Mid+low clear + high = an
	// overhead beam. EntryKind = the reaction a FAST hit would fire for that band. ----
	EAZ_ObstacleReaction EntryKind = EAZ_ObstacleReaction::None;
	bool    bPrimary = false;
	float   PrimD = 0.f, PrimC = 0.f;
	FVector PrimN = FVector::ZeroVector;
	AActor* PrimA = nullptr;
	if (bMid)        { bPrimary = true; PrimD = MidD;  PrimN = MidN;  PrimC = MidC;  PrimA = MidA;  EntryKind = EAZ_ObstacleReaction::Brace;   }
	else if (bLow)   { bPrimary = true; PrimD = LowD;  PrimN = LowN;  PrimC = LowC;  PrimA = LowA;  EntryKind = EAZ_ObstacleReaction::Stumble; }
	else if (bHigh)  { bPrimary = true; PrimD = HighD; PrimN = HighN; PrimC = HighC; PrimA = HighA; EntryKind = EAZ_ObstacleReaction::HeadHit; }

	bObstacleAhead       = bPrimary;
	ObstacleDistance     = bPrimary ? PrimD : 0.f;
	ObstacleNormal       = PrimN;
	ObstacleClosingSpeed = bPrimary ? PrimC : 0.f;
	// Was declared and cleared but NEVER assigned — every consumer read None (that is why the diagnostic probes
	// could not name what the pawn hit). The hit actor is the hook for per-obstacle semantics later.
	ObstacleActor        = PrimA;

	// PEAK approach speed: highest closing speed seen while this obstacle is ahead. The entry gate uses THIS, not
	// the instantaneous PrimC: the movement-capability clamp decelerates the resolved velocity as you near a wall
	// it detects, so by the trigger frame GetVelocity() is already bled off and the instantaneous closing speed
	// fails the gate even on a full-speed run (that's why a HIGH/MID wall — caught by the clamp — never fired, while
	// a LOW bar — ignored by the clamp — did). The peak is captured at TraceDistance, before the clamp engages.
	if (bPrimary) { ApproachClosingSpeed = FMath::Max(ApproachClosingSpeed, PrimC); }
	else          { ApproachClosingSpeed = 0.f; }

	const bool bWithinTrigger = bPrimary && PrimD <= ImpactTriggerDistance;

	// ---- Entry reaction: decided ONCE on the rising edge of reaching the obstacle, so the CLOSING SPEED at contact
	// picks hit-vs-blocked. FAST -> the band's one-shot (Brace/Stumble/HeadHit, held its hold time); SLOW -> no
	// entry -> falls straight to Blocked (AnimInstance cancels intent so the SM does its own stop -> idle). ----
	if (bWithinTrigger && !bWasWithinTrigger)
	{
		const float SpeedGate = (EntryKind == EAZ_ObstacleReaction::Brace) ? ImpactMinSpeed : StumbleMinSpeed;
		if (ApproachClosingSpeed > SpeedGate)   // PEAK approach speed (not the clamp-bled instantaneous PrimC)
		{
			// CLIP-DRIVEN HOLD: the reaction's duration is owned by the SM, which holds LocomotionLoop until the
			// CHT-selected reaction clip is almost done (NotifyReactionClipPushed). The sensor only needs to keep
			// CurrentReaction != None briefly so the AnimInstance picks it up + pushes the clip; after that the SM's
			// clip-driven ReactionEndTime carries the hold. So the per-reaction hold-time PARAMS were removed.
			//
			// --- superseded per-reaction hold-time solution (kept for reference) ---
			// float HoldTime = StumbleHoldTime;
			// if (EntryKind == EAZ_ObstacleReaction::Brace)        { HoldTime = ImpactHoldTime; }
			// else if (EntryKind == EAZ_ObstacleReaction::HeadHit) { HoldTime = HeadHoldTime;   }
			// EntryReactionEndTime = Now + HoldTime;
			constexpr float ReactionTriggerWindow = 0.2f;   // brief bootstrap; the clip length owns the real hold
			LatchedEntry         = EntryKind;
			EntryReactionEndTime = Now + ReactionTriggerWindow;
		}
		else
		{
			LatchedEntry         = EAZ_ObstacleReaction::None;
			EntryReactionEndTime = -1.f;
		}
	}
	bWasWithinTrigger = bWithinTrigger;

	// ---- Resolve CurrentReaction — IMPACT-ONLY. There is NO sustained "Blocked" state anymore: the movement-
	// capability clamp owns "can't move into the wall" (it zeroes / slides the intent upstream), so this sensor
	// only adds the cosmetic flinch on a fast hit. Entry one-shot plays out on the rising edge, then clears to
	// None; a fresh fast approach re-arms it. ----
	if (!bWithinTrigger)
	{
		CurrentReaction = EAZ_ObstacleReaction::None;
		EntryReactionEndTime = -1.f;
	}
	else if (EntryReactionEndTime > 0.f && Now < EntryReactionEndTime)
	{
		CurrentReaction = LatchedEntry;                      // Brace / Stumble / HeadHit one-shot playing out
	}
	else
	{
		CurrentReaction = EAZ_ObstacleReaction::None;        // reached, but no fresh fast hit -> no reaction
	}

	if (bDrawDebug && GEngine)
	{
		const FColor C = CurrentReaction == EAZ_ObstacleReaction::Brace ? FColor::Orange
			: (CurrentReaction == EAZ_ObstacleReaction::None ? FColor::Silver : FColor::Yellow);
		GEngine->AddOnScreenDebugMessage(reinterpret_cast<uint64>(this), 1.f, C,
			FString::Printf(TEXT("[Obstacle] bands L%d M%d H%d  dist=%.0f close=%.0f peak=%.0f reaction=%d (0None 1Brace 4Stumble 8HeadHit)"),
				bLow ? 1 : 0, bMid ? 1 : 0, bHigh ? 1 : 0, ObstacleDistance, ObstacleClosingSpeed, ApproachClosingSpeed,
				static_cast<int32>(CurrentReaction)));
	}
}
