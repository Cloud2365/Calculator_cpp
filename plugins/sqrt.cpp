#include <cmath>
#include <stdexcept>

// Имя sqrt уже занято библиотекой C. В DLL экспортируется как sqrt (см. vs/funcsqrt.def).
extern "C" double plugin_sqrt(double x) {
    if (x < 0.0) {
        throw std::domain_error("аргумент должен быть >= 0");
    }
    return std::sqrt(x);
}
