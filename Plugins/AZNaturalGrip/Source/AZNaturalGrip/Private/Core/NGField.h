// Copyright Artur. AZ project.
// Natural Grip solver core: the weapon's signed-distance field (port of Tools/wgs/natgrip/views.py field lookup).
// A coarse lattice over the whole weapon (0.5 cm) and an optional fine lattice around one hand's grip (0.15-0.25 cm);
// trilinear lookup, the fine lattice wins inside its box, outside both the distance is 10 cm (free space).
#pragma once

#include "NGMath.h"

#include <string>
#include <vector>

namespace ng
{
	struct Lattice
	{
		V3 Origin;
		double Spacing = 0.5;
		int Dims[3] = {0, 0, 0};
		std::vector<double> D;      // index = (X * Dims[1] + Y) * Dims[2] + Z, cm, < 0 inside

		bool IsValid() const
		{
			return Spacing > 0.0 && Dims[0] > 1 && Dims[1] > 1 && Dims[2] > 1
				&& D.size() == static_cast<size_t>(Dims[0]) * Dims[1] * Dims[2];
		}

		/** Trilinear value; false when the point's cell is outside the lattice (views._fine / coarse_field). */
		bool TrySample(double X, double Y, double Z, double& Out) const;

		/** Reads {"origin", "spacing", "dims", "d" | "distances"} (bake_grip_field.py / fine_field.py output). */
		bool LoadJson(const std::string& Path, std::string& Error);
	};

	/** A capsule obstacle (segment A-B, radius R, cm): e.g. the other hand's phalanges. */
	struct Capsule
	{
		V3 A, B;
		double R = 0.0;
	};

	struct Field
	{
		/** Extra obstacles merged into the field (min): signed distance to the capsule surfaces. Empty = the weapon only. */
		std::vector<Capsule> Obstacles;

		Lattice Coarse;
		Lattice Fine;
		bool bUseFine = false;
		double OutsideDistance = 10.0;

		/** views.field: the fine lattice inside its box, else the coarse one, else OutsideDistance. */
		double Sample(double X, double Y, double Z) const
		{
			double V;
			if (!(bUseFine && Fine.TrySample(X, Y, Z, V)))
			{
				V = CoarseSample(X, Y, Z);
			}
			if (!Obstacles.empty())
			{
				V = WithObstacles(V, X, Y, Z);
			}
			return V;
		}
		double Sample(const V3& P) const { return Sample(P.x, P.y, P.z); }

		double WithObstacles(double V, double X, double Y, double Z) const;

		double CoarseSample(double X, double Y, double Z) const
		{
			double V;
			return Coarse.TrySample(X, Y, Z, V) ? V : OutsideDistance;
		}
	};
}
