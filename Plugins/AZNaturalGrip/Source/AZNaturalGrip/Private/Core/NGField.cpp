// Copyright Artur. AZ project.
#include "NGField.h"

#include "NGJson.h"

namespace ng
{
	bool Lattice::TrySample(double X, double Y, double Z, double& Out) const
	{
		const double F[3] = {(X - Origin.x) / Spacing, (Y - Origin.y) / Spacing, (Z - Origin.z) / Spacing};
		long long I[3];
		for (int K = 0; K < 3; ++K)
		{
			if (!(F[K] > -1e9 && F[K] < 1e9))      // also rejects NaN
			{
				return false;
			}
			I[K] = static_cast<long long>(std::floor(F[K]));
			if (I[K] < 0 || I[K] >= Dims[K] - 1)
			{
				return false;
			}
		}
		const double T[3] = {F[0] - static_cast<double>(I[0]), F[1] - static_cast<double>(I[1]), F[2] - static_cast<double>(I[2])};
		double C = 0.0;
		for (int Dx = 0; Dx < 2; ++Dx)
		{
			for (int Dy = 0; Dy < 2; ++Dy)
			{
				for (int Dz = 0; Dz < 2; ++Dz)
				{
					const double W = (Dx ? T[0] : 1 - T[0]) * (Dy ? T[1] : 1 - T[1]) * (Dz ? T[2] : 1 - T[2]);
					C += W * D[static_cast<size_t>(((I[0] + Dx) * Dims[1] + I[1] + Dy) * Dims[2] + I[2] + Dz)];
				}
			}
		}
		Out = C;
		return true;
	}

	double Field::WithObstacles(double V, double X, double Y, double Z) const
	{
		const V3 P(X, Y, Z);
		for (const Capsule& Cp : Obstacles)
		{
			const V3 Ab = Sub(Cp.B, Cp.A);
			const double Den = Dot(Ab, Ab);
			double T = Den > 1e-12 ? Dot(Sub(P, Cp.A), Ab) / Den : 0.0;
			T = T < 0.0 ? 0.0 : (T > 1.0 ? 1.0 : T);
			const double D = Length(Sub(P, Add(Cp.A, Mul(Ab, T)))) - Cp.R;
			if (D < V)
			{
				V = D;
			}
		}
		return V;
	}

	bool Lattice::LoadJson(const std::string& Path, std::string& Error)
	{
		JValue J;
		if (!LoadJsonFile(Path, J, Error))
		{
			return false;
		}
		try
		{
			const JValue& O = J["origin"];
			Origin = V3(O.NumAt(0), O.NumAt(1), O.NumAt(2));
			Spacing = J["spacing"].Number();
			const JValue& N = J["dims"];
			for (int K = 0; K < 3; ++K)
			{
				Dims[K] = static_cast<int>(N.NumAt(K));
			}
			const JValue* Values = J.Find("d");
			if (!Values)
			{
				Values = &J["distances"];
			}
			D = Values->Numbers();
		}
		catch (const std::exception& E)
		{
			Error = Path + ": " + E.what();
			return false;
		}
		if (!IsValid())
		{
			Error = Path + ": inconsistent lattice";
			return false;
		}
		return true;
	}
}
