#define _USE_MATH_DEFINES
#include <cmath>
extern "C" __declspec(dllexport) double deg(double x) {
    return x * M_PI / 180.0;
}
