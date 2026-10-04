// Copyright Artur. AZ project.
#include "NGJson.h"

#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace ng
{
	const JValue* JValue::Find(const std::string& Key) const
	{
		if (Type != EType::Obj)
		{
			return nullptr;
		}
		for (const auto& KV : Obj)
		{
			if (KV.first == Key)
			{
				return &KV.second;
			}
		}
		return nullptr;
	}

	const JValue& JValue::operator[](const std::string& Key) const
	{
		const JValue* V = Find(Key);
		if (!V)
		{
			throw std::runtime_error("json: missing key '" + Key + "'");
		}
		return *V;
	}

	double JValue::NumAt(size_t I) const
	{
		if (Type == EType::NumArr)
		{
			return Nums.at(I);
		}
		return Arr.at(I).Number();
	}

	const JValue& JValue::At(size_t I) const
	{
		if (Type != EType::Arr)
		{
			throw std::runtime_error("json: not a generic array");
		}
		return Arr.at(I);
	}

	double JValue::Number() const
	{
		if (Type == EType::Num)
		{
			return N;
		}
		if (Type == EType::Bool)
		{
			return B ? 1.0 : 0.0;
		}
		throw std::runtime_error("json: not a number");
	}

	std::vector<double> JValue::Numbers() const
	{
		if (Type == EType::NumArr)
		{
			return Nums;
		}
		std::vector<double> R;
		R.reserve(Arr.size());
		for (const JValue& V : Arr)
		{
			R.push_back(V.Number());
		}
		return R;
	}

	namespace
	{
		struct FParser
		{
			const char* P;
			const char* E;
			std::string Err;

			void Ws()
			{
				while (P < E && (*P == ' ' || *P == '\n' || *P == '\r' || *P == '\t'))
				{
					++P;
				}
			}

			bool Fail(const char* Msg)
			{
				if (Err.empty())
				{
					Err = Msg;
				}
				return false;
			}

			bool Lit(const char* L)
			{
				const char* Q = P;
				for (; *L; ++L, ++Q)
				{
					if (Q >= E || *Q != *L)
					{
						return false;
					}
				}
				P = Q;
				return true;
			}

			bool Number(double& Out)
			{
				if (Lit("NaN"))
				{
					Out = std::numeric_limits<double>::quiet_NaN();
					return true;
				}
				if (Lit("Infinity"))
				{
					Out = std::numeric_limits<double>::infinity();
					return true;
				}
				if (Lit("-Infinity"))
				{
					Out = -std::numeric_limits<double>::infinity();
					return true;
				}
				const auto R = std::from_chars(P, E, Out);
				if (R.ec != std::errc())
				{
					return Fail("bad number");
				}
				P = R.ptr;
				return true;
			}

			bool String(std::string& Out)
			{
				if (P >= E || *P != '"')
				{
					return Fail("expected string");
				}
				++P;
				Out.clear();
				while (P < E && *P != '"')
				{
					if (*P == '\\')
					{
						++P;
						if (P >= E)
						{
							return Fail("bad escape");
						}
						switch (*P)
						{
						case 'n': Out += '\n'; break;
						case 't': Out += '\t'; break;
						case 'r': Out += '\r'; break;
						case 'b': Out += '\b'; break;
						case 'f': Out += '\f'; break;
						case 'u':
						{
							if (E - P < 5)
							{
								return Fail("bad \\u escape");
							}
							const unsigned Code = static_cast<unsigned>(std::strtoul(std::string(P + 1, P + 5).c_str(), nullptr, 16));
							P += 4;
							if (Code < 0x80)
							{
								Out += static_cast<char>(Code);
							}
							else if (Code < 0x800)
							{
								Out += static_cast<char>(0xC0 | (Code >> 6));
								Out += static_cast<char>(0x80 | (Code & 0x3F));
							}
							else
							{
								Out += static_cast<char>(0xE0 | (Code >> 12));
								Out += static_cast<char>(0x80 | ((Code >> 6) & 0x3F));
								Out += static_cast<char>(0x80 | (Code & 0x3F));
							}
							break;
						}
						default: Out += *P; break;
						}
						++P;
					}
					else
					{
						Out += *P++;
					}
				}
				if (P >= E)
				{
					return Fail("unterminated string");
				}
				++P;
				return true;
			}

			static bool StartsNumber(char C)
			{
				return C == '-' || (C >= '0' && C <= '9') || C == 'N' || C == 'I';
			}

			bool Value(JValue& V)
			{
				Ws();
				if (P >= E)
				{
					return Fail("unexpected end");
				}
				const char C = *P;
				if (C == '{')
				{
					++P;
					V.Type = JValue::EType::Obj;
					Ws();
					if (P < E && *P == '}')
					{
						++P;
						return true;
					}
					for (;;)
					{
						Ws();
						std::string K;
						if (!String(K))
						{
							return false;
						}
						Ws();
						if (P >= E || *P != ':')
						{
							return Fail("expected ':'");
						}
						++P;
						V.Obj.emplace_back(std::move(K), JValue());
						if (!Value(V.Obj.back().second))
						{
							return false;
						}
						Ws();
						if (P < E && *P == ',')
						{
							++P;
							continue;
						}
						if (P < E && *P == '}')
						{
							++P;
							return true;
						}
						return Fail("expected ',' or '}'");
					}
				}
				if (C == '[')
				{
					++P;
					Ws();
					if (P < E && *P == ']')
					{
						++P;
						V.Type = JValue::EType::Arr;
						return true;
					}
					// flat numeric array while every element is a number
					V.Type = StartsNumber(*P) ? JValue::EType::NumArr : JValue::EType::Arr;
					for (;;)
					{
						Ws();
						if (V.Type == JValue::EType::NumArr)
						{
							if (P < E && StartsNumber(*P))
							{
								double D = 0.0;
								if (!Number(D))
								{
									return false;
								}
								V.Nums.push_back(D);
							}
							else
							{
								// mixed array: convert what we have to generic values
								V.Type = JValue::EType::Arr;
								for (double D : V.Nums)
								{
									JValue N;
									N.Type = JValue::EType::Num;
									N.N = D;
									V.Arr.push_back(N);
								}
								V.Nums.clear();
								V.Arr.emplace_back();
								if (!Value(V.Arr.back()))
								{
									return false;
								}
							}
						}
						else
						{
							V.Arr.emplace_back();
							if (!Value(V.Arr.back()))
							{
								return false;
							}
						}
						Ws();
						if (P < E && *P == ',')
						{
							++P;
							continue;
						}
						if (P < E && *P == ']')
						{
							++P;
							return true;
						}
						return Fail("expected ',' or ']'");
					}
				}
				if (C == '"')
				{
					V.Type = JValue::EType::Str;
					return String(V.S);
				}
				if (Lit("true"))
				{
					V.Type = JValue::EType::Bool;
					V.B = true;
					return true;
				}
				if (Lit("false"))
				{
					V.Type = JValue::EType::Bool;
					V.B = false;
					return true;
				}
				if (Lit("null"))
				{
					V.Type = JValue::EType::Null;
					return true;
				}
				V.Type = JValue::EType::Num;
				return Number(V.N);
			}
		};
	}

	bool ParseJson(const std::string& Text, JValue& Out, std::string& Error)
	{
		FParser Parser{Text.data(), Text.data() + Text.size(), {}};
		Out = JValue();
		if (!Parser.Value(Out))
		{
			Error = Parser.Err + " at byte " + std::to_string(Parser.P - Text.data());
			return false;
		}
		return true;
	}

	bool LoadJsonFile(const std::string& Path, JValue& Out, std::string& Error)
	{
		std::ifstream F(Path, std::ios::binary);
		if (!F)
		{
			Error = "cannot open " + Path;
			return false;
		}
		std::stringstream SS;
		SS << F.rdbuf();
		if (!ParseJson(SS.str(), Out, Error))
		{
			Error = Path + ": " + Error;
			return false;
		}
		return true;
	}

	std::string FormatDouble(double V)
	{
		if (std::isnan(V))
		{
			return "NaN";
		}
		if (std::isinf(V))
		{
			return V > 0 ? "Infinity" : "-Infinity";
		}
		char Buf[64];
		const auto R = std::to_chars(Buf, Buf + sizeof(Buf), V);   // shortest round-trip form
		return std::string(Buf, R.ptr);
	}

	void JWriter::Separator()
	{
		if (bAfterKey_)
		{
			bAfterKey_ = false;
			return;
		}
		if (!First_.empty())
		{
			if (!First_.back())
			{
				Out_ += ',';
			}
			First_.back() = false;
			if (bPretty_)
			{
				Out_ += '\n';
				Out_.append(First_.size(), ' ');
			}
		}
	}

	JWriter& JWriter::BeginObject()
	{
		Separator();
		Out_ += '{';
		First_.push_back(true);
		return *this;
	}

	JWriter& JWriter::EndObject()
	{
		First_.pop_back();
		Out_ += '}';
		return *this;
	}

	JWriter& JWriter::BeginArray()
	{
		Separator();
		Out_ += '[';
		First_.push_back(true);
		return *this;
	}

	JWriter& JWriter::EndArray()
	{
		First_.pop_back();
		Out_ += ']';
		return *this;
	}

	JWriter& JWriter::Key(const std::string& K)
	{
		Separator();
		WriteString(K);
		Out_ += ": ";
		bAfterKey_ = true;
		return *this;
	}

	JWriter& JWriter::Value(double V)
	{
		Separator();
		Out_ += FormatDouble(V);
		return *this;
	}

	JWriter& JWriter::Value(int V)
	{
		Separator();
		Out_ += std::to_string(V);
		return *this;
	}

	JWriter& JWriter::Value(bool V)
	{
		Separator();
		Out_ += V ? "true" : "false";
		return *this;
	}

	JWriter& JWriter::Value(const std::string& V)
	{
		Separator();
		WriteString(V);
		return *this;
	}

	void JWriter::WriteString(const std::string& V)
	{
		std::string Esc;
		Esc.reserve(V.size() + 2);
		Esc += '"';
		for (char C : V)
		{
			switch (C)
			{
			case '"': Esc += "\\\""; break;
			case '\\': Esc += "\\\\"; break;
			case '\n': Esc += "\\n"; break;
			case '\t': Esc += "\\t"; break;
			case '\r': Esc += "\\r"; break;
			default: Esc += C; break;
			}
		}
		Esc += '"';
		Out_ += Esc;
	}

	JWriter& JWriter::Null()
	{
		Separator();
		Out_ += "null";
		return *this;
	}

	JWriter& JWriter::Numbers(const double* V, size_t Count)
	{
		BeginArray();
		const bool bWasPretty = bPretty_;
		bPretty_ = false;
		for (size_t I = 0; I < Count; ++I)
		{
			Value(V[I]);
		}
		bPretty_ = bWasPretty;
		return EndArray();
	}

	bool JWriter::Save(const std::string& Path) const
	{
		std::ofstream F(Path, std::ios::binary);
		if (!F)
		{
			return false;
		}
		F << Out_;
		return static_cast<bool>(F);
	}
}
