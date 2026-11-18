// Virtual time affects only the receiver under test, not production clocks.
#define PltGetMicroseconds AdaptiveTestClock
#include "../../../moonlight-common-c/AdaptiveVideoQueue.c"
