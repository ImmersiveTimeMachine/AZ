// Copyright Artur. AZ project.
#include "AbilitySystem/AZ_MeleeEnvironment.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAZMeleeEnvironmentClearanceTest, "AZ.Melee.Environment.Clearance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAZMeleeEnvironmentClearanceTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Isolated collision world"), World)) return false;
	AActor* Avatar = World->SpawnActor<AActor>();
	UCapsuleComponent* Capsule = NewObject<UCapsuleComponent>(Avatar);
	Avatar->SetRootComponent(Capsule);
	Capsule->InitCapsuleSize(25.f, 90.f);
	Capsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Capsule->SetCollisionObjectType(ECC_Pawn);
	Capsule->SetCollisionResponseToAllChannels(ECR_Block);
	Capsule->RegisterComponent();
	Avatar->SetActorLocation(FVector(0.f, 0.f, 90.f));
	auto Box = [World](const FVector& Centre, const FVector& Extent, ECollisionResponse PawnResponse,
		ECollisionEnabled::Type Enabled = ECollisionEnabled::QueryOnly)
	{
		AActor* Actor = World->SpawnActor<AActor>();
		UBoxComponent* Component = NewObject<UBoxComponent>(Actor);
		Actor->SetRootComponent(Component);
		Component->SetBoxExtent(Extent);
		Component->SetCollisionEnabled(Enabled);
		Component->SetCollisionObjectType(ECC_WorldStatic);
		Component->SetCollisionResponseToAllChannels(ECR_Ignore);
		Component->SetCollisionResponseToChannel(ECC_Pawn, PawnResponse);
		Component->RegisterComponent();
		Actor->SetActorLocation(Centre);
		return Actor;
	};
	Box(FVector(0.f, 0.f, -5.f), FVector(1000.f, 1000.f, 5.f), ECR_Block);
	Box(FVector(40.f, 0.f, 90.f), FVector(2.f, 50.f, 90.f), ECR_Overlap);
	Box(FVector(20.f, 0.f, 90.f), FVector(2.f, 50.f, 90.f), ECR_Block, ECollisionEnabled::PhysicsOnly);
	AActor* Attached = Box(FVector(30.f, 0.f, 90.f), FVector(2.f, 50.f, 90.f), ECR_Block);
	Attached->AttachToActor(Avatar, FAttachmentTransformRules::KeepWorldTransform);
	AActor* Wall = Box(FVector(100.f, 0.f, 90.f), FVector(5.f, 100.f, 90.f), ECR_Block);
	FHitResult Hit;
	TestTrue(TEXT("Scenery sweep finds wall past triggers, self attachments and physics-only bodies"),
		FAZ_MeleeEnvironment::SweepEnvironment(*Avatar, FVector(0.f, 0.f, 100.f), FVector(150.f, 0.f, 100.f), 5.f, Hit));
	TestEqual(TEXT("First eligible contact is the wall"), Hit.GetActor(), Wall);
	TestTrue(TEXT("Contact time includes sphere radius"), FMath::IsNearlyEqual(Hit.Time, 0.6f, 0.01f));
	TestTrue(TEXT("Standing capsule destination is clear above supporting floor"),
		FAZ_MeleeEnvironment::IsCapsulePathClear(*Avatar, Avatar->GetActorLocation(), Avatar->GetActorLocation(), Hit));
	TestTrue(TEXT("Body can approach while preserving full radius"),
		FAZ_MeleeEnvironment::IsCapsulePathClear(*Avatar, Avatar->GetActorLocation(), FVector(50.f, 0.f, 90.f), Hit));
	TestFalse(TEXT("Body path cannot cross a wall"),
		FAZ_MeleeEnvironment::IsCapsulePathClear(*Avatar, Avatar->GetActorLocation(), FVector(140.f, 0.f, 90.f), Hit));
	TestFalse(TEXT("Occupied capsule destination is rejected even with zero travel"),
		FAZ_MeleeEnvironment::IsCapsulePathClear(*Avatar, FVector(100.f, 0.f, 90.f), FVector(100.f, 0.f, 90.f), Hit));
	TestFalse(TEXT("Planted foot contact does not count as striking the floor"),
		FAZ_MeleeEnvironment::SweepEnvironment(*Avatar, FVector(0.f, 0.f, 4.f), FVector(1.f, 0.f, 4.f), 6.f, Hit, true));
	AActor* Table = Box(FVector(0.f, 0.f, 50.f), FVector(20.f, 20.f, 5.f), ECR_Block);
	TestTrue(TEXT("Floor filtering does not hide a table top"),
		FAZ_MeleeEnvironment::SweepEnvironment(*Avatar, FVector(0.f, 0.f, 80.f), FVector(0.f, 0.f, 40.f), 3.f, Hit, true));
	TestEqual(TEXT("Downward punch contacts the table"), Hit.GetActor(), Table);
	World->DestroyWorld(false);
	return true;
}
#endif
