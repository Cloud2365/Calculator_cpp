#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// Унарная функция плагина. Исключение из DLL ловится в калькуляторе.
using UnaryFn = double (*)(double);

struct PluginRegistry {
    std::unordered_map<std::string, UnaryFn> functions;

    // LoadLibrary-хэндлы живут здесь и закрываются в деструкторе.
    struct Handle;
    std::vector<std::unique_ptr<Handle>> handles;

    PluginRegistry();
    ~PluginRegistry();

    PluginRegistry(const PluginRegistry&) = delete;
    PluginRegistry& operator=(const PluginRegistry&) = delete;

    // Каталог ./plugins относительно текущего рабочего каталога.
    // Некорректные библиотеки пропускаются с сообщением в stderr.
    // Отсутствие каталога или библиотек — не ошибка: остаются + - * / ^ ().
    void load_from(const std::string& directory);
};
