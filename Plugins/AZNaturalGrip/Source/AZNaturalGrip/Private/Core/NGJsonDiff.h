// Copyright Artur. AZ project.
// Natural Grip solver core: structural comparison of a result JSON with a reference (the Python solver's output).
// Objects: every reference key must exist (extra keys in the result are ignored); arrays: same length (or the reference's
// prefix when bPrefixArrays); numbers: absolute difference; strings / booleans / null: exact.
#pragma once

#include "NGJson.h"

#include <string>

namespace ng
{
	struct JsonDiffReport
	{
		size_t Numbers = 0;          // numbers compared
		double MaxDiff = 0.0;        // largest absolute difference
		std::string MaxAt;           // where it was
		size_t Mismatches = 0;       // structure / string / bool / null / count differences
		std::string FirstMismatch;

		bool Ok(double Tol) const { return Mismatches == 0 && MaxDiff <= Tol; }
		std::string Summary(double Tol) const;
	};

	/** Compares Got against Ref. bPrefixArrays: a longer Got array is compared on Ref's length only (top-N lists). */
	void JsonCompare(const JValue& Ref, const JValue& Got, JsonDiffReport& Out, bool bPrefixArrays = false, const std::string& Path = "$");
}
