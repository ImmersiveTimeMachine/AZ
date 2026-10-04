// Copyright Artur. AZ project.
#include "NGJsonDiff.h"

#include <cmath>
#include <cstdio>

namespace ng
{
	namespace
	{
		bool IsNumberLike(const JValue& V)
		{
			return V.Type == JValue::EType::Num;
		}

		void Mismatch(JsonDiffReport& Out, const std::string& Path, const std::string& What)
		{
			if (Out.Mismatches == 0)
			{
				Out.FirstMismatch = Path + ": " + What;
			}
			++Out.Mismatches;
		}

		void Number(JsonDiffReport& Out, double Ref, double Got, const std::string& Path)
		{
			++Out.Numbers;
			const bool bBothNan = std::isnan(Ref) && std::isnan(Got);
			const double D = bBothNan ? 0.0 : std::fabs(Ref - Got);
			if (!(D <= Out.MaxDiff))
			{
				Out.MaxDiff = std::isnan(D) ? 1e300 : D;
				Out.MaxAt = Path;
			}
		}
	}

	std::string JsonDiffReport::Summary(double Tol) const
	{
		char Buf[256];
		std::snprintf(Buf, sizeof(Buf), "%s: %zu numbers, max |diff| %.3g, %zu mismatches", Ok(Tol) ? "OK" : "FAIL", Numbers, MaxDiff,
		              Mismatches);
		std::string S = Buf;
		if (!MaxAt.empty() && MaxDiff > 0.0)
		{
			S += " (max at " + MaxAt + ")";
		}
		if (!FirstMismatch.empty())
		{
			S += " (first mismatch " + FirstMismatch + ")";
		}
		return S;
	}

	void JsonCompare(const JValue& Ref, const JValue& Got, JsonDiffReport& Out, bool bPrefixArrays, const std::string& Path)
	{
		if (Ref.IsArray())
		{
			if (!Got.IsArray())
			{
				Mismatch(Out, Path, "array expected");
				return;
			}
			const size_t N = Ref.Size();
			if (Got.Size() != N && !(bPrefixArrays && Got.Size() > N))
			{
				Mismatch(Out, Path, "length " + std::to_string(Got.Size()) + " vs " + std::to_string(N));
			}
			const size_t M = N < Got.Size() ? N : Got.Size();
			for (size_t I = 0; I < M; ++I)
			{
				const std::string P = Path + "[" + std::to_string(I) + "]";
				const bool bRefNum = Ref.Type == JValue::EType::NumArr || IsNumberLike(Ref.Arr[I]);
				const bool bGotNum = Got.Type == JValue::EType::NumArr || IsNumberLike(Got.Arr[I]);
				if (bRefNum && bGotNum)
				{
					Number(Out, Ref.NumAt(I), Got.NumAt(I), P);
				}
				else if (bRefNum != bGotNum)
				{
					Mismatch(Out, P, "number vs other");
				}
				else
				{
					JsonCompare(Ref.Arr[I], Got.Arr[I], Out, bPrefixArrays, P);
				}
			}
			return;
		}
		if (Ref.Type != Got.Type)
		{
			// Python dumps tuples and lists alike; int vs float never differs in JSON numbers; bool vs number is real
			Mismatch(Out, Path, "type");
			return;
		}
		switch (Ref.Type)
		{
		case JValue::EType::Obj:
			for (const auto& KV : Ref.Obj)
			{
				const JValue* G = Got.Find(KV.first);
				if (!G)
				{
					Mismatch(Out, Path + "." + KV.first, "missing");
					continue;
				}
				JsonCompare(KV.second, *G, Out, bPrefixArrays, Path + "." + KV.first);
			}
			break;
		case JValue::EType::Num:
			Number(Out, Ref.N, Got.N, Path);
			break;
		case JValue::EType::Str:
			if (Ref.S != Got.S)
			{
				Mismatch(Out, Path, "'" + Got.S + "' vs '" + Ref.S + "'");
			}
			break;
		case JValue::EType::Bool:
			if (Ref.B != Got.B)
			{
				Mismatch(Out, Path, "bool");
			}
			break;
		default:
			break;
		}
	}
}
