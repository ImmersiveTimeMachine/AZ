#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Animation/AZ_LocomotionTypes.h"
#include "AZ_ChooserUtils.generated.h"

class UChooserTable;
class UAnimSequence;

/**
 * Utility functions for programmatic Chooser table creation and population.
 * Exposes Chooser column/row manipulation to Python/Blueprint.
 */
UCLASS()
class AZ_API UAZ_ChooserUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/** Configure a Chooser table for the SM+BlendStack animation system.
	 *  Sets output type to AnimationAsset, context to AnimInstance class,
	 *  and adds columns for StateMachineState, Gait, Stance + output struct.
	 *  @param ChooserPath Asset path to the ChooserTable
	 *  @param AnimInstanceClass The AnimInstance class to bind context to
	 *  @return true if configured successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|Chooser")
	static bool ConfigureAnimChooser(const FString& ChooserPath, UClass* AnimInstanceClass);

	/** Add a row to the animation Chooser table.
	 *  @param ChooserPath Asset path to the ChooserTable
	 *  @param AnimAsset The animation asset for this row
	 *  @param StateMachineState Which SM state this row applies to
	 *  @param Gait Which gait (Walk/Run/Sprint)
	 *  @param Stance Which stance (Standing/Crouching)
	 *  @param bUseMM Whether to use MotionMatch for anim selection
	 *  @param BlendTime Blend time for transitions
	 *  @param BlendProfileName Name of the blend profile to use
	 *  @return true if row was added
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|Chooser")
	static bool AddAnimRow(const FString& ChooserPath, UAnimSequence* AnimAsset,
		EAZ_StateMachineState StateMachineState, EAZ_Gait Gait, EAZ_Stance Stance,
		bool bUseMM, float BlendTime, FName BlendProfileName);

	/** Add all animations from a PoseSearch database as rows to the Chooser.
	 *  @param ChooserPath Asset path to the ChooserTable
	 *  @param DatabasePath Asset path to the PoseSearchDatabase
	 *  @param StateMachineState SM state for all rows
	 *  @param Gait Gait for all rows
	 *  @param Stance Stance for all rows
	 *  @param bUseMM Use MM for all rows
	 *  @param BlendTime Blend time for all rows
	 *  @param BlendProfileName Blend profile for all rows
	 *  @return Number of rows added
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|Chooser")
	static int32 AddDatabaseRows(const FString& ChooserPath, const FString& DatabasePath,
		EAZ_StateMachineState StateMachineState, EAZ_Gait Gait, EAZ_Stance Stance,
		bool bUseMM, float BlendTime, FName BlendProfileName);

	/** Compile and save the Chooser table after adding rows. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Chooser")
	static bool CompileAndSave(const FString& ChooserPath);

	/** Get the number of rows in a Chooser table. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Chooser")
	static int32 GetRowCount(const FString& ChooserPath);

	/** Clear all rows from a Chooser table (keeps columns and config). */
	UFUNCTION(BlueprintCallable, Category = "AZ|Chooser")
	static bool ClearRows(const FString& ChooserPath);

	// ========================================
	// NESTED CHOOSER SUPPORT
	// ========================================

	/** Create a nested sub-chooser inside a root Chooser table.
	 *  The sub-table inherits context from the root and can have its own columns.
	 *  @param RootChooserPath Asset path to the root ChooserTable
	 *  @param SubTableName Name for the sub-table (e.g., "Stand Walks")
	 *  @return true if created successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|Chooser")
	static bool CreateNestedChooser(const FString& RootChooserPath, const FString& SubTableName);

	/** Add a root-level row that points to a nested sub-chooser.
	 *  The row filters by SM state/gait/stance, and if matched, delegates to the sub-table.
	 *  @param RootChooserPath Root Chooser path
	 *  @param SubTableName Name of the nested sub-table
	 *  @param StateMachineState SM state filter (use 255 for MatchAny)
	 *  @param Gait Gait filter (use 255 for MatchAny)
	 *  @param Stance Stance filter (use 255 for MatchAny)
	 *  @return true if row was added
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|Chooser")
	static bool AddNestedChooserRow(const FString& RootChooserPath, const FString& SubTableName,
		uint8 StateMachineState, uint8 Gait, uint8 Stance);

	/** Add animation rows to a nested sub-chooser from a PoseSearch database.
	 *  @param RootChooserPath Root Chooser path (sub-tables live inside it)
	 *  @param SubTableName Name of the nested sub-table
	 *  @param DatabasePath PoseSearch database to pull anims from
	 *  @param bUseMM Whether to use MotionMatch
	 *  @param BlendTime Blend time
	 *  @param BlendProfileName Blend profile name
	 *  @return Number of rows added
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|Chooser")
	static int32 AddDatabaseRowsToNested(const FString& RootChooserPath, const FString& SubTableName,
		const FString& DatabasePath, bool bUseMM, float BlendTime, FName BlendProfileName);

	/** Inspect all columns in a Chooser table.
	 *  @return Array of "ColumnIndex: Type (PropertyName)" strings
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|Chooser")
	static TArray<FString> InspectChooserColumns(const FString& ChooserPath);

	/** Change a Chooser column type between EnumValue and Enum(Or).
	 *  @param ColumnIndex 0-based column index
	 *  @param bUseMultiEnum true = Enum(Or), false = Enum Value
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|Chooser")
	static bool SetChooserColumnMultiEnum(const FString& ChooserPath, int32 ColumnIndex, bool bUseMultiEnum);

	/** Set a row's multi-enum bitmask value for an Enum(Or) column.
	 *  @param RowIndex 0-based row index
	 *  @param ColumnIndex 0-based column index
	 *  @param EnumValues Array of enum value names to set (OR'd together as bitmask)
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|Chooser")
	static bool SetChooserRowMultiEnumValues(const FString& ChooserPath, int32 RowIndex, int32 ColumnIndex,
		const TArray<FString>& EnumValues);

	/** Dump the full tree structure of a Chooser table as text lines.
	 *  Walks all columns, rows, and nested sub-choosers recursively and emits
	 *  a JSON-like representation. Use to mirror a chooser from another project
	 *  or audit its contents programmatically.
	 *  @param ChooserPath Asset path to the ChooserTable
	 *  @return Array of text lines representing the full tree
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|Chooser")
	static TArray<FString> DumpChooserFullTree(const FString& ChooserPath);

	/** Decode a UserDefinedEnum asset — return display-name-friendly entries.
	 *  Each line is "[index] value=N rawName='X' displayName='Y'".
	 *  Works for both regular UEnum and UserDefinedEnum (falls back to raw name).
	 *  @param EnumPath Asset path (e.g. "/Game/Blueprints/Data/E_Stance")
	 *  @return Array of text lines, one per enum entry (excluding _MAX)
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|Chooser")
	static TArray<FString> DecodeEnum(const FString& EnumPath);

	// ========================================
	// GASP MIGRATION HELPERS
	// ========================================

	/** Walks all columns (root + nested recursive) in a chooser and swaps enum
	 *  pointers matching any entry in FromEnumPaths to the corresponding AZ enum
	 *  looked up by ToEnumTypeNames[i] (must be a C++ UENUM type name).
	 *  Requires both arrays be the same length.
	 *  @return number of column input bindings touched
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|Chooser")
	static int32 RebindChooserEnums(const FString& ChooserPath,
		const TArray<FString>& FromEnumPaths,
		const TArray<FString>& ToEnumTypeNames);

	/** Swap the chooser's context[0] to a new AnimInstance class.
	 *  Keeps Direction = ReadWrite. Leaves additional context entries untouched.
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|Chooser")
	static bool RebindChooserContext(const FString& ChooserPath, UClass* AnimInstanceClass);

	/** Find/replace anim asset references in all FAssetChooser rows across the
	 *  root and all nested sub-choosers. Matching is by the GASP short name
	 *  (e.g. "M_Relaxed_Stand_Idle_Loop"); replacement is loaded from the given
	 *  full package path.
	 *  @return number of asset references replaced
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|Chooser")
	static int32 RemapChooserAssets(const FString& ChooserPath,
		const TArray<FString>& FromAssetNames,
		const TArray<FString>& ToAssetPaths);

	// ========================================
	// PROGRAMMATIC SUB-CHOOSER BUILDERS
	// ========================================
	// These let you build a nested sub-chooser one column + row at a time.
	// Typical usage:
	//   CreateEmptyNestedChooser(rootPath, "Stand Idle Loops")
	//   AddMultiEnumColumnToSub(rootPath, "Stand Idle Loops", "StateMachineState", "/Script/AZ.EAZ_StateMachineState")
	//   AddFloatRangeColumnToSub(rootPath, "Stand Idle Loops", "Trj_TurnAngle")
	//   AddOutputStructColumnToSub(rootPath, "Stand Idle Loops", "/Script/AZ.AZ_ChooserOutputs")
	//   AddRandomizeColumnToSub(rootPath, "Stand Idle Loops")
	//   AddEmptyRowToSub(rootPath, "Stand Idle Loops", IdleLoopAnim)  -> returns row index
	//   SetCellMultiEnumOnSub(rootPath, "Stand Idle Loops", 0, 0, ["Idle Loop"])
	//   SetCellFloatRangeOnSub(rootPath, "Stand Idle Loops", 0, 1, -999, 999, true, true)
	//   SetCellRandomizeOnSub(rootPath, "Stand Idle Loops", 0, 3, 1.0)

	/** Create a nested sub-chooser with NO default columns. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Chooser|Build")
	static bool CreateEmptyNestedChooser(const FString& RootChooserPath, const FString& SubTableName);

	/** Add an EnumColumn to a sub-chooser (or root if SubTableName is empty). */
	UFUNCTION(BlueprintCallable, Category = "AZ|Chooser|Build")
	static int32 AddEnumColumnToSub(const FString& RootChooserPath, const FString& SubTableName,
		const FString& PropertyName, const FString& EnumTypeName);

	/** Add a MultiEnumColumn (Enum(Or)) to a sub-chooser. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Chooser|Build")
	static int32 AddMultiEnumColumnToSub(const FString& RootChooserPath, const FString& SubTableName,
		const FString& PropertyName, const FString& EnumTypeName);

	/** Add a FloatRangeColumn to a sub-chooser. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Chooser|Build")
	static int32 AddFloatRangeColumnToSub(const FString& RootChooserPath, const FString& SubTableName,
		const FString& PropertyName);

	/** Add a BoolColumn to a sub-chooser. PropertyName may be a dotted path
	 *  (e.g. "ChooserContext.bLeftFootDown") to bind through a context struct member. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Chooser|Build")
	static int32 AddBoolColumnToSub(const FString& RootChooserPath, const FString& SubTableName,
		const FString& PropertyName);

	/** Set the full property-binding chain AND the context index on an existing column's input
	 *  (Enum/MultiEnum/FloatRange/Bool). Use to bind through a context member — e.g.
	 *  ({"ChooserContext","bLeftFootDown"}, ContextIndex 0) for an AnimInstance-context chooser, matching
	 *  the working enum columns. Setting ContextIndex is REQUIRED: a stray same-typed struct context can
	 *  otherwise capture a short chain (the bLeftFootDown always-_LU bug). ContextIndex defaults to 0
	 *  (the AnimInstance). Recompiles the chooser so the new binding resolves. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Chooser|Build")
	static bool SetColumnBindingChain(const FString& ChooserPath, int32 ColumnIndex,
		const TArray<FString>& PropertyChain, int32 ContextIndex = 0);

	/** Add a RandomizeColumn to a sub-chooser. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Chooser|Build")
	static int32 AddRandomizeColumnToSub(const FString& RootChooserPath, const FString& SubTableName);

	/** Add an OutputStructColumn to a sub-chooser, bound to the given struct type.
	 *  Values per row are set via SetCellOutputStructFieldOnSub. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Chooser|Build")
	static int32 AddOutputStructColumnToSub(const FString& RootChooserPath, const FString& SubTableName,
		const FString& StructTypeName);

	/** Add an empty row to a sub-chooser with an asset output. All cells get
	 *  default values which you then set via the SetCell* functions.
	 *  @return the new row index, or -1 on failure. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Chooser|Build")
	static int32 AddEmptyRowToSub(const FString& RootChooserPath, const FString& SubTableName,
		UObject* AssetOutput);

	/** Add an empty row to a sub-chooser whose result is a nested sub-chooser reference. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Chooser|Build")
	static int32 AddNestedRowToSub(const FString& RootChooserPath, const FString& SubTableName,
		const FString& TargetNestedName);

	/** Set a cell value on an EnumColumn.
	 *  @param Comparison 0=Equal, 1=NotEqual, 2=Any */
	UFUNCTION(BlueprintCallable, Category = "AZ|Chooser|Build")
	static bool SetCellEnumOnSub(const FString& RootChooserPath, const FString& SubTableName,
		int32 RowIndex, int32 ColumnIndex, const FString& EnumValueName, int32 Comparison);

	/** Set a cell value on a MultiEnumColumn (bitmask of enum values to match). */
	UFUNCTION(BlueprintCallable, Category = "AZ|Chooser|Build")
	static bool SetCellMultiEnumOnSub(const FString& RootChooserPath, const FString& SubTableName,
		int32 RowIndex, int32 ColumnIndex, const TArray<FString>& EnumValueNames);

	/** Set a cell value on a FloatRangeColumn. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Chooser|Build")
	static bool SetCellFloatRangeOnSub(const FString& RootChooserPath, const FString& SubTableName,
		int32 RowIndex, int32 ColumnIndex, float Min, float Max, bool bNoMin, bool bNoMax);

	/** Set a cell value on a BoolColumn.
	 *  @param Value 0=False, 1=True, 2=Any */
	UFUNCTION(BlueprintCallable, Category = "AZ|Chooser|Build")
	static bool SetCellBoolOnSub(const FString& RootChooserPath, const FString& SubTableName,
		int32 RowIndex, int32 ColumnIndex, int32 Value);

	/** Set a cell weight on a RandomizeColumn. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Chooser|Build")
	static bool SetCellRandomizeOnSub(const FString& RootChooserPath, const FString& SubTableName,
		int32 RowIndex, int32 ColumnIndex, float Weight);

	/** Set a single field on an OutputStructColumn row cell.
	 *  Field value is parsed via FProperty::ImportText (e.g. "0.5", "FastFeet", "True"). */
	UFUNCTION(BlueprintCallable, Category = "AZ|Chooser|Build")
	static bool SetCellOutputStructFieldOnSub(const FString& RootChooserPath, const FString& SubTableName,
		int32 RowIndex, int32 ColumnIndex, const FString& FieldName, const FString& FieldValueText);

	/** Replace the asset reference on an existing row's FAssetChooser result.
	 *  Used to swap a row's anim asset without touching its column filter values
	 *  (preserves all c0/c1/c2... cell data).
	 *  @return true if asset was replaced; false if row index invalid or result is not FAssetChooser */
	UFUNCTION(BlueprintCallable, Category = "AZ|Chooser|Build")
	static bool SetCellAssetOnSub(const FString& RootChooserPath, const FString& SubTableName,
		int32 RowIndex, UObject* NewAsset);

	/** Rebind property names in ALL column bindings across root + nested sub-choosers.
	 *  Walks every EnumColumn/MultiEnumColumn/FloatRangeColumn/BoolColumn and checks
	 *  if the PropertyBindingChain[0] matches any entry in FromPropertyNames[]; if so,
	 *  replaces with the corresponding ToPropertyNames[] entry.
	 *  @return number of bindings replaced
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|Chooser")
	static int32 RebindChooserPropertyNames(const FString& ChooserPath,
		const TArray<FString>& FromPropertyNames,
		const TArray<FString>& ToPropertyNames);

	/** Auto-remap all chooser asset references using best-match search.
	 *  Walks all anim assets under SearchPaths and for each chooser asset ref:
	 *    1) Tries exact short-name match
	 *    2) Falls back to normalized token Jaccard similarity >= FuzzyThreshold
	 *    3) If still unmatched, leaves the original reference as-is
	 *  @param SearchPaths     folders to scan (e.g. ["/Game/AZ/NoWeapons"])
	 *  @param FuzzyThreshold  minimum Jaccard score to accept fuzzy match (0..1, ~0.5 works well)
	 *  @param bDryRun         if true, report what would be replaced without modifying the chooser
	 *  @param OutReport       one line per chooser asset: "EXACT|FUZZY|UNMATCHED  score  old → new"
	 *  @return number of assets replaced (0 if bDryRun)
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|Chooser")
	static int32 AutoRemapChooserAssets(const FString& ChooserPath,
		const TArray<FString>& SearchPaths,
		float FuzzyThreshold,
		bool bDryRun,
		TArray<FString>& OutReport);
};
