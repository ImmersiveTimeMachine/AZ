// Copyright Artur. AZ project.

#include "Camera/AZ_GrabCameraShakes.h"

#include "Shakes/PerlinNoiseCameraShakePattern.h"
#include "Shakes/WaveOscillatorCameraShakePattern.h"

UAZ_CameraShake_GrabRumble::UAZ_CameraShake_GrabRumble(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	bSingleInstance = true;   // restart = retune (the curve ramp), never a second stacked rumble

	UPerlinNoiseCameraShakePattern* Pattern =
		ObjInit.CreateDefaultSubobject<UPerlinNoiseCameraShakePattern>(this, TEXT("RootShakePattern"));
	Pattern->Duration = 0.f;        // infinite — GA_PlayerGrabbed stops it on resolve
	Pattern->BlendInTime = 0.4f;
	Pattern->BlendOutTime = 0.4f;
	// Location: small chaotic push — reads as being wrestled, not an earthquake.
	Pattern->X.Amplitude = 1.2f;   Pattern->X.Frequency = 9.f;
	Pattern->Y.Amplitude = 1.2f;   Pattern->Y.Frequency = 11.f;
	Pattern->Z.Amplitude = 1.8f;   Pattern->Z.Frequency = 10.f;
	// Rotation: the real seller at a close boom — degrees, keep subtle.
	Pattern->Pitch.Amplitude = 0.6f; Pattern->Pitch.Frequency = 8.f;
	Pattern->Yaw.Amplitude = 0.5f;   Pattern->Yaw.Frequency = 7.f;
	Pattern->Roll.Amplitude = 0.4f;  Pattern->Roll.Frequency = 6.f;
	SetRootShakePattern(Pattern);
}

UAZ_CameraShake_GrabJolt::UAZ_CameraShake_GrabJolt(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	UWaveOscillatorCameraShakePattern* Pattern =
		ObjInit.CreateDefaultSubobject<UWaveOscillatorCameraShakePattern>(this, TEXT("RootShakePattern"));
	Pattern->Duration = 0.3f;
	Pattern->BlendInTime = 0.02f;   // near-instant hit...
	Pattern->BlendOutTime = 0.2f;   // ...decaying fast
	Pattern->Z.Amplitude = 3.f;     Pattern->Z.Frequency = 18.f;
	Pattern->Pitch.Amplitude = 1.5f; Pattern->Pitch.Frequency = 15.f;
	Pattern->Roll.Amplitude = 0.8f;  Pattern->Roll.Frequency = 14.f;
	SetRootShakePattern(Pattern);
}
