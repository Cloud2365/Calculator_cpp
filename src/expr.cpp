#include "expr.hpp"

#include <cctype>
#include <cmath>
#include <stdexcept>

namespace {

class Parser {
public:
    Parser(std::string in, const PluginRegistry& plugins)
        : s_(std::move(in)), plugins_(plugins) {}

    double parse() {
        skip();
        double v = parse_expr();
        skip();
        if (i_ != s_.size()) {
            throw std::runtime_error("лишние символы в позиции " + std::to_string(i_ + 1));
        }
        return v;
    }

private:
    const std::string s_;
    const PluginRegistry& plugins_;
    size_t i_ = 0;

    void skip() {
        while (i_ < s_.size() && std::isspace(static_cast<unsigned char>(s_[i_]))) {
            ++i_;
        }
    }

    bool eat(char c) {
        skip();
        if (i_ < s_.size() && s_[i_] == c) {
            ++i_;
            return true;
        }
        return false;
    }

    double parse_expr() {
        double v = parse_term();
        for (;;) {
            if (eat('+')) {
                v += parse_term();
            } else if (eat('-')) {
                v -= parse_term();
            } else {
                break;
            }
        }
        return v;
    }

    double parse_term() {
        double v = parse_power();
        for (;;) {
            if (eat('*')) {
                v *= parse_power();
            } else if (eat('/')) {
                double r = parse_power();
                if (r == 0.0) {
                    throw std::runtime_error("деление на ноль");
                }
                v /= r;
            } else {
                break;
            }
        }
        return v;
    }

    // Правоассоциативное возведение в степень: 2^3^2 = 2^(3^2) = 512.
    double parse_power() {
        double base = parse_unary();
        if (eat('^')) {
            double exp = parse_power();
            return std::pow(base, exp);
        }
        return base;
    }

    double parse_unary() {
        if (eat('+')) {
            return parse_unary();
        }
        if (eat('-')) {
            return -parse_unary();
        }
        return parse_primary();
    }

    double parse_primary() {
        skip();
        if (i_ >= s_.size()) {
            throw std::runtime_error("неожиданный конец выражения");
        }
        if (eat('(')) {
            double v = parse_expr();
            if (!eat(')')) {
                throw std::runtime_error("ожидалась ')'");
            }
            return v;
        }
        if (std::isalpha(static_cast<unsigned char>(s_[i_])) || s_[i_] == '_') {
            return parse_call();
        }
        return parse_number();
    }

    double parse_call() {
        size_t start = i_;
        ++i_;
        while (i_ < s_.size() &&
               (std::isalnum(static_cast<unsigned char>(s_[i_])) || s_[i_] == '_')) {
            ++i_;
        }
        std::string name = s_.substr(start, i_ - start);
        if (!eat('(')) {
            throw std::runtime_error("у функции '" + name + "' ожидалась '('");
        }
        double arg = parse_expr();
        if (!eat(')')) {
            throw std::runtime_error("у функции '" + name + "' ожидалась ')'");
        }
        auto it = plugins_.functions.find(name);
        if (it == plugins_.functions.end()) {
            throw std::runtime_error("неизвестная функция '" + name + "'");
        }
        try {
            return it->second(arg);
        } catch (const std::exception& ex) {
            throw std::runtime_error(std::string(name) + ": " + ex.what());
        } catch (...) {
            throw std::runtime_error(name + ": неизвестное исключение внутри плагина");
        }
    }

    double parse_number() {
        skip();
        size_t start = i_;
        bool digit = false;
        while (i_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[i_]))) {
            digit = true;
            ++i_;
        }
        if (i_ < s_.size() && s_[i_] == '.') {
            ++i_;
            while (i_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[i_]))) {
                digit = true;
                ++i_;
            }
        }
        if (!digit) {
            throw std::runtime_error("ожидалось число в позиции " + std::to_string(start + 1));
        }
        if (i_ < s_.size() && (s_[i_] == 'e' || s_[i_] == 'E')) {
            size_t epos = i_;
            ++i_;
            if (i_ < s_.size() && (s_[i_] == '+' || s_[i_] == '-')) {
                ++i_;
            }
            bool exp_digit = false;
            while (i_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[i_]))) {
                exp_digit = true;
                ++i_;
            }
            if (!exp_digit) {
                i_ = epos;
            }
        }
        try {
            return std::stod(s_.substr(start, i_ - start));
        } catch (const std::exception&) {
            throw std::runtime_error("некорректное число");
        }
    }
};

// Word часто подставляет типографские минусы вместо ASCII '-'.
std::string normalize(std::string s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size();) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c == 0xE2 && i + 2 < s.size()) {
            unsigned char b1 = static_cast<unsigned char>(s[i + 1]);
            unsigned char b2 = static_cast<unsigned char>(s[i + 2]);
            // U+2212 minus, U+2013 en dash, U+2014 em dash
            if ((b1 == 0x88 && b2 == 0x92) || (b1 == 0x80 && (b2 == 0x93 || b2 == 0x94))) {
                out.push_back('-');
                i += 3;
                continue;
            }
        }
        out.push_back(s[i]);
        ++i;
    }
    return out;
}

}  // namespace

double evaluate(const std::string& input, const PluginRegistry& plugins) {
    Parser p(normalize(input), plugins);
    return p.parse();
}
