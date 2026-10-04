// Copyright Artur. AZ project.
// Natural Grip solver core: weapon geometry and the signed-distance field bake (no engine dependency).
// Exact port of Tools/wgs/geom.py (per-part exact nearest triangle through a cell grid + generalised winding number
// inside test, union = min), Tools/wgs/skm_parts.py (weld + connected parts), Tools/wgs/bake_grip_field.py (coarse
// lattice) and Tools/wgs/natgrip/fine_field.py (exact narrow band around one hand's grip).
// This is the REFERENCE implementation (parity with the Python fields). The editor can swap in a faster backend
// (GeometryCore BVH + fast winding numbers) behind ISdf and is checked against this one.
#pragma once

#include "NGField.h"
#include "NGMath.h"
#include "NGParallel.h"

#include <array>
#include <map>
#include <string>
#include <vector>

namespace ng
{
	/** Python round(V, 3): correctly rounded to 3 decimals (ties to even on the exact binary value). */
	double PyRound3(double V);

	/** Ericson, Real-Time Collision Detection 5.1.5 (geom.closest_on_triangle). */
	V3 ClosestOnTriangle(const V3& P, const V3& A, const V3& B, const V3& C);

	/** A raw triangle mesh (weapon_skm_dump.py output: vertex positions + index triples). */
	struct TriMesh
	{
		std::vector<V3> Verts;
		std::vector<int> Tris;   // 3 per triangle
	};

	/** One connected part (geom.Part): exact distance through a cell grid, inside test by the winding number. */
	class SdfPart
	{
	public:
		std::string Name;
		std::vector<V3> V;
		std::vector<std::array<V3, 3>> T;
		double Cell = 1.5;
		V3 Lo, Hi;

		void Build(const std::string& InName, const std::vector<V3>& Verts, const std::vector<int>& Tris, double InCell = 1.5);
		double BoxDistance(const V3& P) const;
		/** Unsigned distance, exact within MaxDist; MaxDist when nothing is closer. */
		double Distance(const V3& P, double MaxDist) const;
		/** Generalised winding number (Jacobson 2013): ~1 inside, ~0 outside. */
		double Winding(const V3& P) const;

	private:
		std::map<std::array<long long, 3>, std::vector<int>> Grid;   // cell -> triangle indices (insertion order)
	};

	/** Union of parts (geom.Weapon). */
	class SdfWeapon
	{
	public:
		std::vector<SdfPart> Parts;

		/** Signed distance (cm), < 0 inside, exact where |d| < MaxDist (geom.Weapon.sdf). */
		double Sdf(const V3& P, double MaxDist = 6.0) const;
		V3 Lo() const;
		V3 Hi() const;
	};

	/** skm_parts.py: weld identical positions (rounded to 1e-3), split into connected components, biggest first. */
	struct NamedPart
	{
		std::string Name;
		std::vector<V3> Verts;
		std::vector<int> Tris;
	};
	std::vector<NamedPart> SplitParts(const TriMesh& Mesh);

	/** bake_grip_field.py: the coarse lattice over the weapon's bounds + Margin (values rounded to 1e-3 like Python). */
	Lattice BakeCoarse(const SdfWeapon& Weapon, double Spacing, double Margin, const Exec& Ex);

	/** fine_field.py: the box [Lo, Hi] at Spacing; nodes where the coarse field says |d| > Band keep it, the others get
	 *  the exact distance (values rounded to 1e-3 like Python). */
	Lattice BakeFine(const SdfWeapon& Weapon, const Field& CoarseOnly, const V3& Lo, const V3& Hi, double Spacing, double Band, const Exec& Ex);
}
