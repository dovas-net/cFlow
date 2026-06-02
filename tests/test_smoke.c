#define FLOW_IMPLEMENTATION
#include "../flow.h"
#include "flowtest.h"
int main(void) {
  ASSERT(1, "flow.h compiles and includes cleanly");
  return flowtest_report("test_smoke");
}
