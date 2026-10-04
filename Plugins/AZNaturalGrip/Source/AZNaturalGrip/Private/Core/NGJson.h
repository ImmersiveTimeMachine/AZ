// Copyright Artur. AZ project.
// Natural Grip solver core: a minimal JSON reader/writer for the solver's data files (no engine dependency).
// Arrays made only of numbers are stored flat (NumArr) - the field lattices hold millions of values.
#pragma once

#include <string>
#include <utility>
#include <vector>

namespace ng
{
	struct JValue
	{
		enum class EType { Null, Bool, Num, Str, Arr, NumArr, Obj };
		EType Type = EType::Null;
		bool B = false;
		double N = 0.0;
		std::string S;
		std::vector<JValue> Arr;
		std::vector<double> Nums;
		std::vector<std::pair<std::string, JValue>> Obj;   // insertion order kept (Python dicts are ordered)

		bool IsNull() const { return Type == EType::Null; }
		bool IsObj() const { return Type == EType::Obj; }
		bool IsArray() const { return Type == EType::Arr || Type == EType::NumArr; }

		/** Member lookup; nullptr when missing or not an object. */
		const JValue* Find(const std::string& Key) const;
		/** Member lookup; throws std::runtime_error when missing. */
		const JValue& operator[](const std::string& Key) const;
		/** Element count of an array. */
		size_t Size() const { return Type == EType::NumArr ? Nums.size() : Arr.size(); }
		/** Numeric element I of an array (flat or generic). */
		double NumAt(size_t I) const;
		/** Element I of a generic array (throws for a flat numeric array). */
		const JValue& At(size_t I) const;
		/** Number value (bools convert to 0/1). */
		double Number() const;
		/** All elements of a numeric array. */
		std::vector<double> Numbers() const;
	};

	bool ParseJson(const std::string& Text, JValue& Out, std::string& Error);
	bool LoadJsonFile(const std::string& Path, JValue& Out, std::string& Error);

	/** Streaming JSON writer (shortest round-trip doubles, so values read back exactly). */
	class JWriter
	{
	public:
		explicit JWriter(bool bPretty = true) : bPretty_(bPretty) {}
		JWriter& BeginObject();
		JWriter& EndObject();
		JWriter& BeginArray();
		JWriter& EndArray();
		JWriter& Key(const std::string& K);
		JWriter& Value(double V);
		JWriter& Value(int V);
		JWriter& Value(bool V);
		JWriter& Value(const std::string& V);
		JWriter& Value(const char* V) { return Value(std::string(V)); }
		JWriter& Null();
		JWriter& Numbers(const double* V, size_t Count);
		const std::string& Str() const { return Out_; }
		bool Save(const std::string& Path) const;

	private:
		void Separator();
		void WriteString(const std::string& V);
		std::string Out_;
		std::vector<bool> First_;
		bool bAfterKey_ = false;
		bool bPretty_ = true;
	};

	std::string FormatDouble(double V);
}
