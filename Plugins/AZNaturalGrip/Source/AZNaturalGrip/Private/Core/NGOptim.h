// Copyright Artur. AZ project.
// Natural Grip solver core, part B: local optimisers used instead of exhaustive grids (no engine dependency).
//
//  * PatternMaximize: bounded pattern search (Hooke & Jeeves 1961; the same move set index_nat.refine uses): per step
//    size, try +-step on every coordinate and on the listed coordinate pairs (moving one up and one down slides along a
//    valley), accept every strict improvement, halve when nothing improves. Derivative-free, so it works on the solver's
//    scores with hard rules (penetration, finger overlap) - a rejected point is simply not an improvement.
//  * The scores are the SAME functions the exhaustive (Python-parity) solvers maximise, so part B is checked against
//    part A on equal terms (Tools/ngtest "fast" mode).
#pragma once

#include <functional>
#include <utility>
#include <vector>

namespace ng
{
	/** F(X, Score): false = X is not allowed (a hard rule fails). Maximised. */
	using ScoreFn = std::function<bool(const double* /*X*/, double& /*Score*/)>;

	struct PatternOptions
	{
		std::vector<double> Lo, Hi;                     // bounds per coordinate
		std::vector<double> Scale;                      // per-coordinate step multiplier (e.g. 1 for degrees, 0.01 for ratios)
		std::vector<double> Steps;                      // step sizes, largest first
		std::vector<std::pair<int, int>> Pairs;         // coordinate pairs also moved together in opposite directions
		double MinGain = 1e-9;                          // an improvement must exceed this
	};

	/** Maximises F from X (in place; X must be allowed). Returns the best score; Evals counts F calls. */
	inline double PatternMaximize(const ScoreFn& F, std::vector<double>& X, double Score, const PatternOptions& O, long long& Evals)
	{
		const size_t N = X.size();
		std::vector<double> Y(N);
		auto Try = [&](const std::vector<double>& Cand) -> bool
		{
			double S = 0.0;
			++Evals;
			if (F(Cand.data(), S) && S > Score + O.MinGain)
			{
				X = Cand;
				Score = S;
				return true;
			}
			return false;
		};
		auto Clamp = [&](size_t K, double V)
		{
			return V < O.Lo[K] ? O.Lo[K] : (V > O.Hi[K] ? O.Hi[K] : V);
		};
		for (const double Step : O.Steps)
		{
			bool bImproved = true;
			while (bImproved)
			{
				bImproved = false;
				for (size_t K = 0; K < N; ++K)
				{
					for (const double Sign : {-1.0, 1.0})
					{
						Y = X;
						Y[K] = Clamp(K, Y[K] + Sign * Step * O.Scale[K]);
						if (Y[K] != X[K] && Try(Y))
						{
							bImproved = true;
						}
					}
				}
				for (const auto& Pr : O.Pairs)
				{
					for (const double Sign : {-1.0, 1.0})
					{
						Y = X;
						const size_t A = static_cast<size_t>(Pr.first), B = static_cast<size_t>(Pr.second);
						Y[A] = Clamp(A, Y[A] + Sign * Step * O.Scale[A]);
						Y[B] = Clamp(B, Y[B] - Sign * Step * O.Scale[B]);
						if ((Y[A] != X[A] || Y[B] != X[B]) && Try(Y))
						{
							bImproved = true;
						}
					}
				}
			}
		}
		return Score;
	}
}
