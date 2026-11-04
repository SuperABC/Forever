#include "society/policy.h"

#include <cmath>


int64_t TaxOf(int64_t amount, double percent) {
	if (amount <= 0 || percent <= 0.0) return 0;
	return static_cast<int64_t>(std::floor(static_cast<double>(amount) * percent / 100.0 + 1e-9));
}

int64_t GovernmentPolicy::Tax(int64_t amount, int basePercent) const {
	return TaxOf(amount, basePercent * taxFactor);
}

int64_t GovernmentPolicy::Allowance(int64_t base) const {
	return static_cast<int64_t>(std::llround(static_cast<double>(base) * allowanceFactor));
}
