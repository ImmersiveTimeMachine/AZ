// Copyright Artur. AZ project.

#include "SAZNaturalGripPanel.h"

#include "AZNaturalGripProfile.h"
#include "AssetRegistry/AssetData.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "IDetailsView.h"
#include "Modules/ModuleManager.h"
#include "AssetToolsModule.h"
#include "Factories/DataAssetFactory.h"
#include "IAssetTools.h"
#include "Engine/SkeletalMesh.h"
#include "FileHelpers.h"
#include "Misc/MessageDialog.h"
#include "UObject/Package.h"
#include "Widgets/Input/SComboBox.h"
#include "NGApply.h"
#include "NGJobRunner.h"
#include "NGJobs.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyEditorModule.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "AZNaturalGrip"

namespace
{
	FText StageLabel(const FString& Stage)
	{
		if (Stage == TEXT("fields")) return LOCTEXT("StFields", "Bake fields");
		if (Stage == TEXT("power")) return LOCTEXT("StPower", "Power grasp (support hand)");
		if (Stage == TEXT("stage1")) return LOCTEXT("StStage1", "Stage 1: placements");
		if (Stage == TEXT("stage2")) return LOCTEXT("StStage2", "Stage 2: fingers (top N)");
		if (Stage == TEXT("final")) return LOCTEXT("StFinal", "Final solve");
		if (Stage == TEXT("polish")) return LOCTEXT("StPolish", "Final solve + polish (B)");
		if (Stage == TEXT("inputs")) return LOCTEXT("StInputs", "Check asset inputs");
		if (Stage == TEXT("placement")) return LOCTEXT("StPlacement", "Placement search");
		if (Stage == TEXT("solve")) return LOCTEXT("StSolve", "Final solve");
		if (Stage == TEXT("parity")) return LOCTEXT("StParity", "Parity check vs Python");
		return FText::FromString(Stage);
	}
}

void SAZNaturalGripPanel::Construct(const FArguments& InArgs)
{
	Runner = MakeShared<FNGJobRunner>();
	for (int32 I = 0; I < StaticEnum<EAZWeaponGripType>()->NumEnums() - 1; ++I)
	{
		WeaponTypes.Add(MakeShared<int32>(static_cast<int32>(StaticEnum<EAZWeaponGripType>()->GetValueByIndex(I))));
	}

	FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	FDetailsViewArgs Args;
	Args.bAllowSearch = true;
	Args.bHideSelectionTip = true;
	Args.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	Details = PropertyEditor.CreateDetailView(Args);
	// the stage buttons depend on the hand
	Details->OnFinishedChangingProperties().AddSP(this, &SAZNaturalGripPanel::OnDetailsChanged);

	ChildSlot
	[
		SNew(SVerticalBox)
		// profile picker
		+ SVerticalBox::Slot().AutoHeight().Padding(6.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)
			[
				SNew(STextBlock).Text(LOCTEXT("Profile", "Weapon hand profile"))
			]
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				SNew(SObjectPropertyEntryBox)
				.AllowedClass(UAZNaturalGripProfile::StaticClass())
				.ObjectPath(this, &SAZNaturalGripPanel::GetProfilePath)
				.OnObjectChanged(this, &SAZNaturalGripPanel::OnProfileChanged)
				.AllowClear(true)
				.DisplayThumbnail(false)
			]
		]
		// weapon type -> profiles for each hand role
		+ SVerticalBox::Slot().AutoHeight().Padding(6.f, 0.f, 6.f, 6.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)
			[
				SNew(STextBlock).Text(LOCTEXT("WeaponType", "Weapon type"))
			]
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				SNew(SComboBox<TSharedPtr<int32>>)
				.OptionsSource(&WeaponTypes)
				.OnGenerateWidget_Lambda([](TSharedPtr<int32> V)
				{
					return SNew(STextBlock).Text(StaticEnum<EAZWeaponGripType>()->GetDisplayNameTextByValue(*V));
				})
				.OnSelectionChanged_Lambda([this](TSharedPtr<int32> V, ESelectInfo::Type)
				{
					if (V.IsValid())
					{
						WeaponType = *V;
					}
				})
				[
					SNew(STextBlock).Text(this, &SAZNaturalGripPanel::GetWeaponTypeText)
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(8.f, 0.f, 0.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("CreateProfiles", "Create profiles"))
				.ToolTipText(LOCTEXT("CreateProfilesTip", "Creates one profile per hand role of this weapon type in /AZNaturalGrip/Profiles (NGP_<WeaponKey>_<hand>), copied from the profile picked above (weapon mesh, blueprint, clips...) or new when none is picked."))
				.OnClicked(this, &SAZNaturalGripPanel::OnCreateProfiles)
			]
		]
		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			SNew(SSplitter).Orientation(Orient_Vertical)
			+ SSplitter::Slot().Value(0.45f)
			[
				Details.ToSharedRef()
			]
			+ SSplitter::Slot().Value(0.55f)
			[
				SNew(SVerticalBox)
				// stage buttons
				+ SVerticalBox::Slot().AutoHeight().Padding(6.f, 6.f, 6.f, 2.f)
				[
					SAssignNew(StageBox, SVerticalBox)
				]
				// threads, cancel, status
				+ SVerticalBox::Slot().AutoHeight().Padding(6.f, 2.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
					[
						SNew(STextBlock).Text(LOCTEXT("Threads", "Threads (0 = all)"))
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SBox).WidthOverride(70.f)
						[
							SNew(SSpinBox<int32>)
							.MinValue(0).MaxValue(256)
							.Value_Lambda([this]() { return Threads; })
							.OnValueChanged_Lambda([this](int32 V) { Threads = V; })
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(8.f, 0.f)
					[
						SNew(SButton)
						.Text(LOCTEXT("Cancel", "Cancel"))
						.IsEnabled_Lambda([this]() { return Runner->IsRunning(); })
						.OnClicked(this, &SAZNaturalGripPanel::OnCancel)
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton)
						.Text(LOCTEXT("ClearLog", "Clear log"))
						.OnClicked(this, &SAZNaturalGripPanel::OnClearLog)
					]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(8.f, 0.f)
					[
						SNew(STextBlock).Text(this, &SAZNaturalGripPanel::GetStatusText)
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(6.f, 2.f)
				[
					SNew(SProgressBar).Percent(this, &SAZNaturalGripPanel::GetProgress)
				]
				// log
				+ SVerticalBox::Slot().FillHeight(1.f).Padding(6.f)
				[
					SAssignNew(LogBox, SMultiLineEditableTextBox)
					.IsReadOnly(true)
					.AlwaysShowScrollbars(true)
					.Font(FAppStyle::GetFontStyle(TEXT("MonoFont")))
					.Text_Lambda([this]() { return FText::FromString(LogText); })
				]
			]
		]
	];
	RebuildStageButtons();
}

FString SAZNaturalGripPanel::GetProfilePath() const
{
	return Profile.IsValid() ? Profile->GetPathName() : FString();
}

void SAZNaturalGripPanel::OnProfileChanged(const FAssetData& Asset)
{
	Profile = Cast<UAZNaturalGripProfile>(Asset.GetAsset());
	Details->SetObject(Profile.Get());
	RebuildStageButtons();
}

void SAZNaturalGripPanel::OnDetailsChanged(const FPropertyChangedEvent& Event)
{
	RebuildStageButtons();
}

void SAZNaturalGripPanel::RebuildStageButtons()
{
	StageBox->ClearChildren();
	if (!Profile.IsValid())
	{
		StageBox->AddSlot().AutoHeight()
		[
			SNew(STextBlock).Text(LOCTEXT("PickProfile", "Pick a weapon hand profile (Content Browser: Miscellaneous > Data Asset > AZNaturalGripProfile)."))
		];
		return;
	}
	const EAZGripHand Role = Profile->Hand;
	const TCHAR* SolveStage = Profile->IsTriggerRole() ? TEXT("trigger")
		: ((Role == EAZGripHand::Handle || Role == EAZGripHand::PistolCup) ? TEXT("power") : TEXT("support"));
	TSharedRef<SWrapBox> Main = SNew(SWrapBox).UseAllottedSize(true);
	Main->AddSlot().Padding(2.f)
	[
		SNew(SButton)
		.Text(LOCTEXT("SolveWeapon", "Solve weapon hand"))
		.ToolTipText(LOCTEXT("SolveWeaponTip", "The whole chain for this hand: placement search, fingers, final solve on the best placement."))
		.IsEnabled(this, &SAZNaturalGripPanel::CanRun)
		.OnClicked(this, &SAZNaturalGripPanel::OnRunStage, FString(SolveStage))
	];
	TSharedRef<SWrapBox> Diag = SNew(SWrapBox).UseAllottedSize(true);
	for (const FString& Stage : NGJobs::StagesFor(Role))
	{
		Diag->AddSlot().Padding(2.f)
		[
			SNew(SButton)
			.Text(StageLabel(Stage))
			.IsEnabled(this, &SAZNaturalGripPanel::CanRun)
			.OnClicked(this, &SAZNaturalGripPanel::OnRunStage, Stage)
		];
	}
	Main->AddSlot().Padding(2.f)
	[
		SNew(SButton)
		.Text(LOCTEXT("ApplyDry", "Apply: dry run"))
		.ToolTipText(LOCTEXT("ApplyDryTip", "Show what Apply would write (from the last final solve of this profile). Writes nothing."))
		.IsEnabled(this, &SAZNaturalGripPanel::CanRun)
		.OnClicked(this, &SAZNaturalGripPanel::OnApply, true)
	];
	Main->AddSlot().Padding(2.f)
	[
		SNew(SButton)
		.Text(LOCTEXT("Apply", "Apply..."))
		.ToolTipText(LOCTEXT("ApplyTip", "Write the last final solve into the weapon's assets: backup, write, save, read back. Asks first."))
		.IsEnabled(this, &SAZNaturalGripPanel::CanRun)
		.OnClicked(this, &SAZNaturalGripPanel::OnApply, false)
	];
	StageBox->AddSlot().AutoHeight()[Main];
	StageBox->AddSlot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)[Diag];
}

FText SAZNaturalGripPanel::GetWeaponTypeText() const
{
	return StaticEnum<EAZWeaponGripType>()->GetDisplayNameTextByValue(WeaponType);
}

FReply SAZNaturalGripPanel::OnCreateProfiles()
{
	struct FRole
	{
		EAZGripHand Role;
		EAZGripSide Side;
		const TCHAR* Suffix;
	};
	TArray<FRole> Roles;
	switch (static_cast<EAZWeaponGripType>(WeaponType))
	{
	case EAZWeaponGripType::Rifle: Roles = {{EAZGripHand::Trigger, EAZGripSide::Right, TEXT("Right")}, {EAZGripHand::Support, EAZGripSide::Left, TEXT("Left")}}; break;
	case EAZWeaponGripType::Shotgun: Roles = {{EAZGripHand::TriggerStraight, EAZGripSide::Right, TEXT("Right")}, {EAZGripHand::Support, EAZGripSide::Left, TEXT("Left")}}; break;
	case EAZWeaponGripType::Pistol: Roles = {{EAZGripHand::Trigger, EAZGripSide::Right, TEXT("Right")}, {EAZGripHand::PistolCup, EAZGripSide::Left, TEXT("Left")}}; break;
	case EAZWeaponGripType::Knife: Roles = {{EAZGripHand::Handle, EAZGripSide::Right, TEXT("Right")}}; break;
	case EAZWeaponGripType::TwoHandMelee: Roles = {{EAZGripHand::Handle, EAZGripSide::Right, TEXT("Right")}, {EAZGripHand::Handle, EAZGripSide::Left, TEXT("Left")}}; break;
	}
	// the weapon: the skeletal mesh selected in the Content Browser (else the picked profile's weapon)
	USkeletalMesh* Selected = GEditor ? GEditor->GetSelectedObjects()->GetTop<USkeletalMesh>() : nullptr;
	FString Key = Profile.IsValid() ? Profile->WeaponKey : TEXT("weapon");
	if (Selected)
	{
		Key = Selected->GetName();
	}
	else if (Profile.IsValid())
	{
		if (USkeletalMesh* Mesh = Profile->WeaponMesh.LoadSynchronous())
		{
			Key = Mesh->GetName();
		}
	}
	IAssetTools& Tools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
	const FString Path = TEXT("/AZNaturalGrip/Profiles");
	UAZNaturalGripProfile* First = nullptr;
	TArray<UPackage*> ToSave;
	for (const FRole& R : Roles)
	{
		const FString Name = FString::Printf(TEXT("NGP_%s_%s"), *Key, R.Suffix);
		UAZNaturalGripProfile* P = LoadObject<UAZNaturalGripProfile>(nullptr, *(Path / Name + TEXT(".") + Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
		const bool bExisted = P != nullptr;
		if (P)
		{
			AppendLog(FString::Printf(TEXT("profile %s exists - kept as it is"), *Name));
		}
		else if (Profile.IsValid())
		{
			P = Cast<UAZNaturalGripProfile>(Tools.DuplicateAsset(Name, Path, Profile.Get()));
			if (P)
			{
				// a new weapon: the search starts at its clip hold, not at the M16's approved re-grip
				P->SearchCentre = FAZGripPlacement();
				P->WeaponKey = Key;
				if (Selected && Profile->WeaponMesh.ToSoftObjectPath() != FSoftObjectPath(Selected))
				{
					// another weapon: Apply must never write into the template weapon's assets, nor read its hold
					P->WeaponBlueprint.Reset();
					P->GripPoseOverride.Reset();
					P->ClipFolder.Path.Reset();
					P->WeaponSocketOnHero = NAME_None;
					P->OtherHand.Reset();
				}
			}
		}
		else
		{
			UDataAssetFactory* Factory = NewObject<UDataAssetFactory>();
			Factory->DataAssetClass = UAZNaturalGripProfile::StaticClass();
			P = Cast<UAZNaturalGripProfile>(Tools.CreateAsset(Name, Path, UAZNaturalGripProfile::StaticClass(), Factory));
			if (P)
			{
				P->SearchCentre = FAZGripPlacement();
				P->Input = EAZGripInputSource::Assets;
				P->WeaponKey = Key;
			}
		}
		if (!P)
		{
			AppendLog(FString::Printf(TEXT("ERROR cannot create %s"), *Name));
			continue;
		}
		if (!bExisted)
		{
			P->Modify();
			P->Hand = R.Role;
			P->Side = R.Side;
			P->Input = EAZGripInputSource::Assets;
			if (Selected)
			{
				P->WeaponMesh = Selected;
			}
		}
		if (!bExisted && R.Role == EAZGripHand::PistolCup && First)
		{
			P->OtherHand = First;                  // the pistol second hand lies on the right hand
		}
		ToSave.Add(P->GetOutermost());
		AppendLog(FString::Printf(TEXT("profile %s: %s"), *P->GetPathName(), *StaticEnum<EAZGripHand>()->GetDisplayNameTextByValue(static_cast<int64>(R.Role)).ToString()));
		First = First ? First : P;
	}
	UEditorLoadingAndSavingUtils::SavePackages(ToSave, true);
	AppendLog(TEXT("now fill in each new profile: Weapon Blueprint, Clip Folder + Weapon Socket On Hero (the hold), Weapon Forward; ")
		TEXT("weapon mesh marker sockets: trigger hand NG_Trigger + NG_GripFront + NG_ThumbLimit, handguard NG_Handguard + LeftHandGrip, ")
		TEXT("handle / pistol second hand NG_HandleBack + NG_HandleFront"));
	if (First)
	{
		Profile = First;
		Details->SetObject(First);
		RebuildStageButtons();
	}
	return FReply::Handled();
}

FReply SAZNaturalGripPanel::OnApply(bool bDryRun)
{
	if (!CanRun())
	{
		return FReply::Handled();
	}
	FString Plan;
	const bool bPlanOk = NGApply::Apply(*Profile, true, Plan);
	AppendLog(Plan);
	if (bDryRun || !bPlanOk)
	{
		return FReply::Handled();
	}
	const FText Question = FText::Format(LOCTEXT("ApplyAsk", "Write these changes? The files are backed up first.\n\n{0}"), FText::FromString(Plan));
	if (FMessageDialog::Open(EAppMsgType::YesNo, Question, LOCTEXT("ApplyTitle", "AZ Natural Grip: Apply")) != EAppReturnType::Yes)
	{
		AppendLog(TEXT("apply: cancelled"));
		return FReply::Handled();
	}
	FString Log;
	NGApply::Apply(*Profile, false, Log);
	AppendLog(Log);
	return FReply::Handled();
}

bool SAZNaturalGripPanel::CanRun() const
{
	return Profile.IsValid() && !Runner->IsRunning();
}

FReply SAZNaturalGripPanel::OnRunStage(FString Stage)
{
	if (!CanRun())
	{
		return FReply::Handled();
	}
	FNGProfileData Data;
	FString Error;
	if (!NGJobs::Snapshot(*Profile, Data, Error))
	{
		AppendLog(FString::Printf(TEXT("ERROR %s"), *Error));
		return FReply::Handled();
	}
	AppendLog(FString::Printf(TEXT("--- %s: %s"), *Profile->GetName(), *StageLabel(Stage).ToString()));
	TWeakPtr<SAZNaturalGripPanel> WeakThis = StaticCastSharedRef<SAZNaturalGripPanel>(AsShared());
	Runner->Start(StageLabel(Stage).ToString(), Threads,
		[Data = MoveTemp(Data), Stage](const ng::Exec& Ex) { return NGJobs::Run(Data, Stage, Ex); },
		[WeakThis](const FString& Log, bool bCancelled)
		{
			if (TSharedPtr<SAZNaturalGripPanel> This = WeakThis.Pin())
			{
				This->AppendLog(Log);
				This->LastResult = bCancelled ? TEXT("cancelled") : TEXT("done");
			}
		});
	return FReply::Handled();
}

FReply SAZNaturalGripPanel::OnCancel()
{
	Runner->Cancel();
	return FReply::Handled();
}

FReply SAZNaturalGripPanel::OnClearLog()
{
	LogText.Reset();
	return FReply::Handled();
}

FText SAZNaturalGripPanel::GetStatusText() const
{
	if (Runner->IsRunning())
	{
		return FText::FromString(FString::Printf(TEXT("%s ... %.1f s"), *Runner->GetStage(), Runner->GetElapsed()));
	}
	return FText::FromString(LastResult);
}

TOptional<float> SAZNaturalGripPanel::GetProgress() const
{
	if (!Runner->IsRunning())
	{
		return 0.f;
	}
	const float F = Runner->GetFraction();
	return F > 0.f ? TOptional<float>(F) : TOptional<float>();      // unset = marquee while the stage reports nothing
}

void SAZNaturalGripPanel::AppendLog(const FString& Text)
{
	LogText += Text;
	if (!LogText.EndsWith(TEXT("\n")))
	{
		LogText += TEXT("\n");
	}
	if (LogBox.IsValid())
	{
		LogBox->ScrollTo(ETextLocation::EndOfDocument);
	}
}

#undef LOCTEXT_NAMESPACE
