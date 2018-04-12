
#include "tools.h"
#include <chrono>
#include <iostream>
#include <cstdint>

using namespace std;
using namespace std::chrono;

int64_t stopWatch(std::function<void()> fn) {
  high_resolution_clock::time_point t1 = high_resolution_clock::now();
  fn();
  high_resolution_clock::time_point t2 = high_resolution_clock::now();

  auto duration = duration_cast<microseconds>( t2 - t1 ).count();

  cout << duration/1000.0 << "" << endl;
  return duration;
}
