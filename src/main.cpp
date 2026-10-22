#include "expr.hpp"
#include "plugins.hpp"

#include <cmath>
#include <iostream>
#include <sstream>
#include <string>
#include <locale>
#include <windows.h>
namespace {

std::string format_number(double v) {
    if (!std::isfinite(v)) {
        throw std::runtime_error("результат не является конечным числом");
    }
    std::ostringstream oss;
    oss.precision(15);
    oss << v;
    return oss.str();
}

}  // namespace

int main() {
    SetConsoleOutputCP(65001);
    SetConsoleCP(1251);

    PluginRegistry plugins;
    try {
        plugins.load_from("plugins");
    } catch (const std::exception& ex) {
        std::cerr << "не удалось загрузить плагины: " << ex.what() << "\n";
    }

    std::cerr << "калькулятор. пустая строка или exit — выход.\n";

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty() || line == "exit" || line == "quit") {
            break;
        }
        try {
            double value = evaluate(line, plugins);
            std::cout << format_number(value) << std::endl;
        } catch (const std::exception& ex) {
            std::cout << "ошибка: " << ex.what() << std::endl;
        }
    }
    return 0;
}
