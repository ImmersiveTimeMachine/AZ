// Copyright Artur. AZ project.
#include "NGGeometry.h"

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <cmath>
#include <unordered_set>
#include <utility>

namespace ng
{
	namespace
	{
		constexpr double kFourPi = 4.0 * kPi;

		long long FloorL(double V)
		{
			return static_cast<long long>(std::floor(V));
		}
	}

	double PyRound3(double V)
	{
		if (!std::isfinite(V))
		{
			return V;
		}
		char Buf[128];
		const auto R = std::to_chars(Buf, Buf + sizeof(Buf), V, std::chars_format::fixed, 3);
		double Out = 0.0;
		std::from_chars(Buf, R.ptr, Out);
		return Out;
	}

	V3 ClosestOnTriangle(const V3& P, const V3& A, const V3& B, const V3& C)
	{
		const V3 Ab = Sub(B, A), Ac = Sub(C, A), Ap = Sub(P, A);
		const double D1 = Dot(Ab, Ap), D2 = Dot(Ac, Ap);
		if (D1 <= 0 && D2 <= 0)
		{
			return A;
		}
		const V3 Bp = Sub(P, B);
		const double D3 = Dot(Ab, Bp), D4 = Dot(Ac, Bp);
		if (D3 >= 0 && D4 <= D3)
		{
			return B;
		}
		const double Vc = D1 * D4 - D3 * D2;
		if (Vc <= 0 && D1 >= 0 && D3 <= 0)
		{
			const double Vv = D1 / (D1 - D3);
			return V3(A.x + Vv * Ab.x, A.y + Vv * Ab.y, A.z + Vv * Ab.z);
		}
		const V3 Cp = Sub(P, C);
		const double D5 = Dot(Ab, Cp), D6 = Dot(Ac, Cp);
		if (D6 >= 0 && D5 <= D6)
		{
			return C;
		}
		const double Vb = D5 * D2 - D1 * D6;
		if (Vb <= 0 && D2 >= 0 && D6 <= 0)
		{
			const double W = D2 / (D2 - D6);
			return V3(A.x + W * Ac.x, A.y + W * Ac.y, A.z + W * Ac.z);
		}
		const double Va = D3 * D6 - D5 * D4;
		if (Va <= 0 && (D4 - D3) >= 0 && (D5 - D6) >= 0)
		{
			const double W = (D4 - D3) / ((D4 - D3) + (D5 - D6));
			return V3(B.x + W * (C.x - B.x), B.y + W * (C.y - B.y), B.z + W * (C.z - B.z));
		}
		const double Denom = 1.0 / (Va + Vb + Vc);
		const double Vv = Vb * Denom, W = Vc * Denom;
		return V3(A.x + Ab.x * Vv + Ac.x * W, A.y + Ab.y * Vv + Ac.y * W, A.z + Ab.z * Vv + Ac.z * W);
	}

	void SdfPart::Build(const std::string& InName, const std::vector<V3>& Verts, const std::vector<int>& Tris, double InCell)
	{
		Name = InName;
		V = Verts;
		Cell = InCell;
		T.clear();
		Grid.clear();
		for (size_t I = 0; I + 2 < Tris.size(); I += 3)
		{
			T.push_back({V[static_cast<size_t>(Tris[I])], V[static_cast<size_t>(Tris[I + 1])], V[static_cast<size_t>(Tris[I + 2])]});
		}
		for (size_t Ti = 0; Ti < T.size(); ++Ti)
		{
			const std::array<V3, 3>& Tr = T[Ti];
			long long L[3], H[3];
			for (int K = 0; K < 3; ++K)
			{
				const double Mn = std::min(Tr[0][K], std::min(Tr[1][K], Tr[2][K]));
				const double Mx = std::max(Tr[0][K], std::max(Tr[1][K], Tr[2][K]));
				L[K] = FloorL(Mn / Cell);
				H[K] = FloorL(Mx / Cell);
			}
			for (long long I = L[0]; I <= H[0]; ++I)
			{
				for (long long J = L[1]; J <= H[1]; ++J)
				{
					for (long long K = L[2]; K <= H[2]; ++K)
					{
						Grid[{I, J, K}].push_back(static_cast<int>(Ti));
					}
				}
			}
		}
		for (int K = 0; K < 3; ++K)
		{
			double Mn = V[0][K], Mx = V[0][K];
			for (const V3& P : V)
			{
				Mn = std::min(Mn, P[K]);
				Mx = std::max(Mx, P[K]);
			}
			Lo[K] = Mn;
			Hi[K] = Mx;
		}
	}

	double SdfPart::BoxDistance(const V3& P) const
	{
		V3 D;
		for (int K = 0; K < 3; ++K)
		{
			// Python max(lo - p, 0.0, p - hi): the first of equal values is kept
			double M = Lo[K] - P[K];
			if (0.0 > M) M = 0.0;
			if (P[K] - Hi[K] > M) M = P[K] - Hi[K];
			D[K] = M;
		}
		return Length(D);
	}

	double SdfPart::Distance(const V3& P, double MaxDist) const
	{
		if (BoxDistance(P) >= MaxDist)
		{
			return MaxDist;
		}
		const double C = Cell;
		const long long Ci[3] = {FloorL(P.x / C), FloorL(P.y / C), FloorL(P.z / C)};
		double Best = MaxDist;
		std::unordered_set<int> Seen;
		const long long Rings = static_cast<long long>(std::ceil(MaxDist / C));
		for (long long R = 0; R <= Rings; ++R)
		{
			if (R > 0 && static_cast<double>(R - 1) * C > Best)
			{
				break;
			}
			for (long long I = Ci[0] - R; I <= Ci[0] + R; ++I)
			{
				for (long long J = Ci[1] - R; J <= Ci[1] + R; ++J)
				{
					for (long long K = Ci[2] - R; K <= Ci[2] + R; ++K)
					{
						const long long Cheb = std::max(std::llabs(I - Ci[0]), std::max(std::llabs(J - Ci[1]), std::llabs(K - Ci[2])));
						if (Cheb != R)
						{
							continue;
						}
						const auto It = Grid.find({I, J, K});
						if (It == Grid.end())
						{
							continue;
						}
						for (const int Ti : It->second)
						{
							if (!Seen.insert(Ti).second)
							{
								continue;
							}
							const std::array<V3, 3>& Tr = T[static_cast<size_t>(Ti)];
							const V3 Q = ClosestOnTriangle(P, Tr[0], Tr[1], Tr[2]);
							const double D = Length(Sub(P, Q));
							if (D < Best)
							{
								Best = D;
							}
						}
					}
				}
			}
		}
		return Best;
	}

	double SdfPart::Winding(const V3& P) const
	{
		double Total = 0.0;
		for (const std::array<V3, 3>& Tr : T)
		{
			const V3 A = Sub(Tr[0], P), B = Sub(Tr[1], P), C = Sub(Tr[2], P);
			const double La = Length(A), Lb = Length(B), Lc = Length(C);
			if (La < 1e-9 || Lb < 1e-9 || Lc < 1e-9)
			{
				return 0.5;
			}
			const double Num = Dot(A, Cross(B, C));
			const double Den = La * Lb * Lc + Dot(A, B) * Lc + Dot(A, C) * Lb + Dot(B, C) * La;
			Total += 2.0 * std::atan2(Num, Den);
		}
		return Total / kFourPi;
	}

	double SdfWeapon::Sdf(const V3& P, double MaxDist) const
	{
		std::vector<std::pair<double, size_t>> Dists;
		Dists.reserve(Parts.size());
		for (size_t I = 0; I < Parts.size(); ++I)
		{
			Dists.emplace_back(Parts[I].Distance(P, MaxDist), I);
		}
		std::sort(Dists.begin(), Dists.end());
		for (const auto& DI : Dists)
		{
			if (DI.first >= 2.5)
			{
				break;
			}
			if (std::fabs(Parts[DI.second].Winding(P)) > 0.5)
			{
				return -DI.first;
			}
		}
		return Dists.front().first;
	}

	V3 SdfWeapon::Lo() const
	{
		V3 R = Parts.front().Lo;
		for (const SdfPart& Pt : Parts)
		{
			for (int K = 0; K < 3; ++K)
			{
				R[K] = std::min(R[K], Pt.Lo[K]);
			}
		}
		return R;
	}

	V3 SdfWeapon::Hi() const
	{
		V3 R = Parts.front().Hi;
		for (const SdfPart& Pt : Parts)
		{
			for (int K = 0; K < 3; ++K)
			{
				R[K] = std::max(R[K], Pt.Hi[K]);
			}
		}
		return R;
	}

	std::vector<NamedPart> SplitParts(const TriMesh& Mesh)
	{
		// weld identical positions (Python dict keyed by rounded tuples: -0.0 == 0.0, the first key's values are kept)
		std::map<std::array<double, 3>, int> Key;
		std::vector<std::array<double, 3>> Pos;
		std::vector<int> Remap;
		Remap.reserve(Mesh.Verts.size());
		for (const V3& Vt : Mesh.Verts)
		{
			const std::array<double, 3> K = {PyRound3(Vt.x), PyRound3(Vt.y), PyRound3(Vt.z)};
			const auto It = Key.find(K);
			if (It == Key.end())
			{
				const int Id = static_cast<int>(Pos.size());
				Key.emplace(K, Id);
				Pos.push_back(K);
				Remap.push_back(Id);
			}
			else
			{
				Remap.push_back(It->second);
			}
		}
		std::vector<int> Parent(Pos.size());
		for (size_t I = 0; I < Parent.size(); ++I)
		{
			Parent[I] = static_cast<int>(I);
		}
		auto Find = [&Parent](int A)
		{
			while (Parent[static_cast<size_t>(A)] != A)
			{
				Parent[static_cast<size_t>(A)] = Parent[static_cast<size_t>(Parent[static_cast<size_t>(A)])];
				A = Parent[static_cast<size_t>(A)];
			}
			return A;
		};
		const size_t NumTris = Mesh.Tris.size() / 3;
		for (size_t I = 0; I < NumTris; ++I)
		{
			const int A = Remap[static_cast<size_t>(Mesh.Tris[3 * I])], B = Remap[static_cast<size_t>(Mesh.Tris[3 * I + 1])], C = Remap[static_cast<size_t>(Mesh.Tris[3 * I + 2])];
			const int Pairs[2][2] = {{A, B}, {B, C}};
			for (const auto& Pr : Pairs)
			{
				const int Rx = Find(Pr[0]), Ry = Find(Pr[1]);
				if (Rx != Ry)
				{
					Parent[static_cast<size_t>(Rx)] = Ry;
				}
			}
		}
		// components in first-appearance order, then stable-sorted by size (biggest first)
		std::vector<int> Roots;
		std::map<int, size_t> RootIndex;
		std::vector<std::vector<std::array<int, 3>>> Comps;
		for (size_t I = 0; I < NumTris; ++I)
		{
			const int A = Remap[static_cast<size_t>(Mesh.Tris[3 * I])], B = Remap[static_cast<size_t>(Mesh.Tris[3 * I + 1])], C = Remap[static_cast<size_t>(Mesh.Tris[3 * I + 2])];
			const int R = Find(A);
			auto It = RootIndex.find(R);
			if (It == RootIndex.end())
			{
				It = RootIndex.emplace(R, Comps.size()).first;
				Comps.emplace_back();
			}
			Comps[It->second].push_back({A, B, C});
		}
		std::vector<size_t> Order(Comps.size());
		for (size_t I = 0; I < Order.size(); ++I)
		{
			Order[I] = I;
		}
		std::stable_sort(Order.begin(), Order.end(), [&Comps](size_t L, size_t R) { return Comps[L].size() > Comps[R].size(); });
		std::vector<NamedPart> Out;
		for (size_t N = 0; N < Order.size(); ++N)
		{
			NamedPart Part;
			char Buf[16];
			std::snprintf(Buf, sizeof(Buf), "part%02d", static_cast<int>(N));
			Part.Name = Buf;
			std::map<int, int> Idx;
			for (const std::array<int, 3>& Tr : Comps[Order[N]])
			{
				for (const int Vi : Tr)
				{
					auto It = Idx.find(Vi);
					if (It == Idx.end())
					{
						It = Idx.emplace(Vi, static_cast<int>(Part.Verts.size())).first;
						const std::array<double, 3>& P = Pos[static_cast<size_t>(Vi)];
						Part.Verts.emplace_back(P[0], P[1], P[2]);
					}
					Part.Tris.push_back(It->second);
				}
			}
			Out.push_back(std::move(Part));
		}
		return Out;
	}

	Lattice BakeCoarse(const SdfWeapon& Weapon, double Spacing, double Margin, const Exec& Ex)
	{
		Lattice L;
		const V3 Lo = Weapon.Lo(), Hi = Weapon.Hi();
		V3 Origin, Top;
		for (int K = 0; K < 3; ++K)
		{
			Origin[K] = Lo[K] - Margin;
			Top[K] = Hi[K] + Margin;
			L.Dims[K] = static_cast<int>(std::ceil((Top[K] - Origin[K]) / Spacing)) + 1;
		}
		L.Origin = Origin;
		L.Spacing = Spacing;
		const size_t Ny = static_cast<size_t>(L.Dims[1]), Nz = static_cast<size_t>(L.Dims[2]);
		L.D.assign(static_cast<size_t>(L.Dims[0]) * Ny * Nz, 0.0);
		Ex.For(L.Dims[0], [&](int I)
		{
			if (Ex.Cancelled())
			{
				return;
			}
			const double X0 = Origin.x + I * Spacing;
			for (size_t J = 0; J < Ny; ++J)
			{
				for (size_t K = 0; K < Nz; ++K)
				{
					const V3 P(X0, Origin.y + static_cast<double>(J) * Spacing, Origin.z + static_cast<double>(K) * Spacing);
					L.D[(static_cast<size_t>(I) * Ny + J) * Nz + K] = PyRound3(Weapon.Sdf(P, 8.0));
				}
			}
			Ex.Report("coarse field", 0.0);
		});
		return L;
	}

	Lattice BakeFine(const SdfWeapon& Weapon, const Field& CoarseOnly, const V3& Lo, const V3& Hi, double Spacing, double Band, const Exec& Ex)
	{
		Lattice L;
		L.Origin = Lo;
		L.Spacing = Spacing;
		for (int K = 0; K < 3; ++K)
		{
			L.Dims[K] = static_cast<int>(std::ceil((Hi[K] - Lo[K]) / Spacing)) + 1;
		}
		const size_t Ny = static_cast<size_t>(L.Dims[1]), Nz = static_cast<size_t>(L.Dims[2]);
		L.D.assign(static_cast<size_t>(L.Dims[0]) * Ny * Nz, 0.0);
		Ex.For(L.Dims[0], [&](int I)
		{
			if (Ex.Cancelled())
			{
				return;
			}
			const double X0 = Lo.x + I * Spacing;
			for (size_t J = 0; J < Ny; ++J)
			{
				const double Y0 = Lo.y + static_cast<double>(J) * Spacing;
				for (size_t K = 0; K < Nz; ++K)
				{
					const double Z0 = Lo.z + static_cast<double>(K) * Spacing;
					const double C = CoarseOnly.CoarseSample(X0, Y0, Z0);
					L.D[(static_cast<size_t>(I) * Ny + J) * Nz + K] = std::fabs(C) > Band ? PyRound3(C) : PyRound3(Weapon.Sdf(V3(X0, Y0, Z0), 3.0));
				}
			}
			Ex.Report("fine field", 0.0);
		});
		return L;
	}
}
