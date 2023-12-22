#include "jobsys.h"

jsrlib::JobSystem jsr::jobsys(std::thread::hardware_concurrency(), 256);

