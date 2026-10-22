#pragma once

#include "plugins.hpp"

#include <string>

// Разбор и вычисление арифметического выражения.
// Грамматика (приоритет сверху вниз, ^ правоассоциативна):
//   expr   = term { ('+' | '-') term }
//   term   = power { ('*' | '/') power }
//   power  = unary { '^' power }
//   unary  = '-' unary | primary
//   primary = number | ident '(' expr ')' | '(' expr ')'
//
// Имя функции ищется только в реестре плагинов.
double evaluate(const std::string& input, const PluginRegistry& plugins);
