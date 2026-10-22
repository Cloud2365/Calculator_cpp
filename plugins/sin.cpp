#include <cmath>

// Имя sin уже занято библиотекой C. Функция называется иначе,
// а в DLL экспортируется как sin (см. vs/funcsin.def).
extern "C" double plugin_sin(double x) { return std::sin(x); }
