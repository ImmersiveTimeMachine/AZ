#include "Animation/AZ_ChooserUtils.h"

#if WITH_EDITOR
#include "Chooser.h"
#include "ChooserPropertyAccess.h"
#include "EnumColumn.h"
#include "MultiEnumColumn.h"
#include "OutputStructColumn.h"
#include "FloatRangeColumn.h"
#include "BoolColumn.h"
#include "RandomizeColumn.h"
#include "ObjectChooser_Asset.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "FileHelpers.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Animation/AnimSequenceBase.h"
#endif

#if WITH_EDITOR
static UChooserTable* LoadChooser(const FString& Path)
{
	return LoadObject<UChooserTable>(nullptr, *FString::Printf(TEXT("%s.%s"), *Path, *FPackageName::GetShortName(Path)));
}
#endif

bool UAZ_ChooserUtils::ConfigureAnimChooser(const FString& ChooserPath, UClass* AnimInstanceClass)
{
#if WITH_EDITOR
	UChooserTable* Table = LoadChooser(ChooserPath);
	if (!Table)
	{
		UE_LOG(LogTemp, Error, TEXT("AZ_ChooserUtils: Could not load Chooser: %s"), *ChooserPath);
		return false;
	}

	if (!AnimInstanceClass)
	{
		UE_LOG(LogTemp, Error, TEXT("AZ_ChooserUtils: AnimInstanceClass is null"));
		return false;
	}

	// Set result type
	Table->ResultType = EObjectChooserResultType::ObjectResult;
	Table->OutputObjectType = UAnimSequence::StaticClass();

	// Set context data — GASP pattern: 2 entries
	// Context[0]: AnimInstance class (ReadWrite) — read enum values for filtering
	// Context[1]: ChooserOutputs struct (Write) — output target for sub-tables
	Table->ContextData.SetNum(2);

	Table->ContextData[0].InitializeAs<FContextObjectTypeClass>();
	FContextObjectTypeClass& Ctx = Table->ContextData[0].GetMutable<FContextObjectTypeClass>();
	Ctx.Class = AnimInstanceClass;
	Ctx.Direction = EContextObjectDirection::ReadWrite;

	Table->ContextData[1].InitializeAs<FContextObjectTypeStruct>();
	FContextObjectTypeStruct& CtxStruct = Table->ContextData[1].GetMutable<FContextObjectTypeStruct>();
	CtxStruct.Struct = FAZ_ChooserOutputs::StaticStruct();
	CtxStruct.Direction = EContextObjectDirection::Write;

	// Clear existing columns — root table has only 3 filter columns (no output column)
	Table->ColumnsStructs.Empty();

	// Column 0: StateMachineState (Enum filter)
	{
		FInstancedStruct ColStruct;
		ColStruct.InitializeAs<FEnumColumn>();
		FEnumColumn& Col = ColStruct.GetMutable<FEnumColumn>();
		Col.InputValue.InitializeAs<FEnumContextProperty>();
		FEnumContextProperty& Prop = Col.InputValue.GetMutable<FEnumContextProperty>();
		Prop.Binding.PropertyBindingChain = { FName("StateMachineState") };
		Prop.Binding.ContextIndex = 0;
#if WITH_EDITORONLY_DATA
		Prop.Binding.Enum = StaticEnum<EAZ_StateMachineState>();
#endif
		Table->ColumnsStructs.Add(MoveTemp(ColStruct));
	}

	// Column 1: Gait (Enum filter)
	{
		FInstancedStruct ColStruct;
		ColStruct.InitializeAs<FEnumColumn>();
		FEnumColumn& Col = ColStruct.GetMutable<FEnumColumn>();
		Col.InputValue.InitializeAs<FEnumContextProperty>();
		FEnumContextProperty& Prop = Col.InputValue.GetMutable<FEnumContextProperty>();
		Prop.Binding.PropertyBindingChain = { FName("Gait") };
		Prop.Binding.ContextIndex = 0;
#if WITH_EDITORONLY_DATA
		Prop.Binding.Enum = StaticEnum<EAZ_Gait>();
#endif
		Table->ColumnsStructs.Add(MoveTemp(ColStruct));
	}

	// Column 2: Stance (Enum filter)
	{
		FInstancedStruct ColStruct;
		ColStruct.InitializeAs<FEnumColumn>();
		FEnumColumn& Col = ColStruct.GetMutable<FEnumColumn>();
		Col.InputValue.InitializeAs<FEnumContextProperty>();
		FEnumContextProperty& Prop = Col.InputValue.GetMutable<FEnumContextProperty>();
		Prop.Binding.PropertyBindingChain = { FName("Stance") };
		Prop.Binding.ContextIndex = 0;
#if WITH_EDITORONLY_DATA
		Prop.Binding.Enum = StaticEnum<EAZ_Stance>();
#endif
		Table->ColumnsStructs.Add(MoveTemp(ColStruct));
	}

	// No output column on root — output is handled by sub-table OutputStructColumns
	// writing to context[1] (FAZ_ChooserOutputs)

	Table->Compile(true);
	Table->MarkPackageDirty();

	UE_LOG(LogTemp, Log, TEXT("AZ_ChooserUtils: Configured Chooser with 3 filter columns + 1 output column"));
	return true;
#else
	return false;
#endif
}

bool UAZ_ChooserUtils::AddAnimRow(const FString& ChooserPath, UAnimSequence* AnimAsset,
	EAZ_StateMachineState StateMachineState, EAZ_Gait Gait, EAZ_Stance Stance,
	bool bUseMM, float BlendTime, FName BlendProfileName)
{
#if WITH_EDITOR
	UChooserTable* Table = LoadChooser(ChooserPath);
	if (!Table || !AnimAsset) return false;
	// HARD GUARD (audit P3-26): this legacy helper assumes the ORIGINAL 3-column layout (SMState/Gait/
	// Stance, all FEnumColumn) and appends cells ONLY to columns 0-2. On a wider table (the live CHT_v2
	// has 11 columns) that silently MISALIGNS every later row's cells — the engine indexes RowValues[r]
	// positionally. Refuse anything but exactly-3 enum columns; use AddEmptyRowToSub + SetCell* instead.
	if (Table->ColumnsStructs.Num() != 3)
	{
		UE_LOG(LogTemp, Error, TEXT("AZ_ChooserUtils: AddAnimRow requires the legacy EXACT 3-column layout "
			"(have %d). On wider tables use AddEmptyRowToSub + SetCell* — appending to a subset of columns "
			"misaligns all later rows."), Table->ColumnsStructs.Num());
		return false;
	}
	for (int32 ColIdx = 0; ColIdx < 3; ++ColIdx)
	{
		if (!Table->ColumnsStructs[ColIdx].GetPtr<FEnumColumn>())
		{
			UE_LOG(LogTemp, Error, TEXT("AZ_ChooserUtils: AddAnimRow — column %d is not an FEnumColumn; aborting "
				"(GetMutable would assert)."), ColIdx);
			return false;
		}
	}
	// NOTE: bUseMM/BlendTime/BlendProfileName are accepted for signature compatibility but NOT stored —
	// the root table has no OutputStruct column in this legacy layout (outputs live on sub-tables).

	// Add result row
	FInstancedStruct& Result = Table->ResultsStructs.AddDefaulted_GetRef();
	Result.InitializeAs<FAssetChooser>();
	Result.GetMutable<FAssetChooser>().Asset = AnimAsset;

	// Column 0: StateMachineState
	FEnumColumn& SMCol = Table->ColumnsStructs[0].GetMutable<FEnumColumn>();
	FChooserEnumRowData SMRow;
	SMRow.Value = static_cast<uint8>(StateMachineState);
	SMRow.Comparison = EEnumColumnCellValueComparison::MatchEqual;
	SMCol.RowValues.Add(SMRow);

	// Column 1: Gait
	FEnumColumn& GaitCol = Table->ColumnsStructs[1].GetMutable<FEnumColumn>();
	FChooserEnumRowData GaitRow;
	GaitRow.Value = static_cast<uint8>(Gait);
	GaitRow.Comparison = EEnumColumnCellValueComparison::MatchEqual;
	GaitCol.RowValues.Add(GaitRow);

	// Column 2: Stance
	FEnumColumn& StanceCol = Table->ColumnsStructs[2].GetMutable<FEnumColumn>();
	FChooserEnumRowData StanceRow;
	StanceRow.Value = static_cast<uint8>(Stance);
	StanceRow.Comparison = EEnumColumnCellValueComparison::MatchEqual;
	StanceCol.RowValues.Add(StanceRow);

	// Note: Output struct is NOT on root table — it's on sub-tables only (GASP pattern)
	// For flat table usage, the output data is ignored at root level

#if WITH_EDITORONLY_DATA
	Table->DisabledRows.Add(false);
#endif

	Table->MarkPackageDirty();
	return true;
#else
	return false;
#endif
}

int32 UAZ_ChooserUtils::AddDatabaseRows(const FString& ChooserPath, const FString& DatabasePath,
	EAZ_StateMachineState StateMachineState, EAZ_Gait Gait, EAZ_Stance Stance,
	bool bUseMM, float BlendTime, FName BlendProfileName)
{
#if WITH_EDITOR
	UPoseSearchDatabase* Database = LoadObject<UPoseSearchDatabase>(nullptr,
		*FString::Printf(TEXT("%s.%s"), *DatabasePath, *FPackageName::GetShortName(DatabasePath)));
	if (!Database)
	{
		UE_LOG(LogTemp, Error, TEXT("AZ_ChooserUtils: Could not load database: %s"), *DatabasePath);
		return 0;
	}

	// Get animations from the database
	int32 Added = 0;
	const int32 NumAnims = Database->GetNumAnimationAssets();
	for (int32 i = 0; i < NumAnims; i++)
	{
		const FPoseSearchDatabaseAnimationAsset* Entry = Database->GetDatabaseAnimationAsset(i);
		if (Entry)
		{
			UAnimSequence* Anim = Cast<UAnimSequence>(Entry->AnimAsset.Get());
			if (Anim && AddAnimRow(ChooserPath, Anim, StateMachineState, Gait, Stance, bUseMM, BlendTime, BlendProfileName))
			{
				Added++;
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("AZ_ChooserUtils: Added %d anims from %s"), Added, *FPackageName::GetShortName(DatabasePath));
	return Added;
#else
	return 0;
#endif
}

bool UAZ_ChooserUtils::CompileAndSave(const FString& ChooserPath)
{
#if WITH_EDITOR
	UChooserTable* Table = LoadChooser(ChooserPath);
	if (!Table) return false;

	Table->Compile(true);

	// Save the package — and REPORT the save result. Dropping it returned true on a declined/failed
	// save, which combined with open-asset-tab races produced disk/live divergences (audit P3-26).
	TArray<UPackage*> PackagesToSave;
	PackagesToSave.Add(Table->GetOutermost());
	const FEditorFileUtils::EPromptReturnCode SaveResult =
		FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, false, false);

	const bool bSaved = (SaveResult == FEditorFileUtils::PR_Success);
	UE_LOG(LogTemp, Log, TEXT("AZ_ChooserUtils: Compiled (%d rows), save %s"),
		Table->ResultsStructs.Num(), bSaved ? TEXT("OK") : TEXT("FAILED/DECLINED"));
	return bSaved;
#else
	return false;
#endif
}

int32 UAZ_ChooserUtils::GetRowCount(const FString& ChooserPath)
{
#if WITH_EDITOR
	UChooserTable* Table = LoadChooser(ChooserPath);
	return Table ? Table->ResultsStructs.Num() : 0;
#else
	return 0;
#endif
}

bool UAZ_ChooserUtils::ClearRows(const FString& ChooserPath)
{
#if WITH_EDITOR
	UChooserTable* Table = LoadChooser(ChooserPath);
	if (!Table) return false;

	Table->ResultsStructs.Empty();
#if WITH_EDITORONLY_DATA
	Table->DisabledRows.Empty();
#endif

	// Clear column row data too
	for (FInstancedStruct& ColData : Table->ColumnsStructs)
	{
		ColData.GetMutable<FChooserColumnBase>().SetNumRows(0);
	}

	Table->MarkPackageDirty();
	UE_LOG(LogTemp, Log, TEXT("AZ_ChooserUtils: Cleared all rows"));
	return true;
#else
	return false;
#endif
}

// ========================================
// NESTED CHOOSER SUPPORT
// ========================================

#if WITH_EDITOR
static UChooserTable* FindNestedChooser(UChooserTable* Root, const FString& Name)
{
	// Nested choosers are stored in NestedObjects (not NestedChoosers)
	for (const TObjectPtr<UObject>& Obj : Root->NestedObjects)
	{
		if (UChooserTable* Nested = Cast<UChooserTable>(Obj))
		{
			if (Nested->GetName() == Name)
			{
				return Nested;
			}
		}
	}
	return nullptr;
}

static void ConfigureSubTableColumns(UChooserTable* SubTable)
{
	// Sub-tables get an output struct column for ChooserOutputs
	// Bound to context[1] (the Write-only struct parameter on root)
	// This matches GASP's pattern where root context[1] = S_ChooserOutputs
	SubTable->ColumnsStructs.Empty();

	FInstancedStruct ColStruct;
	ColStruct.InitializeAs<FOutputStructColumn>();
	FOutputStructColumn& Col = ColStruct.GetMutable<FOutputStructColumn>();
	Col.InputValue.InitializeAs<FStructContextProperty>();
	FStructContextProperty& Prop = Col.InputValue.GetMutable<FStructContextProperty>();
	Prop.Binding.PropertyBindingChain = {};  // Empty chain — binds to root of context struct
	Prop.Binding.ContextIndex = 1;           // Context[1] = FAZ_ChooserOutputs struct
	Prop.Binding.IsBoundToRoot = true;       // Writing to the struct itself, not a property on it
#if WITH_EDITORONLY_DATA
	Prop.Binding.StructType = FAZ_ChooserOutputs::StaticStruct();
#endif
	Col.DefaultRowValue.InitializeAs<FAZ_ChooserOutputs>();
	SubTable->ColumnsStructs.Add(MoveTemp(ColStruct));
}
#endif

bool UAZ_ChooserUtils::CreateNestedChooser(const FString& RootChooserPath, const FString& SubTableName)
{
#if WITH_EDITOR
	UChooserTable* Root = LoadChooser(RootChooserPath);
	if (!Root) return false;

	// Check if already exists
	if (FindNestedChooser(Root, SubTableName))
	{
		UE_LOG(LogTemp, Warning, TEXT("AZ_ChooserUtils: Nested chooser '%s' already exists"), *SubTableName);
		return true;
	}

	// Create sub-table as subobject of root
	UChooserTable* SubTable = NewObject<UChooserTable>(Root, *SubTableName, RF_Transactional);
	SubTable->RootChooser = Root;
	SubTable->ResultType = Root->ResultType;
	SubTable->OutputObjectType = Root->OutputObjectType;

	// Add output struct column
	ConfigureSubTableColumns(SubTable);

	// Register with root
	Root->AddNestedChooser(SubTable);
	Root->MarkPackageDirty();

	UE_LOG(LogTemp, Log, TEXT("AZ_ChooserUtils: Created nested chooser '%s'"), *SubTableName);
	return true;
#else
	return false;
#endif
}

bool UAZ_ChooserUtils::AddNestedChooserRow(const FString& RootChooserPath, const FString& SubTableName,
	uint8 StateMachineState, uint8 Gait, uint8 Stance)
{
#if WITH_EDITOR
	UChooserTable* Root = LoadChooser(RootChooserPath);
	if (!Root) return false;
	if (Root->ColumnsStructs.Num() < 3)
	{
		UE_LOG(LogTemp, Error, TEXT("AZ_ChooserUtils: Root needs at least 3 columns"));
		return false;
	}

	UChooserTable* SubTable = FindNestedChooser(Root, SubTableName);
	if (!SubTable)
	{
		UE_LOG(LogTemp, Error, TEXT("AZ_ChooserUtils: Nested chooser '%s' not found"), *SubTableName);
		return false;
	}

	// Add result row pointing to nested chooser
	FInstancedStruct& Result = Root->ResultsStructs.AddDefaulted_GetRef();
	Result.InitializeAs<FNestedChooser>();
	Result.GetMutable<FNestedChooser>().Chooser = SubTable;

	// Column 0: StateMachineState
	FEnumColumn& SMCol = Root->ColumnsStructs[0].GetMutable<FEnumColumn>();
	FChooserEnumRowData SMRow;
	SMRow.Value = StateMachineState;
	SMRow.Comparison = (StateMachineState == 255) ? EEnumColumnCellValueComparison::MatchAny : EEnumColumnCellValueComparison::MatchEqual;
	SMCol.RowValues.Add(SMRow);

	// Column 1: Gait
	FEnumColumn& GaitCol = Root->ColumnsStructs[1].GetMutable<FEnumColumn>();
	FChooserEnumRowData GaitRow;
	GaitRow.Value = Gait;
	GaitRow.Comparison = (Gait == 255) ? EEnumColumnCellValueComparison::MatchAny : EEnumColumnCellValueComparison::MatchEqual;
	GaitCol.RowValues.Add(GaitRow);

	// Column 2: Stance
	FEnumColumn& StanceCol = Root->ColumnsStructs[2].GetMutable<FEnumColumn>();
	FChooserEnumRowData StanceRow;
	StanceRow.Value = Stance;
	StanceRow.Comparison = (Stance == 255) ? EEnumColumnCellValueComparison::MatchAny : EEnumColumnCellValueComparison::MatchEqual;
	StanceCol.RowValues.Add(StanceRow);

	// No output column on root — output is on sub-tables only (GASP pattern)

#if WITH_EDITORONLY_DATA
	Root->DisabledRows.Add(false);
#endif

	Root->MarkPackageDirty();
	UE_LOG(LogTemp, Log, TEXT("AZ_ChooserUtils: Added nested row '%s' (SM=%d, Gait=%d, Stance=%d)"),
		*SubTableName, StateMachineState, Gait, Stance);
	return true;
#else
	return false;
#endif
}

int32 UAZ_ChooserUtils::AddDatabaseRowsToNested(const FString& RootChooserPath, const FString& SubTableName,
	const FString& DatabasePath, bool bUseMM, float BlendTime, FName BlendProfileName)
{
#if WITH_EDITOR
	UChooserTable* Root = LoadChooser(RootChooserPath);
	if (!Root) return 0;

	UChooserTable* SubTable = FindNestedChooser(Root, SubTableName);
	if (!SubTable)
	{
		UE_LOG(LogTemp, Error, TEXT("AZ_ChooserUtils: Nested chooser '%s' not found"), *SubTableName);
		return 0;
	}

	UPoseSearchDatabase* Database = LoadObject<UPoseSearchDatabase>(nullptr,
		*FString::Printf(TEXT("%s.%s"), *DatabasePath, *FPackageName::GetShortName(DatabasePath)));
	if (!Database)
	{
		UE_LOG(LogTemp, Error, TEXT("AZ_ChooserUtils: Could not load database: %s"), *DatabasePath);
		return 0;
	}

	int32 Added = 0;
	const int32 NumAnims = Database->GetNumAnimationAssets();
	for (int32 i = 0; i < NumAnims; i++)
	{
		const FPoseSearchDatabaseAnimationAsset* Entry = Database->GetDatabaseAnimationAsset(i);
		if (!Entry) continue;

		UAnimSequence* Anim = Cast<UAnimSequence>(Entry->AnimAsset.Get());
		if (!Anim) continue;

		// Add result to sub-table
		FInstancedStruct& Result = SubTable->ResultsStructs.AddDefaulted_GetRef();
		Result.InitializeAs<FAssetChooser>();
		Result.GetMutable<FAssetChooser>().Asset = Anim;

		// Add output struct data (column 0 on sub-table)
		if (SubTable->ColumnsStructs.Num() > 0)
		{
			FOutputStructColumn& OutCol = SubTable->ColumnsStructs[0].GetMutable<FOutputStructColumn>();
			FInstancedStruct OutputData;
			OutputData.InitializeAs<FAZ_ChooserOutputs>();
			FAZ_ChooserOutputs& Outputs = OutputData.GetMutable<FAZ_ChooserOutputs>();
			Outputs.bUseMM = bUseMM;
			Outputs.BlendTime = BlendTime;
			Outputs.BlendProfile = BlendProfileName;
			OutCol.RowValues.Add(MoveTemp(OutputData));
		}

#if WITH_EDITORONLY_DATA
		SubTable->DisabledRows.Add(false);
#endif
		Added++;
	}

	Root->MarkPackageDirty();
	UE_LOG(LogTemp, Log, TEXT("AZ_ChooserUtils: Added %d anims to nested '%s' from %s"),
		Added, *SubTableName, *FPackageName::GetShortName(DatabasePath));
	return Added;
#else
	return 0;
#endif
}

TArray<FString> UAZ_ChooserUtils::InspectChooserColumns(const FString& ChooserPath)
{
	TArray<FString> Result;
#if WITH_EDITOR
	UChooserTable* Table = LoadChooser(ChooserPath);
	if (!Table) return Result;

	for (int32 i = 0; i < Table->ColumnsStructs.Num(); ++i)
	{
		FInstancedStruct& ColStruct = Table->ColumnsStructs[i];
		const UScriptStruct* Type = ColStruct.GetScriptStruct();
		FString TypeName = Type ? Type->GetName() : TEXT("Unknown");

		// Try to get property name from the column
		FString PropName;
		if (const FEnumColumn* EnumCol = ColStruct.GetPtr<FEnumColumn>())
		{
			if (const FChooserParameterEnumBase* Input = EnumCol->InputValue.GetPtr<FChooserParameterEnumBase>())
			{
				if (const UEnum* Enum = Input->GetEnum())
				{
					PropName = Enum->GetName();
				}
			}
		}
		else if (const FMultiEnumColumn* MultiCol = ColStruct.GetPtr<FMultiEnumColumn>())
		{
			if (const FChooserParameterEnumBase* Input = MultiCol->InputValue.GetPtr<FChooserParameterEnumBase>())
			{
				if (const UEnum* Enum = Input->GetEnum())
				{
					PropName = Enum->GetName();
				}
			}
		}

		Result.Add(FString::Printf(TEXT("[%d] %s (%s)"), i, *TypeName, *PropName));
	}
#endif
	return Result;
}

bool UAZ_ChooserUtils::SetChooserColumnMultiEnum(const FString& ChooserPath, int32 ColumnIndex, bool bUseMultiEnum)
{
#if WITH_EDITOR
	UChooserTable* Table = LoadChooser(ChooserPath);
	if (!Table || !Table->ColumnsStructs.IsValidIndex(ColumnIndex)) return false;

	FInstancedStruct& ColStruct = Table->ColumnsStructs[ColumnIndex];

	if (bUseMultiEnum)
	{
		// Convert from FEnumColumn to FMultiEnumColumn
		const FEnumColumn* OldCol = ColStruct.GetPtr<FEnumColumn>();
		if (!OldCol) return false; // Already multi or not enum

		FMultiEnumColumn NewCol;
		NewCol.InputValue = OldCol->InputValue; // Copy the property binding

		// Convert existing row values (single enum index -> bitmask)
		NewCol.RowValues.SetNum(OldCol->RowValues.Num());
		for (int32 i = 0; i < OldCol->RowValues.Num(); ++i)
		{
			uint8 EnumIdx = OldCol->RowValues[i].Value;
			EEnumColumnCellValueComparison Comp = OldCol->RowValues[i].Comparison;
			if (Comp == EEnumColumnCellValueComparison::MatchAny)
			{
				NewCol.RowValues[i].Value = 0; // 0 = match any in MultiEnum
			}
			else
			{
				NewCol.RowValues[i].Value = (1u << EnumIdx);
			}
		}

		Table->Modify();
		// Remove old column and insert new one at same position (in-place InitializeAs crashes editor widgets)
		Table->ColumnsStructs.RemoveAt(ColumnIndex);
		Table->ColumnsStructs.Insert(FInstancedStruct::Make<FMultiEnumColumn>(NewCol), ColumnIndex);
		Table->MarkPackageDirty();

		UE_LOG(LogTemp, Log, TEXT("AZ_ChooserUtils: Column %d converted to Enum(Or) with %d rows"), ColumnIndex, NewCol.RowValues.Num());
		return true;
	}
	else
	{
		// Convert from FMultiEnumColumn to FEnumColumn (not commonly needed)
		return false;
	}
#else
	return false;
#endif
}

bool UAZ_ChooserUtils::SetChooserRowMultiEnumValues(const FString& ChooserPath, int32 RowIndex, int32 ColumnIndex,
	const TArray<FString>& EnumValues)
{
#if WITH_EDITOR
	UChooserTable* Table = LoadChooser(ChooserPath);
	if (!Table || !Table->ColumnsStructs.IsValidIndex(ColumnIndex)) return false;

	FInstancedStruct& ColStruct = Table->ColumnsStructs[ColumnIndex];
	FMultiEnumColumn* MultiCol = ColStruct.GetMutablePtr<FMultiEnumColumn>();
	if (!MultiCol) return false;

	if (!MultiCol->RowValues.IsValidIndex(RowIndex)) return false;

	// Get the enum to resolve names to indices
	const UEnum* Enum = nullptr;
	if (const FChooserParameterEnumBase* Input = MultiCol->InputValue.GetPtr<FChooserParameterEnumBase>())
	{
		Enum = Input->GetEnum();
	}
	if (!Enum) return false;

	// Build bitmask from enum value names. The bitmask is indexed by the enum INDEX, NOT the raw value
	// (matches SetCellMultiEnumOnSub + the dump decoder) — shifting by raw value writes wrong masks the
	// moment a sparse enum is used (audit P3-26). Any unresolvable name = honest failure, no partial write.
	uint32 Bitmask = 0;
	for (const FString& ValName : EnumValues)
	{
		const int64 EnumVal = Enum->GetValueByNameString(ValName);
		const int32 EnumIdx = (EnumVal != INDEX_NONE) ? Enum->GetIndexByValue(EnumVal) : INDEX_NONE;
		if (EnumIdx == INDEX_NONE)
		{
			UE_LOG(LogTemp, Warning, TEXT("AZ_ChooserUtils: Enum value '%s' not found in %s — row untouched"),
				*ValName, *Enum->GetName());
			return false;
		}
		Bitmask |= (1u << EnumIdx);
	}

	Table->Modify();
	MultiCol->RowValues[RowIndex].Value = Bitmask;
	Table->MarkPackageDirty();

	UE_LOG(LogTemp, Log, TEXT("AZ_ChooserUtils: Row %d Col %d set bitmask=0x%X (%d values)"),
		RowIndex, ColumnIndex, Bitmask, EnumValues.Num());
	return true;
#else
	return false;
#endif
}

// ========================================
// DUMP CHOOSER FULL TREE
// ========================================

#if WITH_EDITOR
static FString PadStr(int32 Indent) { return FString::ChrN(Indent * 2, TCHAR(' ')); }

// UserDefinedStruct properties are mangled as "FieldName_NN_<32-hex-GUID>".
// Strip the trailing "_NN_GUID" suffix to return just "FieldName".
static FString CleanUDStructFieldName(const FString& RawName)
{
	int32 LastUnderscore = INDEX_NONE;
	if (!RawName.FindLastChar(TEXT('_'), LastUnderscore) || LastUnderscore < 0)
	{
		return RawName;
	}

	// Check trailing segment is a 32-char hex GUID
	const FString HexPart = RawName.Mid(LastUnderscore + 1);
	if (HexPart.Len() != 32)
	{
		return RawName;
	}
	for (TCHAR C : HexPart)
	{
		if (!FChar::IsHexDigit(C))
		{
			return RawName;
		}
	}

	// Strip _GUID, then strip _NN
	FString Trimmed = RawName.Left(LastUnderscore);
	int32 PrevUnderscore = INDEX_NONE;
	if (!Trimmed.FindLastChar(TEXT('_'), PrevUnderscore) || PrevUnderscore < 0)
	{
		return RawName;
	}
	const FString DigitPart = Trimmed.Mid(PrevUnderscore + 1);
	if (DigitPart.IsEmpty())
	{
		return RawName;
	}
	for (TCHAR C : DigitPart)
	{
		if (!FChar::IsDigit(C))
		{
			return RawName;
		}
	}
	return Trimmed.Left(PrevUnderscore);
}

static void DumpChooserTableRecursive(UChooserTable* Table, TArray<FString>& Out, int32 Indent)
{
	const FString Pad = PadStr(Indent);
	if (!Table)
	{
		Out.Add(FString::Printf(TEXT("%s(null chooser)"), *Pad));
		return;
	}

	Out.Add(FString::Printf(TEXT("%s{"), *Pad));
	Out.Add(FString::Printf(TEXT("%s  \"name\": \"%s\","), *Pad, *Table->GetName()));
	Out.Add(FString::Printf(TEXT("%s  \"result_type\": %d,"), *Pad, static_cast<int32>(Table->ResultType)));
	const FString OutTypeName = Table->OutputObjectType ? Table->OutputObjectType->GetName() : TEXT("None");
	Out.Add(FString::Printf(TEXT("%s  \"output_type\": \"%s\","), *Pad, *OutTypeName));
	Out.Add(FString::Printf(TEXT("%s  \"context_count\": %d,"), *Pad, Table->ContextData.Num()));

	// --- Columns ---
	Out.Add(FString::Printf(TEXT("%s  \"columns\": ["), *Pad));
	for (int32 i = 0; i < Table->ColumnsStructs.Num(); ++i)
	{
		const FInstancedStruct& ColS = Table->ColumnsStructs[i];
		const UScriptStruct* Type = ColS.GetScriptStruct();
		const FString TypeName = Type ? Type->GetName() : TEXT("Unknown");
		FString EnumName = TEXT("");
		if (const FEnumColumn* EC = ColS.GetPtr<FEnumColumn>())
		{
			if (const FChooserParameterEnumBase* Input = EC->InputValue.GetPtr<FChooserParameterEnumBase>())
				if (const UEnum* Enum = Input->GetEnum()) EnumName = Enum->GetName();
		}
		else if (const FMultiEnumColumn* MC = ColS.GetPtr<FMultiEnumColumn>())
		{
			if (const FChooserParameterEnumBase* Input = MC->InputValue.GetPtr<FChooserParameterEnumBase>())
				if (const UEnum* Enum = Input->GetEnum()) EnumName = Enum->GetName();
		}
		Out.Add(FString::Printf(TEXT("%s    { \"index\": %d, \"type\": \"%s\", \"enum\": \"%s\" },"),
			*Pad, i, *TypeName, *EnumName));
	}
	Out.Add(FString::Printf(TEXT("%s  ],"), *Pad));

	// --- Rows ---
	const int32 NumRows = Table->ResultsStructs.Num();
	Out.Add(FString::Printf(TEXT("%s  \"rows\": [   // %d rows"), *Pad, NumRows));
	for (int32 r = 0; r < NumRows; ++r)
	{
		FString RowLine = FString::Printf(TEXT("%s    { \"i\": %d"), *Pad, r);

		// Per-column value for this row
		for (int32 c = 0; c < Table->ColumnsStructs.Num(); ++c)
		{
			const FInstancedStruct& ColS = Table->ColumnsStructs[c];

			if (const FEnumColumn* EC = ColS.GetPtr<FEnumColumn>())
			{
				if (EC->RowValues.IsValidIndex(r))
				{
					const FChooserEnumRowData& RD = EC->RowValues[r];
					FString ValStr;
					if (const FChooserParameterEnumBase* Input = EC->InputValue.GetPtr<FChooserParameterEnumBase>())
					{
						if (const UEnum* Enum = Input->GetEnum())
						{
							// Prefer display name (works for UserDefinedEnum with user-set names).
							const int32 Idx = Enum->GetIndexByValue(RD.Value);
							if (Idx != INDEX_NONE)
							{
								ValStr = Enum->GetDisplayNameTextByIndex(Idx).ToString();
								if (ValStr.IsEmpty()) ValStr = Enum->GetNameStringByIndex(Idx);
							}
						}
					}
					if (ValStr.IsEmpty()) ValStr = FString::FromInt(RD.Value);
					const FString Cmp =
						(RD.Comparison == EEnumColumnCellValueComparison::MatchEqual)    ? TEXT("=")  :
						(RD.Comparison == EEnumColumnCellValueComparison::MatchNotEqual) ? TEXT("!=") :
						                                                                   TEXT("Any");
					RowLine += FString::Printf(TEXT(", \"c%d\": \"%s %s\""), c, *Cmp, *ValStr);
				}
			}
			else if (const FMultiEnumColumn* MC = ColS.GetPtr<FMultiEnumColumn>())
			{
				if (MC->RowValues.IsValidIndex(r))
				{
					const uint32 Mask = MC->RowValues[r].Value;
					FString ValStr;
					if (const FChooserParameterEnumBase* Input = MC->InputValue.GetPtr<FChooserParameterEnumBase>())
					{
						if (const UEnum* Enum = Input->GetEnum())
						{
							if (Mask == 0)
							{
								ValStr = TEXT("Any");
							}
							else
							{
								// MultiEnum bitmask is indexed by the enum *index* (0..NumEnums-2),
								// NOT by raw value. Prefer display name over raw name.
								TArray<FString> Parts;
								for (int32 Bit = 0; Bit < Enum->NumEnums() - 1; ++Bit)
								{
									if (Mask & (1u << Bit))
									{
										FString NameStr = Enum->GetDisplayNameTextByIndex(Bit).ToString();
										if (NameStr.IsEmpty()) NameStr = Enum->GetNameStringByIndex(Bit);
										Parts.Add(NameStr);
									}
								}
								ValStr = FString::Join(Parts, TEXT("|"));
							}
						}
					}
					if (ValStr.IsEmpty()) ValStr = FString::Printf(TEXT("0x%X"), Mask);
					RowLine += FString::Printf(TEXT(", \"c%d\": \"%s\""), c, *ValStr);
				}
			}
			else if (const FFloatRangeColumn* FRC = ColS.GetPtr<FFloatRangeColumn>())
			{
				if (FRC->RowValues.IsValidIndex(r))
				{
					const FChooserFloatRangeRowData& RD = FRC->RowValues[r];
					const FString MinStr = RD.bNoMin ? TEXT("-inf") : FString::SanitizeFloat(RD.Min);
					const FString MaxStr = RD.bNoMax ? TEXT("+inf") : FString::SanitizeFloat(RD.Max);
					RowLine += FString::Printf(TEXT(", \"c%d\": \"[%s..%s]\""), c, *MinStr, *MaxStr);
				}
			}
			else if (const FBoolColumn* BC = ColS.GetPtr<FBoolColumn>())
			{
				if (BC->RowValuesWithAny.IsValidIndex(r))
				{
					const EBoolColumnCellValue V = BC->RowValuesWithAny[r];
					const FString ValStr =
						(V == EBoolColumnCellValue::MatchTrue)  ? TEXT("True")  :
						(V == EBoolColumnCellValue::MatchFalse) ? TEXT("False") :
						                                          TEXT("Any");
					RowLine += FString::Printf(TEXT(", \"c%d\": \"%s\""), c, *ValStr);
				}
			}
			else if (const FRandomizeColumn* RC = ColS.GetPtr<FRandomizeColumn>())
			{
				if (RC->RowValues.IsValidIndex(r))
				{
					RowLine += FString::Printf(TEXT(", \"c%d\": \"w=%.3f\""), c, RC->RowValues[r]);
				}
			}
			else if (const FOutputStructColumn* OSC = ColS.GetPtr<FOutputStructColumn>())
			{
				if (OSC->RowValues.IsValidIndex(r))
				{
					const FInstancedStruct& InstS = OSC->RowValues[r];
					const UScriptStruct* ST = InstS.GetScriptStruct();
					FString StructName = ST ? ST->GetName() : TEXT("?");
					FString Fields;
					if (ST)
					{
						// Iterate all properties of the struct and emit key=value.
						for (TFieldIterator<FProperty> It(ST); It; ++It)
						{
							FProperty* Prop = *It;
							FString ValueStr;
							const void* ValPtr = Prop->ContainerPtrToValuePtr<void>(InstS.GetMemory());
							Prop->ExportTextItem_Direct(ValueStr, ValPtr, nullptr, nullptr, PPF_None);
							if (!Fields.IsEmpty()) Fields += TEXT(", ");
							const FString CleanName = CleanUDStructFieldName(Prop->GetName());
							Fields += FString::Printf(TEXT("%s=%s"), *CleanName, *ValueStr);
						}
					}
					RowLine += FString::Printf(TEXT(", \"c%d\": \"%s{%s}\""), c, *StructName, *Fields);
				}
			}
			else
			{
				const UScriptStruct* CT = ColS.GetScriptStruct();
				const FString CTName = CT ? CT->GetName() : TEXT("?");
				RowLine += FString::Printf(TEXT(", \"c%d\": \"(%s)\""), c, *CTName);
			}
		}

		// Row output
		const FInstancedStruct& ResS = Table->ResultsStructs[r];
		if (const FAssetChooser* AC = ResS.GetPtr<FAssetChooser>())
		{
			const FString AssetName = AC->Asset ? AC->Asset->GetName() : TEXT("null");
			const FString AssetType = AC->Asset ? AC->Asset->GetClass()->GetName() : TEXT("null");
			RowLine += FString::Printf(TEXT(", \"out\": \"Asset[%s]:%s\""), *AssetType, *AssetName);
		}
		else if (const FNestedChooser* NC = ResS.GetPtr<FNestedChooser>())
		{
			const FString SubName = NC->Chooser ? NC->Chooser->GetName() : TEXT("null");
			RowLine += FString::Printf(TEXT(", \"out\": \"Nested:%s\""), *SubName);
		}
		else
		{
			const UScriptStruct* RT = ResS.GetScriptStruct();
			const FString RTName = RT ? RT->GetName() : TEXT("Empty");
			RowLine += FString::Printf(TEXT(", \"out\": \"(%s)\""), *RTName);
		}

		RowLine += TEXT(" },");
		Out.Add(RowLine);
	}
	Out.Add(FString::Printf(TEXT("%s  ],"), *Pad));

	// --- Nested sub-choosers ---
	TArray<UChooserTable*> Nested;
	for (const TObjectPtr<UObject>& Obj : Table->NestedObjects)
	{
		if (UChooserTable* Sub = Cast<UChooserTable>(Obj))
		{
			Nested.Add(Sub);
		}
	}
	Out.Add(FString::Printf(TEXT("%s  \"nested\": [   // %d nested"), *Pad, Nested.Num()));
	for (UChooserTable* Sub : Nested)
	{
		DumpChooserTableRecursive(Sub, Out, Indent + 2);
	}
	Out.Add(FString::Printf(TEXT("%s  ]"), *Pad));
	Out.Add(FString::Printf(TEXT("%s}"), *Pad));
}
#endif

TArray<FString> UAZ_ChooserUtils::DumpChooserFullTree(const FString& ChooserPath)
{
	TArray<FString> Result;
#if WITH_EDITOR
	UChooserTable* Table = LoadChooser(ChooserPath);
	if (!Table)
	{
		Result.Add(FString::Printf(TEXT("ERROR: Chooser not found at %s"), *ChooserPath));
		return Result;
	}
	DumpChooserTableRecursive(Table, Result, 0);
#endif
	return Result;
}

// ========================================
// GASP MIGRATION HELPERS
// ========================================

#if WITH_EDITOR
// Collect all tables: root + every nested sub-chooser (recursive).
static void CollectAllTables(UChooserTable* Root, TArray<UChooserTable*>& OutTables)
{
	if (!Root) return;
	OutTables.Add(Root);
	for (const TObjectPtr<UObject>& Obj : Root->NestedObjects)
	{
		if (UChooserTable* Sub = Cast<UChooserTable>(Obj))
		{
			CollectAllTables(Sub, OutTables);
		}
	}
}
#endif

int32 UAZ_ChooserUtils::RebindChooserEnums(const FString& ChooserPath,
	const TArray<FString>& FromEnumPaths,
	const TArray<FString>& ToEnumTypeNames)
{
#if WITH_EDITOR
	if (FromEnumPaths.Num() != ToEnumTypeNames.Num())
	{
		UE_LOG(LogTemp, Error, TEXT("RebindChooserEnums: array length mismatch (%d vs %d)"),
			FromEnumPaths.Num(), ToEnumTypeNames.Num());
		return 0;
	}
	UChooserTable* Root = LoadChooser(ChooserPath);
	if (!Root) return 0;

	// Build lookup: GASP UserDefinedEnum short name -> target UEnum*
	TMap<FString, UEnum*> NameToEnum;
	for (int32 i = 0; i < FromEnumPaths.Num(); ++i)
	{
		const FString ShortName = FPackageName::GetShortName(FromEnumPaths[i]);
		UEnum* TargetEnum = FindFirstObject<UEnum>(*ToEnumTypeNames[i], EFindFirstObjectOptions::EnsureIfAmbiguous);
		if (!TargetEnum)
		{
			UE_LOG(LogTemp, Warning, TEXT("  target enum not found: %s"), *ToEnumTypeNames[i]);
			continue;
		}
		NameToEnum.Add(ShortName, TargetEnum);
	}

	TArray<UChooserTable*> AllTables;
	CollectAllTables(Root, AllTables);

	int32 TouchedCount = 0;
	for (UChooserTable* Table : AllTables)
	{
		Table->Modify();
		for (FInstancedStruct& ColS : Table->ColumnsStructs)
		{
			// EnumColumn
			if (FEnumColumn* EC = ColS.GetMutablePtr<FEnumColumn>())
			{
				if (FChooserParameterEnumBase* Input = EC->InputValue.GetMutablePtr<FChooserParameterEnumBase>())
				{
					if (FEnumContextProperty* Prop = static_cast<FEnumContextProperty*>(Input))
					{
						const UEnum* OldEnum = Prop->Binding.Enum;
						if (OldEnum)
						{
							if (UEnum** NewEnum = NameToEnum.Find(OldEnum->GetName()))
							{
								Prop->Binding.Enum = *NewEnum;
								++TouchedCount;
							}
						}
					}
				}
			}
			// MultiEnumColumn
			else if (FMultiEnumColumn* MC = ColS.GetMutablePtr<FMultiEnumColumn>())
			{
				if (FChooserParameterEnumBase* Input = MC->InputValue.GetMutablePtr<FChooserParameterEnumBase>())
				{
					if (FEnumContextProperty* Prop = static_cast<FEnumContextProperty*>(Input))
					{
						const UEnum* OldEnum = Prop->Binding.Enum;
						if (OldEnum)
						{
							if (UEnum** NewEnum = NameToEnum.Find(OldEnum->GetName()))
							{
								Prop->Binding.Enum = *NewEnum;
								++TouchedCount;
							}
						}
					}
				}
			}
		}
		Table->MarkPackageDirty();
	}

	Root->Compile(true);
	UE_LOG(LogTemp, Log, TEXT("RebindChooserEnums: touched %d columns across %d tables"),
		TouchedCount, AllTables.Num());
	return TouchedCount;
#else
	return 0;
#endif
}

bool UAZ_ChooserUtils::RebindChooserContext(const FString& ChooserPath, UClass* AnimInstanceClass)
{
#if WITH_EDITOR
	UChooserTable* Root = LoadChooser(ChooserPath);
	if (!Root || !AnimInstanceClass) return false;

	if (Root->ContextData.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("RebindChooserContext: chooser has no context data"));
		return false;
	}

	FInstancedStruct& Ctx0 = Root->ContextData[0];
	if (!Ctx0.IsValid() || Ctx0.GetScriptStruct() != FContextObjectTypeClass::StaticStruct())
	{
		UE_LOG(LogTemp, Warning, TEXT("RebindChooserContext: context[0] is not FContextObjectTypeClass"));
		return false;
	}

	FContextObjectTypeClass& Ctx = Ctx0.GetMutable<FContextObjectTypeClass>();
	Ctx.Class = AnimInstanceClass;
	Ctx.Direction = EContextObjectDirection::ReadWrite;

	Root->Modify();
	Root->MarkPackageDirty();
	Root->Compile(true);
	UE_LOG(LogTemp, Log, TEXT("RebindChooserContext: context[0] set to %s"), *AnimInstanceClass->GetName());
	return true;
#else
	return false;
#endif
}

int32 UAZ_ChooserUtils::RebindChooserPropertyNames(const FString& ChooserPath,
	const TArray<FString>& FromPropertyNames,
	const TArray<FString>& ToPropertyNames)
{
#if WITH_EDITOR
	if (FromPropertyNames.Num() != ToPropertyNames.Num()) return 0;
	UChooserTable* Root = LoadChooser(ChooserPath);
	if (!Root) return 0;

	// Build lookup
	TMap<FName, FName> NameMap;
	for (int32 i = 0; i < FromPropertyNames.Num(); ++i)
	{
		NameMap.Add(FName(*FromPropertyNames[i]), FName(*ToPropertyNames[i]));
	}

	TArray<UChooserTable*> AllTables;
	CollectAllTables(Root, AllTables);

	int32 ReplacedCount = 0;
	for (UChooserTable* Table : AllTables)
	{
		Table->Modify();
		for (FInstancedStruct& ColS : Table->ColumnsStructs)
		{
			// Helper lambda to rebind a PropertyBindingChain
			auto TryRebind = [&](FChooserPropertyBinding& Binding) -> bool
			{
				if (Binding.PropertyBindingChain.Num() > 0)
				{
					if (const FName* NewName = NameMap.Find(Binding.PropertyBindingChain[0]))
					{
						Binding.PropertyBindingChain[0] = *NewName;
						++ReplacedCount;
						return true;
					}
				}
				return false;
			};

			if (FEnumColumn* EC = ColS.GetMutablePtr<FEnumColumn>())
			{
				if (FEnumContextProperty* Prop = EC->InputValue.GetMutablePtr<FEnumContextProperty>())
					TryRebind(Prop->Binding);
			}
			else if (FMultiEnumColumn* MC = ColS.GetMutablePtr<FMultiEnumColumn>())
			{
				if (FEnumContextProperty* Prop = MC->InputValue.GetMutablePtr<FEnumContextProperty>())
					TryRebind(Prop->Binding);
			}
			else if (FFloatRangeColumn* FRC = ColS.GetMutablePtr<FFloatRangeColumn>())
			{
				if (FFloatContextProperty* Prop = FRC->InputValue.GetMutablePtr<FFloatContextProperty>())
					TryRebind(Prop->Binding);
			}
			else if (FBoolColumn* BC = ColS.GetMutablePtr<FBoolColumn>())
			{
				if (FBoolContextProperty* Prop = BC->InputValue.GetMutablePtr<FBoolContextProperty>())
					TryRebind(Prop->Binding);
			}
		}
		Table->MarkPackageDirty();
	}

	Root->Compile(true);
	UE_LOG(LogTemp, Log, TEXT("RebindChooserPropertyNames: replaced %d bindings across %d tables"),
		ReplacedCount, AllTables.Num());
	return ReplacedCount;
#else
	return 0;
#endif
}

int32 UAZ_ChooserUtils::RemapChooserAssets(const FString& ChooserPath,
	const TArray<FString>& FromAssetNames,
	const TArray<FString>& ToAssetPaths)
{
#if WITH_EDITOR
	if (FromAssetNames.Num() != ToAssetPaths.Num())
	{
		UE_LOG(LogTemp, Error, TEXT("RemapChooserAssets: array length mismatch"));
		return 0;
	}
	UChooserTable* Root = LoadChooser(ChooserPath);
	if (!Root) return 0;

	// Build lookup: short name -> loaded UObject
	TMap<FString, UObject*> NameToAsset;
	for (int32 i = 0; i < FromAssetNames.Num(); ++i)
	{
		const FString LoadPath = FString::Printf(TEXT("%s.%s"),
			*ToAssetPaths[i], *FPackageName::GetShortName(ToAssetPaths[i]));
		if (UObject* Loaded = LoadObject<UObject>(nullptr, *LoadPath))
		{
			NameToAsset.Add(FromAssetNames[i], Loaded);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("  could not load target asset: %s"), *ToAssetPaths[i]);
		}
	}

	TArray<UChooserTable*> AllTables;
	CollectAllTables(Root, AllTables);

	int32 ReplacedCount = 0;
	for (UChooserTable* Table : AllTables)
	{
		Table->Modify();
		for (FInstancedStruct& ResS : Table->ResultsStructs)
		{
			if (FAssetChooser* AC = ResS.GetMutablePtr<FAssetChooser>())
			{
				if (AC->Asset)
				{
					const FString ShortName = AC->Asset->GetName();
					if (UObject** NewAsset = NameToAsset.Find(ShortName))
					{
						AC->Asset = *NewAsset;
						++ReplacedCount;
					}
				}
			}
		}
		Table->MarkPackageDirty();
	}

	Root->Compile(true);
	UE_LOG(LogTemp, Log, TEXT("RemapChooserAssets: replaced %d assets across %d tables"),
		ReplacedCount, AllTables.Num());
	return ReplacedCount;
#else
	return 0;
#endif
}

// ========================================
// PROGRAMMATIC SUB-CHOOSER BUILDERS
// ========================================

#if WITH_EDITOR
// Find a table by sub-table name; empty name returns the root.
static UChooserTable* ResolveTable(UChooserTable* Root, const FString& SubTableName)
{
	if (!Root) return nullptr;
	if (SubTableName.IsEmpty()) return Root;
	return FindNestedChooser(Root, SubTableName);
}

// Look up a UENUM by type name (e.g. "EAZ_StateMachineState").
static UEnum* ResolveEnumByTypeName(const FString& TypeName)
{
	return FindFirstObject<UEnum>(*TypeName, EFindFirstObjectOptions::EnsureIfAmbiguous);
}
#endif

bool UAZ_ChooserUtils::CreateEmptyNestedChooser(const FString& RootChooserPath, const FString& SubTableName)
{
#if WITH_EDITOR
	UChooserTable* Root = LoadChooser(RootChooserPath);
	if (!Root) return false;
	if (FindNestedChooser(Root, SubTableName))
	{
		UE_LOG(LogTemp, Warning, TEXT("CreateEmptyNestedChooser: '%s' already exists"), *SubTableName);
		return true;
	}
	UChooserTable* Sub = NewObject<UChooserTable>(Root, *SubTableName, RF_Transactional);
	Sub->RootChooser = Root;
	Sub->ResultType = Root->ResultType;
	Sub->OutputObjectType = Root->OutputObjectType;
	// NO default columns — caller adds them explicitly
	Root->AddNestedChooser(Sub);
	Root->MarkPackageDirty();
	return true;
#else
	return false;
#endif
}

int32 UAZ_ChooserUtils::AddEnumColumnToSub(const FString& RootChooserPath, const FString& SubTableName,
	const FString& PropertyName, const FString& EnumTypeName)
{
#if WITH_EDITOR
	UChooserTable* Root = LoadChooser(RootChooserPath);
	UChooserTable* Table = ResolveTable(Root, SubTableName);
	if (!Table) return -1;
	UEnum* Enum = ResolveEnumByTypeName(EnumTypeName);
	if (!Enum)
	{
		UE_LOG(LogTemp, Error, TEXT("AddEnumColumnToSub: enum '%s' not found"), *EnumTypeName);
		return -1;
	}

	FInstancedStruct ColStruct;
	ColStruct.InitializeAs<FEnumColumn>();
	FEnumColumn& Col = ColStruct.GetMutable<FEnumColumn>();
	Col.InputValue.InitializeAs<FEnumContextProperty>();
	FEnumContextProperty& Prop = Col.InputValue.GetMutable<FEnumContextProperty>();
	Prop.Binding.PropertyBindingChain = { FName(*PropertyName) };
	Prop.Binding.ContextIndex = 0;
#if WITH_EDITORONLY_DATA
	Prop.Binding.Enum = Enum;
#endif
	// Extend existing rows with MATCH-ANY cells (audit P3-26): SetNum's default-constructed cell is
	// Comparison=MatchEqual/Value=0 — adding a column to a populated table silently turned every existing
	// row into "must equal enum value 0". Mirror AppendDefaultCellToAllColumns' Any-equivalents.
	{
		FChooserEnumRowData AnyCell;
		AnyCell.Comparison = EEnumColumnCellValueComparison::MatchAny;
		AnyCell.Value = 0;
		Col.RowValues.Init(AnyCell, Table->ResultsStructs.Num());
	}
	const int32 NewIndex = Table->ColumnsStructs.Add(MoveTemp(ColStruct));
	Root->MarkPackageDirty();
	return NewIndex;
#else
	return -1;
#endif
}

int32 UAZ_ChooserUtils::AddMultiEnumColumnToSub(const FString& RootChooserPath, const FString& SubTableName,
	const FString& PropertyName, const FString& EnumTypeName)
{
#if WITH_EDITOR
	UChooserTable* Root = LoadChooser(RootChooserPath);
	UChooserTable* Table = ResolveTable(Root, SubTableName);
	if (!Table) return -1;
	UEnum* Enum = ResolveEnumByTypeName(EnumTypeName);
	if (!Enum)
	{
		UE_LOG(LogTemp, Error, TEXT("AddMultiEnumColumnToSub: enum '%s' not found"), *EnumTypeName);
		return -1;
	}

	FInstancedStruct ColStruct;
	ColStruct.InitializeAs<FMultiEnumColumn>();
	FMultiEnumColumn& Col = ColStruct.GetMutable<FMultiEnumColumn>();
	Col.InputValue.InitializeAs<FEnumContextProperty>();
	FEnumContextProperty& Prop = Col.InputValue.GetMutable<FEnumContextProperty>();
	Prop.Binding.PropertyBindingChain = { FName(*PropertyName) };
	Prop.Binding.ContextIndex = 0;
#if WITH_EDITORONLY_DATA
	Prop.Binding.Enum = Enum;
#endif
	// Backfill = match-any (Value 0 IS match-any for the multi-enum bitmask; explicit for clarity).
	{
		FChooserMultiEnumRowData AnyCell;
		AnyCell.Value = 0;
		Col.RowValues.Init(AnyCell, Table->ResultsStructs.Num());
	}
	const int32 NewIndex = Table->ColumnsStructs.Add(MoveTemp(ColStruct));
	Root->MarkPackageDirty();
	return NewIndex;
#else
	return -1;
#endif
}

int32 UAZ_ChooserUtils::AddFloatRangeColumnToSub(const FString& RootChooserPath, const FString& SubTableName,
	const FString& PropertyName)
{
#if WITH_EDITOR
	UChooserTable* Root = LoadChooser(RootChooserPath);
	UChooserTable* Table = ResolveTable(Root, SubTableName);
	if (!Table) return -1;

	FInstancedStruct ColStruct;
	ColStruct.InitializeAs<FFloatRangeColumn>();
	FFloatRangeColumn& Col = ColStruct.GetMutable<FFloatRangeColumn>();
	Col.InputValue.InitializeAs<FFloatContextProperty>();
	FFloatContextProperty& Prop = Col.InputValue.GetMutable<FFloatContextProperty>();
	Prop.Binding.PropertyBindingChain = { FName(*PropertyName) };
	Prop.Binding.ContextIndex = 0;
	// Backfill = match-any (audit P3-26): SetNum's default cell is [0..0] with bNoMin/bNoMax=false —
	// "must be exactly 0". Existing rows must not gain a constraint from a column add.
	{
		FChooserFloatRangeRowData AnyCell;
		AnyCell.Min = 0.f; AnyCell.Max = 0.f; AnyCell.bNoMin = true; AnyCell.bNoMax = true;
		Col.RowValues.Init(AnyCell, Table->ResultsStructs.Num());
	}
	const int32 NewIndex = Table->ColumnsStructs.Add(MoveTemp(ColStruct));
	Root->MarkPackageDirty();
	return NewIndex;
#else
	return -1;
#endif
}

int32 UAZ_ChooserUtils::AddBoolColumnToSub(const FString& RootChooserPath, const FString& SubTableName,
	const FString& PropertyName)
{
#if WITH_EDITOR
	UChooserTable* Root = LoadChooser(RootChooserPath);
	UChooserTable* Table = ResolveTable(Root, SubTableName);
	if (!Table) return -1;

	FInstancedStruct ColStruct;
	ColStruct.InitializeAs<FBoolColumn>();
	FBoolColumn& Col = ColStruct.GetMutable<FBoolColumn>();
	Col.InputValue.InitializeAs<FBoolContextProperty>();
	FBoolContextProperty& Prop = Col.InputValue.GetMutable<FBoolContextProperty>();
	// Split a dotted path ("ChooserContext.bLeftFootDown") into a multi-element chain so the column can
	// bind through a context struct member; a bare name stays single-element (backward compatible). A
	// single-element chain does NOT resolve when the field lives under a context member — see
	// SetColumnBindingChain and the bLeftFootDown binding fix.
	{
		TArray<FString> Parts;
		PropertyName.ParseIntoArray(Parts, TEXT("."));
		Prop.Binding.PropertyBindingChain.Reset();
		for (const FString& Part : Parts) { Prop.Binding.PropertyBindingChain.Add(FName(*Part)); }
		if (Prop.Binding.PropertyBindingChain.Num() == 0) { Prop.Binding.PropertyBindingChain.Add(FName(*PropertyName)); }
	}
	Prop.Binding.ContextIndex = 0;
	// Fill existing row count with MatchAny default
	Col.RowValuesWithAny.Init(EBoolColumnCellValue::MatchAny, Table->ResultsStructs.Num());
	const int32 NewIndex = Table->ColumnsStructs.Add(MoveTemp(ColStruct));
	Root->MarkPackageDirty();
	return NewIndex;
#else
	return -1;
#endif
}

bool UAZ_ChooserUtils::SetColumnBindingChain(const FString& ChooserPath, int32 ColumnIndex,
	const TArray<FString>& PropertyChain, int32 ContextIndex)
{
#if WITH_EDITOR
	UChooserTable* Root = LoadChooser(ChooserPath);
	if (!Root || !Root->ColumnsStructs.IsValidIndex(ColumnIndex))
	{
		UE_LOG(LogTemp, Error, TEXT("SetColumnBindingChain: bad chooser or column index %d"), ColumnIndex);
		return false;
	}

	TArray<FName> Chain;
	for (const FString& S : PropertyChain) { Chain.Add(FName(*S)); }

	// Set BOTH the chain and the context index. Setting ContextIndex is essential: leaving it stale lets a
	// same-typed struct context capture the binding (the bLeftFootDown always-_LU bug) — the chain must be
	// resolved against the intended context (0 = the AnimInstance, matching the enum columns).
	auto Apply = [&](FChooserPropertyBinding& B) { B.PropertyBindingChain = Chain; B.ContextIndex = ContextIndex; };

	FInstancedStruct& ColS = Root->ColumnsStructs[ColumnIndex];
	bool bSet = false;
	if (FEnumColumn* EC = ColS.GetMutablePtr<FEnumColumn>())
	{
		if (FEnumContextProperty* P = EC->InputValue.GetMutablePtr<FEnumContextProperty>()) { Apply(P->Binding); bSet = true; }
	}
	else if (FMultiEnumColumn* MC = ColS.GetMutablePtr<FMultiEnumColumn>())
	{
		if (FEnumContextProperty* P = MC->InputValue.GetMutablePtr<FEnumContextProperty>()) { Apply(P->Binding); bSet = true; }
	}
	else if (FFloatRangeColumn* FC = ColS.GetMutablePtr<FFloatRangeColumn>())
	{
		if (FFloatContextProperty* P = FC->InputValue.GetMutablePtr<FFloatContextProperty>()) { Apply(P->Binding); bSet = true; }
	}
	else if (FBoolColumn* BC = ColS.GetMutablePtr<FBoolColumn>())
	{
		if (FBoolContextProperty* P = BC->InputValue.GetMutablePtr<FBoolContextProperty>()) { Apply(P->Binding); bSet = true; }
	}
	else if (FOutputStructColumn* OSC = ColS.GetMutablePtr<FOutputStructColumn>())
	{
		// OUTPUT column: rebinds which context the matched row's struct is WRITTEN to. The output-side
		// analogue of the bLeftFootDown wrong-context bug — created with ContextIndex=1 (stale 2-context
		// assumption) but AZ_ChooserOutputs is context[2] in the 3-context CHT_v2 (0 AnimInstance,
		// 1 AZ_v2_ChooserContext, 2 AZ_ChooserOutputs), so the matched row's struct was written to the wrong
		// context and never reached the EvaluateChooser output → ChooserOut.bUseMM stuck false. Pass empty
		// chain + ContextIndex=2; IsBoundToRoot (set at creation) keeps it writing the whole struct.
		if (FStructContextProperty* P = OSC->InputValue.GetMutablePtr<FStructContextProperty>()) { Apply(P->Binding); bSet = true; }
	}

	if (!bSet)
	{
		UE_LOG(LogTemp, Error, TEXT("SetColumnBindingChain: column %d has no rebindable context-property input"), ColumnIndex);
		return false;
	}

	Root->Modify();
	Root->MarkPackageDirty();
	Root->Compile(true);   // resolve the new binding so it takes effect this session (mirrors RebindChooserContext)
	UE_LOG(LogTemp, Log, TEXT("SetColumnBindingChain: column %d -> ctx %d chain [%s]"),
		ColumnIndex, ContextIndex, *FString::JoinBy(Chain, TEXT("."), [](const FName& N){ return N.ToString(); }));
	return true;
#else
	return false;
#endif
}

int32 UAZ_ChooserUtils::AddRandomizeColumnToSub(const FString& RootChooserPath, const FString& SubTableName)
{
#if WITH_EDITOR
	UChooserTable* Root = LoadChooser(RootChooserPath);
	UChooserTable* Table = ResolveTable(Root, SubTableName);
	if (!Table) return -1;

	FInstancedStruct ColStruct;
	ColStruct.InitializeAs<FRandomizeColumn>();
	FRandomizeColumn& Col = ColStruct.GetMutable<FRandomizeColumn>();
	Col.RowValues.Init(1.0f, Table->ResultsStructs.Num());  // default weight 1.0
	const int32 NewIndex = Table->ColumnsStructs.Add(MoveTemp(ColStruct));
	Root->MarkPackageDirty();
	return NewIndex;
#else
	return -1;
#endif
}

int32 UAZ_ChooserUtils::AddOutputStructColumnToSub(const FString& RootChooserPath, const FString& SubTableName,
	const FString& StructTypeName)
{
#if WITH_EDITOR
	UChooserTable* Root = LoadChooser(RootChooserPath);
	UChooserTable* Table = ResolveTable(Root, SubTableName);
	if (!Table) return -1;
	UScriptStruct* StructType = FindFirstObject<UScriptStruct>(*StructTypeName, EFindFirstObjectOptions::EnsureIfAmbiguous);
	if (!StructType)
	{
		UE_LOG(LogTemp, Error, TEXT("AddOutputStructColumnToSub: struct '%s' not found"), *StructTypeName);
		return -1;
	}

	FInstancedStruct ColStruct;
	ColStruct.InitializeAs<FOutputStructColumn>();
	FOutputStructColumn& Col = ColStruct.GetMutable<FOutputStructColumn>();
	Col.InputValue.InitializeAs<FStructContextProperty>();
	FStructContextProperty& Prop = Col.InputValue.GetMutable<FStructContextProperty>();
	Prop.Binding.PropertyBindingChain = {};
	// RESOLVE the context index by scanning the root's ContextData for the struct-typed entry matching
	// StructType — fully determined, no layout guessing (audit P3-26). The old hard default "1" assumed a
	// 2-context layout; on the v2 3-context choosers ([0] AnimInstance, [1] AZ_v2_ChooserContext,
	// [2] AZ_ChooserOutputs) the output struct silently never reached EvaluateChooser's output (ChooserOut
	// stuck at defaults — cost the cross-clip MM a long debug, 2026-05-31).
	int32 ResolvedContextIndex = INDEX_NONE;
	for (int32 CtxIdx = 0; CtxIdx < Root->ContextData.Num(); ++CtxIdx)
	{
		if (const FContextObjectTypeStruct* StructCtx = Root->ContextData[CtxIdx].GetPtr<FContextObjectTypeStruct>())
		{
			if (StructCtx->Struct == StructType)
			{
				ResolvedContextIndex = CtxIdx;
				break;
			}
		}
	}
	if (ResolvedContextIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Error, TEXT("AddOutputStructColumnToSub: no struct-typed context entry of type '%s' on "
			"the root chooser — the column could never bind. Add the context entry first."), *StructTypeName);
		return -1;
	}
	Prop.Binding.ContextIndex = ResolvedContextIndex;
	Prop.Binding.IsBoundToRoot = true;
#if WITH_EDITORONLY_DATA
	Prop.Binding.StructType = StructType;
	Col.DefaultRowValue.InitializeAs(StructType);
#endif
	// Initialize each existing row with a default instance of the struct
	const int32 NumRows = Table->ResultsStructs.Num();
	Col.RowValues.Reset(NumRows);
	for (int32 i = 0; i < NumRows; ++i)
	{
		FInstancedStruct& V = Col.RowValues.AddDefaulted_GetRef();
		V.InitializeAs(StructType);
	}
	const int32 NewIndex = Table->ColumnsStructs.Add(MoveTemp(ColStruct));
	Root->MarkPackageDirty();
	return NewIndex;
#else
	return -1;
#endif
}

#if WITH_EDITOR
// Extend all existing columns of a table by one cell (default value) so indices
// stay aligned when a new row is added.
static void AppendDefaultCellToAllColumns(UChooserTable* Table)
{
	for (FInstancedStruct& ColStruct : Table->ColumnsStructs)
	{
		if (FEnumColumn* EC = ColStruct.GetMutablePtr<FEnumColumn>())
		{
			FChooserEnumRowData D; D.Comparison = EEnumColumnCellValueComparison::MatchAny; D.Value = 0;
			EC->RowValues.Add(D);
		}
		else if (FMultiEnumColumn* MC = ColStruct.GetMutablePtr<FMultiEnumColumn>())
		{
			FChooserMultiEnumRowData D; D.Value = 0;  // 0 = match any
			MC->RowValues.Add(D);
		}
		else if (FFloatRangeColumn* FRC = ColStruct.GetMutablePtr<FFloatRangeColumn>())
		{
			FChooserFloatRangeRowData D;
			D.Min = 0.f; D.Max = 0.f; D.bNoMin = true; D.bNoMax = true;  // match any
			FRC->RowValues.Add(D);
		}
		else if (FBoolColumn* BC = ColStruct.GetMutablePtr<FBoolColumn>())
		{
			BC->RowValuesWithAny.Add(EBoolColumnCellValue::MatchAny);
		}
		else if (FRandomizeColumn* RC = ColStruct.GetMutablePtr<FRandomizeColumn>())
		{
			RC->RowValues.Add(1.0f);  // default weight
		}
		else if (FOutputStructColumn* OSC = ColStruct.GetMutablePtr<FOutputStructColumn>())
		{
			FInstancedStruct& V = OSC->RowValues.AddDefaulted_GetRef();
			if (OSC->DefaultRowValue.GetScriptStruct())
			{
				V.InitializeAs(OSC->DefaultRowValue.GetScriptStruct());
			}
		}
	}
}
#endif

int32 UAZ_ChooserUtils::AddEmptyRowToSub(const FString& RootChooserPath, const FString& SubTableName,
	UObject* AssetOutput)
{
#if WITH_EDITOR
	UChooserTable* Root = LoadChooser(RootChooserPath);
	UChooserTable* Table = ResolveTable(Root, SubTableName);
	if (!Table) return -1;

	// Add the result (asset chooser)
	FInstancedStruct& Result = Table->ResultsStructs.AddDefaulted_GetRef();
	Result.InitializeAs<FAssetChooser>();
	Result.GetMutable<FAssetChooser>().Asset = AssetOutput;

	// Extend every column by one cell with default values
	AppendDefaultCellToAllColumns(Table);
#if WITH_EDITORONLY_DATA
	Table->DisabledRows.Add(false);
#endif
	Root->MarkPackageDirty();
	return Table->ResultsStructs.Num() - 1;
#else
	return -1;
#endif
}

int32 UAZ_ChooserUtils::AddNestedRowToSub(const FString& RootChooserPath, const FString& SubTableName,
	const FString& TargetNestedName)
{
#if WITH_EDITOR
	UChooserTable* Root = LoadChooser(RootChooserPath);
	UChooserTable* Table = ResolveTable(Root, SubTableName);
	if (!Table) return -1;
	UChooserTable* TargetSub = FindNestedChooser(Root, TargetNestedName);
	if (!TargetSub)
	{
		UE_LOG(LogTemp, Error, TEXT("AddNestedRowToSub: nested '%s' not found"), *TargetNestedName);
		return -1;
	}

	FInstancedStruct& Result = Table->ResultsStructs.AddDefaulted_GetRef();
	Result.InitializeAs<FNestedChooser>();
	Result.GetMutable<FNestedChooser>().Chooser = TargetSub;

	AppendDefaultCellToAllColumns(Table);
#if WITH_EDITORONLY_DATA
	Table->DisabledRows.Add(false);
#endif
	Root->MarkPackageDirty();
	return Table->ResultsStructs.Num() - 1;
#else
	return -1;
#endif
}

bool UAZ_ChooserUtils::SetCellEnumOnSub(const FString& RootChooserPath, const FString& SubTableName,
	int32 RowIndex, int32 ColumnIndex, const FString& EnumValueName, int32 Comparison)
{
#if WITH_EDITOR
	UChooserTable* Root = LoadChooser(RootChooserPath);
	UChooserTable* Table = ResolveTable(Root, SubTableName);
	if (!Table || !Table->ColumnsStructs.IsValidIndex(ColumnIndex)) return false;
	FEnumColumn* Col = Table->ColumnsStructs[ColumnIndex].GetMutablePtr<FEnumColumn>();
	if (!Col || !Col->RowValues.IsValidIndex(RowIndex)) return false;

	const UEnum* Enum = nullptr;
	if (const FChooserParameterEnumBase* Input = Col->InputValue.GetPtr<FChooserParameterEnumBase>())
	{
		Enum = Input->GetEnum();
	}

	FChooserEnumRowData& Cell = Col->RowValues[RowIndex];
	if (Comparison == 2)
	{
		Cell.Comparison = EEnumColumnCellValueComparison::MatchAny;
		Cell.Value = 0;
	}
	else
	{
		// HONEST FAILURE (audit P3-26 — the "silent no-op" trap): validate BEFORE writing anything.
		// The old code wrote Comparison first and returned true even when the enum was unresolved
		// (null on a fresh session until the chooser compiles) or the name didn't resolve — so a
		// failed call could flip a cell from Any to "Equal <stale value>" while reporting success.
		if (!Enum)
		{
			UE_LOG(LogTemp, Warning, TEXT("AZ_ChooserUtils: SetCellEnumOnSub col %d — column enum unresolved "
				"(compile the chooser first, then set enum cells). Cell untouched."), ColumnIndex);
			return false;
		}
		const int64 Val = Enum->GetValueByNameString(EnumValueName);
		if (Val == INDEX_NONE)
		{
			UE_LOG(LogTemp, Warning, TEXT("AZ_ChooserUtils: SetCellEnumOnSub — '%s' is not a value of %s. Cell untouched."),
				*EnumValueName, *Enum->GetName());
			return false;
		}
		Cell.Comparison = (Comparison == 1) ? EEnumColumnCellValueComparison::MatchNotEqual : EEnumColumnCellValueComparison::MatchEqual;
		Cell.Value = static_cast<uint8>(Val);
	}
	Root->MarkPackageDirty();
	return true;
#else
	return false;
#endif
}

bool UAZ_ChooserUtils::SetCellMultiEnumOnSub(const FString& RootChooserPath, const FString& SubTableName,
	int32 RowIndex, int32 ColumnIndex, const TArray<FString>& EnumValueNames)
{
#if WITH_EDITOR
	UChooserTable* Root = LoadChooser(RootChooserPath);
	UChooserTable* Table = ResolveTable(Root, SubTableName);
	if (!Table || !Table->ColumnsStructs.IsValidIndex(ColumnIndex)) return false;
	FMultiEnumColumn* Col = Table->ColumnsStructs[ColumnIndex].GetMutablePtr<FMultiEnumColumn>();
	if (!Col || !Col->RowValues.IsValidIndex(RowIndex)) return false;

	const UEnum* Enum = nullptr;
	if (const FChooserParameterEnumBase* Input = Col->InputValue.GetPtr<FChooserParameterEnumBase>())
	{
		Enum = Input->GetEnum();
	}
	if (!Enum) return false;

	uint32 Bitmask = 0;
	for (const FString& Name : EnumValueNames)
	{
		const int64 Val = Enum->GetValueByNameString(Name);
		if (Val != INDEX_NONE)
		{
			const int32 Idx = Enum->GetIndexByValue(Val);
			if (Idx != INDEX_NONE) Bitmask |= (1u << Idx);
		}
	}
	Col->RowValues[RowIndex].Value = Bitmask;
	Root->MarkPackageDirty();
	return true;
#else
	return false;
#endif
}

bool UAZ_ChooserUtils::SetCellFloatRangeOnSub(const FString& RootChooserPath, const FString& SubTableName,
	int32 RowIndex, int32 ColumnIndex, float Min, float Max, bool bNoMin, bool bNoMax)
{
#if WITH_EDITOR
	UChooserTable* Root = LoadChooser(RootChooserPath);
	UChooserTable* Table = ResolveTable(Root, SubTableName);
	if (!Table || !Table->ColumnsStructs.IsValidIndex(ColumnIndex)) return false;
	FFloatRangeColumn* Col = Table->ColumnsStructs[ColumnIndex].GetMutablePtr<FFloatRangeColumn>();
	if (!Col || !Col->RowValues.IsValidIndex(RowIndex)) return false;
	FChooserFloatRangeRowData& Cell = Col->RowValues[RowIndex];
	Cell.Min = Min;
	Cell.Max = Max;
	Cell.bNoMin = bNoMin;
	Cell.bNoMax = bNoMax;
	Root->MarkPackageDirty();
	return true;
#else
	return false;
#endif
}

bool UAZ_ChooserUtils::SetCellBoolOnSub(const FString& RootChooserPath, const FString& SubTableName,
	int32 RowIndex, int32 ColumnIndex, int32 Value)
{
#if WITH_EDITOR
	UChooserTable* Root = LoadChooser(RootChooserPath);
	UChooserTable* Table = ResolveTable(Root, SubTableName);
	if (!Table || !Table->ColumnsStructs.IsValidIndex(ColumnIndex)) return false;
	FBoolColumn* Col = Table->ColumnsStructs[ColumnIndex].GetMutablePtr<FBoolColumn>();
	if (!Col || !Col->RowValuesWithAny.IsValidIndex(RowIndex)) return false;
	const EBoolColumnCellValue V =
		(Value == 1) ? EBoolColumnCellValue::MatchTrue  :
		(Value == 0) ? EBoolColumnCellValue::MatchFalse :
		               EBoolColumnCellValue::MatchAny;
	Col->RowValuesWithAny[RowIndex] = V;
	Root->MarkPackageDirty();
	return true;
#else
	return false;
#endif
}

bool UAZ_ChooserUtils::SetCellRandomizeOnSub(const FString& RootChooserPath, const FString& SubTableName,
	int32 RowIndex, int32 ColumnIndex, float Weight)
{
#if WITH_EDITOR
	UChooserTable* Root = LoadChooser(RootChooserPath);
	UChooserTable* Table = ResolveTable(Root, SubTableName);
	if (!Table || !Table->ColumnsStructs.IsValidIndex(ColumnIndex)) return false;
	FRandomizeColumn* Col = Table->ColumnsStructs[ColumnIndex].GetMutablePtr<FRandomizeColumn>();
	if (!Col || !Col->RowValues.IsValidIndex(RowIndex)) return false;
	Col->RowValues[RowIndex] = Weight;
	Root->MarkPackageDirty();
	return true;
#else
	return false;
#endif
}

bool UAZ_ChooserUtils::SetCellOutputStructFieldOnSub(const FString& RootChooserPath, const FString& SubTableName,
	int32 RowIndex, int32 ColumnIndex, const FString& FieldName, const FString& FieldValueText)
{
#if WITH_EDITOR
	UChooserTable* Root = LoadChooser(RootChooserPath);
	UChooserTable* Table = ResolveTable(Root, SubTableName);
	if (!Table || !Table->ColumnsStructs.IsValidIndex(ColumnIndex)) return false;
	FOutputStructColumn* Col = Table->ColumnsStructs[ColumnIndex].GetMutablePtr<FOutputStructColumn>();
	if (!Col || !Col->RowValues.IsValidIndex(RowIndex)) return false;

	FInstancedStruct& Cell = Col->RowValues[RowIndex];
	const UScriptStruct* ST = Cell.GetScriptStruct();
	if (!ST) return false;

	// Find property by name (try exact match and also "b" + name for bools).
	FProperty* FoundProp = nullptr;
	for (TFieldIterator<FProperty> It(ST); It; ++It)
	{
		FProperty* P = *It;
		if (P->GetName() == FieldName || P->GetName() == (TEXT("b") + FieldName))
		{
			FoundProp = P;
			break;
		}
	}
	if (!FoundProp)
	{
		UE_LOG(LogTemp, Warning, TEXT("SetCellOutputStructFieldOnSub: field '%s' not on %s"),
			*FieldName, *ST->GetName());
		return false;
	}

	void* ValuePtr = FoundProp->ContainerPtrToValuePtr<void>(Cell.GetMutableMemory());
	const TCHAR* Remaining = FoundProp->ImportText_Direct(*FieldValueText, ValuePtr, nullptr, PPF_None);
	if (!Remaining)
	{
		UE_LOG(LogTemp, Warning, TEXT("SetCellOutputStructFieldOnSub: failed to parse '%s' for %s"),
			*FieldValueText, *FieldName);
		return false;
	}
	Root->MarkPackageDirty();
	return true;
#else
	return false;
#endif
}

bool UAZ_ChooserUtils::SetCellAssetOnSub(const FString& RootChooserPath, const FString& SubTableName,
	int32 RowIndex, UObject* NewAsset)
{
#if WITH_EDITOR
	UChooserTable* Root = LoadChooser(RootChooserPath);
	UChooserTable* Table = ResolveTable(Root, SubTableName);
	if (!Table || !Table->ResultsStructs.IsValidIndex(RowIndex)) return false;

	FInstancedStruct& Result = Table->ResultsStructs[RowIndex];
	FAssetChooser* AC = Result.GetMutablePtr<FAssetChooser>();
	if (!AC)
	{
		UE_LOG(LogTemp, Warning, TEXT("SetCellAssetOnSub: row %d in '%s' is not FAssetChooser"),
			RowIndex, *SubTableName);
		return false;
	}
	AC->Asset = NewAsset;
	Root->MarkPackageDirty();
	return true;
#else
	return false;
#endif
}

// ========================================
// AUTO-REMAP (fuzzy matching)
// ========================================

#if WITH_EDITOR
// Normalize an asset short name into a set of lowercase tokens for Jaccard similarity.
// Strips common prefixes (M_, LM_, RM_, Relaxed_, Neutral_, Pose_, sv_, ret_, Lean_) and
// normalizes synonyms (fwd/forward, bwd/backward, lfoot/l, rfoot/r).
static TSet<FString> NormalizeNameTokens(const FString& InName)
{
	FString Work = InName.ToLower();

	// Strip common prefixes iteratively. rtg_rm_ MUST be here alongside lm_rm_: the two parallel retarget
	// batches (LM_RM_* v1 in /Game/AZ/NoWeapons/RootMotions, RTG_RM_* v2 in /Game/Assets/RM_Movement)
	// duplicate ~100 clip names — stripping only one prefix made the fuzzy matcher able to silently swap
	// a chooser row onto the OTHER batch (audit P3-27).
	static const TArray<FString> Prefixes = {
		TEXT("m_relaxed_"), TEXT("m_neutral_"), TEXT("m_"),
		TEXT("rtg_rm_"), TEXT("rtg_"),
		TEXT("lm_rm_"), TEXT("lm_"), TEXT("rm_"),
		TEXT("sv_"), TEXT("ret_"), TEXT("lean_"),
		TEXT("ret_01_"),
	};
	bool bChanged = true;
	while (bChanged)
	{
		bChanged = false;
		for (const FString& Pfx : Prefixes)
		{
			if (Work.StartsWith(Pfx))
			{
				Work = Work.RightChop(Pfx.Len());
				bChanged = true;
				break;
			}
		}
	}

	// Strip common suffixes
	static const TArray<FString> Suffixes = {
		TEXT("_new"), TEXT("1"), // "something1" is often a variant dup
	};
	for (const FString& Sfx : Suffixes)
	{
		if (Work.EndsWith(Sfx))
		{
			Work = Work.LeftChop(Sfx.Len());
		}
	}

	// Split by '_' and normalize each token via synonym table.
	TArray<FString> Parts;
	Work.ParseIntoArray(Parts, TEXT("_"), true);

	TSet<FString> Tokens;
	for (FString& Tok : Parts)
	{
		// Synonym normalization
		if (Tok == TEXT("forward")) Tok = TEXT("fwd");
		else if (Tok == TEXT("backward")) Tok = TEXT("bwd");
		else if (Tok == TEXT("left")) Tok = TEXT("l");
		else if (Tok == TEXT("right")) Tok = TEXT("r");
		else if (Tok == TEXT("lfoot")) Tok = TEXT("l");
		else if (Tok == TEXT("rfoot")) Tok = TEXT("r");
		else if (Tok == TEXT("pose")) continue;  // ignore filler
		else if (Tok == TEXT("loop")) Tok = TEXT("lp");
		else if (Tok == TEXT("start")) Tok = TEXT("st");
		else if (Tok == TEXT("stop")) Tok = TEXT("sp");

		if (!Tok.IsEmpty())
		{
			Tokens.Add(Tok);
		}
	}
	return Tokens;
}

// Overlap coefficient = |A ∩ B| / min(|A|, |B|).
// This works better than Jaccard for short-vs-long name mismatches because it
// doesn't penalize a small candidate having all its tokens present in a larger
// needle (e.g. "idle" in "stand idle loop" scores 1.0 instead of 0.333).
static float OverlapSimilarity(const TSet<FString>& A, const TSet<FString>& B)
{
	if (A.Num() == 0 || B.Num() == 0) return 0.f;
	int32 Intersection = 0;
	for (const FString& X : A)
	{
		if (B.Contains(X)) ++Intersection;
	}
	const int32 MinSize = FMath::Min(A.Num(), B.Num());
	return MinSize > 0 ? (static_cast<float>(Intersection) / MinSize) : 0.f;
}
#endif

int32 UAZ_ChooserUtils::AutoRemapChooserAssets(const FString& ChooserPath,
	const TArray<FString>& SearchPaths,
	float FuzzyThreshold,
	bool bDryRun,
	TArray<FString>& OutReport)
{
#if WITH_EDITOR
	OutReport.Reset();
	UChooserTable* Root = LoadChooser(ChooserPath);
	if (!Root) return 0;

	// --- Build candidate index from SearchPaths ---
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	TMap<FString, FAssetData> ExactIndex;  // short name -> asset data
	TArray<TPair<FString, TSet<FString>>> FuzzyIndex;  // (short name, normalized tokens)

	for (const FString& Path : SearchPaths)
	{
		TArray<FAssetData> FoundAssets;
		Registry.GetAssetsByPath(*Path, FoundAssets, /*bRecursive=*/true);
		for (const FAssetData& AD : FoundAssets)
		{
			// Only consider AnimSequence / AnimationAsset derivatives (and PoseSearchDatabase).
			UClass* AssetClass = AD.GetClass();
			if (!AssetClass) continue;
			const bool bIsAnim = AssetClass->IsChildOf(UAnimSequenceBase::StaticClass());
			const bool bIsPSD = AssetClass->GetName() == TEXT("PoseSearchDatabase");
			if (!bIsAnim && !bIsPSD) continue;

			const FString ShortName = AD.AssetName.ToString();
			ExactIndex.FindOrAdd(ShortName) = AD;
			FuzzyIndex.Add(TPair<FString, TSet<FString>>(ShortName, NormalizeNameTokens(ShortName)));
		}
	}
	OutReport.Add(FString::Printf(TEXT("# Index: %d exact candidates, %d fuzzy candidates"),
		ExactIndex.Num(), FuzzyIndex.Num()));

	// --- Collect tables ---
	TArray<UChooserTable*> AllTables;
	CollectAllTables(Root, AllTables);

	int32 ExactCount = 0, FuzzyCount = 0, UnmatchedCount = 0, ReplacedCount = 0;

	for (UChooserTable* Table : AllTables)
	{
		if (!bDryRun) Table->Modify();
		for (FInstancedStruct& ResS : Table->ResultsStructs)
		{
			FAssetChooser* AC = ResS.GetMutablePtr<FAssetChooser>();
			if (!AC || !AC->Asset) continue;

			const FString CurName = AC->Asset->GetName();

			// Tier 1: exact
			if (const FAssetData* ExactHit = ExactIndex.Find(CurName))
			{
				if (UObject* Loaded = ExactHit->GetAsset())
				{
					if (Loaded != AC->Asset.Get())
					{
						if (!bDryRun) AC->Asset = Loaded;
						++ReplacedCount;
					}
					++ExactCount;
					OutReport.Add(FString::Printf(TEXT("EXACT    1.000  %s  →  %s"),
						*CurName, *CurName));
					continue;
				}
			}

			// Tier 2: fuzzy (overlap coefficient)
			TSet<FString> NeedleTokens = NormalizeNameTokens(CurName);
			float BestScore = 0.f;
			FString BestName;
			for (const TPair<FString, TSet<FString>>& Candidate : FuzzyIndex)
			{
				const float Score = OverlapSimilarity(NeedleTokens, Candidate.Value);
				if (Score > BestScore)
				{
					BestScore = Score;
					BestName = Candidate.Key;
				}
			}
			if (BestScore >= FuzzyThreshold && !BestName.IsEmpty())
			{
				if (const FAssetData* FuzzyHit = ExactIndex.Find(BestName))
				{
					if (UObject* Loaded = FuzzyHit->GetAsset())
					{
						if (!bDryRun) AC->Asset = Loaded;
						++FuzzyCount;
						++ReplacedCount;
						OutReport.Add(FString::Printf(TEXT("FUZZY    %.3f  %s  →  %s"),
							BestScore, *CurName, *BestName));
						continue;
					}
				}
			}

			// Tier 3: unmatched
			++UnmatchedCount;
			OutReport.Add(FString::Printf(TEXT("UNMATCHED %.3f  %s  (best: %s)"),
				BestScore, *CurName, BestName.IsEmpty() ? TEXT("none") : *BestName));
		}
		if (!bDryRun) Table->MarkPackageDirty();
	}

	OutReport.Add(FString::Printf(TEXT("# Summary: exact=%d fuzzy=%d unmatched=%d replaced=%d%s"),
		ExactCount, FuzzyCount, UnmatchedCount, ReplacedCount, bDryRun ? TEXT(" (DRY RUN)") : TEXT("")));

	if (!bDryRun)
	{
		Root->Compile(true);
	}
	UE_LOG(LogTemp, Log, TEXT("AutoRemapChooserAssets: exact=%d fuzzy=%d unmatched=%d replaced=%d"),
		ExactCount, FuzzyCount, UnmatchedCount, ReplacedCount);
	return ReplacedCount;
#else
	return 0;
#endif
}

TArray<FString> UAZ_ChooserUtils::DecodeEnum(const FString& EnumPath)
{
	TArray<FString> Result;
#if WITH_EDITOR
	const FString LoadPath = FString::Printf(TEXT("%s.%s"), *EnumPath, *FPackageName::GetShortName(EnumPath));
	UEnum* Enum = LoadObject<UEnum>(nullptr, *LoadPath);
	if (!Enum)
	{
		Result.Add(FString::Printf(TEXT("ERROR: Enum not found at %s"), *EnumPath));
		return Result;
	}
	Result.Add(FString::Printf(TEXT("=== %s (%d values) ==="), *Enum->GetName(), Enum->NumEnums() - 1));
	// NumEnums() includes the _MAX sentinel; skip it.
	for (int32 i = 0; i < Enum->NumEnums() - 1; ++i)
	{
		const int64 Val = Enum->GetValueByIndex(i);
		const FString RawName = Enum->GetNameStringByIndex(i);
		const FText DisplayText = Enum->GetDisplayNameTextByIndex(i);
		const FString Display = DisplayText.ToString();
		Result.Add(FString::Printf(TEXT("  [%d] val=%lld  raw='%s'  display='%s'"),
			i, Val, *RawName, *Display));
	}
#endif
	return Result;
}
