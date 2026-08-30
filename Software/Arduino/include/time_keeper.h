#pragma once

#include <time.h>

namespace time_keeper {

void initializeFromCompileTime();
void applyElapsedWalltime();
void setCurrentTime(const tm &newTime);

}  // namespace time_keeper
