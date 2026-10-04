// Copyright Artur. AZ project.
// The "AZ Natural Grip" tab: pick a weapon-hand profile, edit it, run the solver stages in the background, read the log.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class FNGJobRunner;
class IDetailsView;
class SMultiLineEditableTextBox;
class SVerticalBox;
class UAZNaturalGripProfile;
struct FAssetData;

class SAZNaturalGripPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAZNaturalGripPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FString GetProfilePath() const;
	void OnProfileChanged(const FAssetData& Asset);
	void OnDetailsChanged(const FPropertyChangedEvent& Event);
	void RebuildStageButtons();

	FReply OnRunStage(FString Stage);
	FReply OnApply(bool bDryRun);
	FReply OnCreateProfiles();
	FText GetWeaponTypeText() const;
	FReply OnCancel();
	FReply OnClearLog();
	bool CanRun() const;

	FText GetStatusText() const;
	TOptional<float> GetProgress() const;
	void AppendLog(const FString& Text);

	TWeakObjectPtr<UAZNaturalGripProfile> Profile;
	TSharedPtr<IDetailsView> Details;
	TSharedPtr<FNGJobRunner> Runner;
	TSharedPtr<SMultiLineEditableTextBox> LogBox;
	TSharedPtr<SVerticalBox> StageBox;
	FString LogText;
	FString LastResult;
	int32 Threads = 0;
	TArray<TSharedPtr<int32>> WeaponTypes;          // EAZWeaponGripType values for the combo
	int32 WeaponType = 0;
};
