#include <cmath>
#include <stdexcept>

extern "C" __declspec(dllexport) double ln(double x) {
    if (!(x > 0.0)) {
        throw std::domain_error("аргумент должен быть > 0");
    }
    return std::log(x);
}
