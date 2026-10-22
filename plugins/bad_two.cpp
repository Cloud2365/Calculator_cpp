// Намеренно некорректный плагин: две функции. Загрузчик должен его отвергнуть.
extern "C" __declspec(dllexport) double alpha(double x) { return x + 1.0; }
extern "C" __declspec(dllexport) double beta(double x) { return x - 1.0; }
