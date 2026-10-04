// Copyright Artur. AZ project.
// Natural Grip solver core: how the solvers run their parallel loops (no engine dependency).
// The host injects the loop: Unreal's ParallelFor in the editor, a std::thread pool in the ngtest harness, or SerialFor.
//
// DETERMINISM RULE for every solver: a parallel loop body writes only its own slot (results[i]); the best pick is then
// reduced on one thread in the Python loop order with the same strict '>' (first of equal scores wins). So the result
// is identical for any thread count and identical to the Python solver.
#pragma once

#include <atomic>
#include <functional>

namespace ng
{
	using ForBody = std::function<void(int)>;
	using ForFn = std::function<void(int /*Num*/, const ForBody&)>;

	inline void SerialFor(int Num, const ForBody& Body)
	{
		for (int I = 0; I < Num; ++I)
		{
			Body(I);
		}
	}

	struct Exec
	{
		ForFn For = SerialFor;
		/** Optional cancel flag (set by the UI); solvers check it between rows and return early. */
		const std::atomic<bool>* Cancel = nullptr;
		/** Optional progress callback; may be called from worker threads. */
		std::function<void(const char* /*Stage*/, double /*Fraction*/)> Progress;

		bool Cancelled() const { return Cancel && Cancel->load(std::memory_order_relaxed); }
		void Report(const char* Stage, double Fraction) const
		{
			if (Progress)
			{
				Progress(Stage, Fraction);
			}
		}
	};
}
