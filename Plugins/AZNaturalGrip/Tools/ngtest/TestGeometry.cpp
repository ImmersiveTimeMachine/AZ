// Copyright Artur. AZ project.
// Geometry parity: mesh -> parts, coarse and fine field bakes vs the Python files (Saved/wgs).
//   ngtest geometry split|coarse|fine_left|fine_right|all
#include "NGTest.h"

#include "NGGeometry.h"
#include "NGJson.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>

namespace ngtest
{
	using namespace ng;

	namespace
	{
		const std::string kSaved = "C:/UnrealEngine/Games/AZ/Saved/wgs";

		bool LoadParts(const std::string& Path, SdfWeapon& Out, std::vector<NamedPart>* Raw)
		{
			JValue J;
			std::string Err;
			if (!LoadJsonFile(Path, J, Err))
			{
				std::printf("ERROR %s\n", Err.c_str());
				return false;
			}
			for (const auto& KV : J.Obj)
			{
				NamedPart P;
				P.Name = KV.first;
				const JValue& Vs = KV.second["verts"];
				for (size_t I = 0; I < Vs.Size(); ++I)
				{
					const JValue& V = Vs.At(I);
					P.Verts.emplace_back(V.NumAt(0), V.NumAt(1), V.NumAt(2));
				}
				for (const double T : KV.second["tris"].Numbers())
				{
					P.Tris.push_back(static_cast<int>(T));
				}
				Out.Parts.emplace_back();
				Out.Parts.back().Build(P.Name, P.Verts, P.Tris);
				if (Raw)
				{
					Raw->push_back(std::move(P));
				}
			}
			return true;
		}

		bool CompareLattice(const char* Label, const Lattice& Cpp, const std::string& PyPath, double Tol)
		{
			Lattice Py;
			std::string Err;
			if (!Py.LoadJson(PyPath, Err))
			{
				std::printf("ERROR %s\n", Err.c_str());
				return false;
			}
			Diff D(Label);
			for (int K = 0; K < 3; ++K)
			{
				D.Exact(Py.Dims[K] == Cpp.Dims[K], "dims");
				D.Num(Py.Origin[K], Cpp.Origin[K], "origin");
			}
			D.Num(Py.Spacing, Cpp.Spacing, "spacing");
			size_t Off = 0;
			if (Py.D.size() == Cpp.D.size())
			{
				for (size_t I = 0; I < Py.D.size(); ++I)
				{
					D.Num(Py.D[I], Cpp.D[I], "node " + std::to_string(I));
					Off += Py.D[I] != Cpp.D[I] ? 1 : 0;
				}
			}
			else
			{
				D.Exact(false, "size");
			}
			D.Print(Tol);
			std::printf("    nodes %zu, differing %zu\n", Py.D.size(), Off);
			return D.Ok(Tol);
		}

		double Seconds(std::chrono::steady_clock::time_point T0)
		{
			return std::chrono::duration<double>(std::chrono::steady_clock::now() - T0).count();
		}
	}

	int RunGeometry(int Argc, char** Argv)
	{
		const std::string Mode = Argc > 0 ? Argv[0] : "all";
		const bool bAll = Mode == "all";
		bool bOk = true;
		const Exec Ex = HarnessExec();

		if (bAll || Mode == "split")
		{
			JValue M;
			std::string Err;
			if (!LoadJsonFile(kSaved + "/weapons/m16_mesh.json", M, Err))
			{
				std::printf("ERROR %s\n", Err.c_str());
				return 2;
			}
			TriMesh Mesh;
			const JValue& Vs = M["verts"];
			for (size_t I = 0; I < Vs.Size(); ++I)
			{
				const JValue& V = Vs.At(I);
				Mesh.Verts.emplace_back(V.NumAt(0), V.NumAt(1), V.NumAt(2));
			}
			for (const double T : M["tris"].Numbers())
			{
				Mesh.Tris.push_back(static_cast<int>(T));
			}
			const std::vector<NamedPart> Cpp = SplitParts(Mesh);
			SdfWeapon W;
			std::vector<NamedPart> Py;
			if (!LoadParts(kSaved + "/weapons/m16_parts.json", W, &Py))
			{
				return 2;
			}
			Diff D("mesh -> parts");
			D.Exact(Cpp.size() == Py.size(), "part count");
			for (size_t P = 0; P < Cpp.size() && P < Py.size(); ++P)
			{
				const std::string At = Py[P].Name;
				D.Exact(Cpp[P].Name == Py[P].Name, At + " name");
				D.Exact(Cpp[P].Verts.size() == Py[P].Verts.size() && Cpp[P].Tris == Py[P].Tris, At + " topology");
				for (size_t I = 0; I < Cpp[P].Verts.size() && I < Py[P].Verts.size(); ++I)
				{
					for (int K = 0; K < 3; ++K)
					{
						D.Num(Py[P].Verts[I][K], Cpp[P].Verts[I][K], At);
					}
				}
			}
			D.Print(0.0);
			bOk = bOk && D.Ok(0.0);
		}

		SdfWeapon Weapon;
		if (!LoadParts(kSaved + "/weapons/m16_parts.json", Weapon, nullptr))
		{
			return 2;
		}
		if (bAll || Mode == "coarse")
		{
			const auto T0 = std::chrono::steady_clock::now();
			const Lattice L = BakeCoarse(Weapon, 0.5, 3.0, Ex);
			std::printf("  coarse bake %d x %d x %d in %.1f s\n", L.Dims[0], L.Dims[1], L.Dims[2], Seconds(T0));
			bOk = CompareLattice("coarse field (m16)", L, kSaved + "/fields/m16_field.json", 1e-12) && bOk;
		}
		Field Coarse;
		{
			std::string Err;
			if (!Coarse.Coarse.LoadJson(kSaved + "/fields/m16_field.json", Err))
			{
				std::printf("ERROR %s\n", Err.c_str());
				return 2;
			}
		}
		if (bAll || Mode == "fine_left")
		{
			const auto T0 = std::chrono::steady_clock::now();
			const Lattice L = BakeFine(Weapon, Coarse, V3(-8, 12, 0), V3(10, 34, 20), 0.25, 1.3, Ex);
			std::printf("  fine left bake %d x %d x %d in %.1f s\n", L.Dims[0], L.Dims[1], L.Dims[2], Seconds(T0));
			bOk = CompareLattice("fine field left (m16)", L, kSaved + "/fine_field_left_m16.json", 1e-12) && bOk;
		}
		if (bAll || Mode == "fine_right")
		{
			const auto T0 = std::chrono::steady_clock::now();
			const Lattice L = BakeFine(Weapon, Coarse, V3(-9, -20, -13), V3(8, 10, 14), 0.15, 1.3, Ex);
			std::printf("  fine right bake %d x %d x %d in %.1f s\n", L.Dims[0], L.Dims[1], L.Dims[2], Seconds(T0));
			bOk = CompareLattice("fine field right (m16)", L, kSaved + "/fine_field_right_m16.json", 1e-12) && bOk;
		}
		std::printf("geometry %s: %s\n", Mode.c_str(), bOk ? "PASS" : "FAIL");
		return bOk ? 0 : 1;
	}
}
