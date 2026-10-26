#include "plugins.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace fs = std::filesystem;

struct PluginRegistry::Handle {
    HMODULE raw = nullptr;
    explicit Handle(HMODULE h) : raw(h) {}
    ~Handle() {
        if (raw) {
            FreeLibrary(raw);
        }
    }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
};

PluginRegistry::PluginRegistry() = default;
PluginRegistry::~PluginRegistry() = default;

namespace {

std::vector<unsigned char> read_file(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("не удалось прочитать файл");
    }
    return std::vector<unsigned char>((std::istreambuf_iterator<char>(in)),
                                      std::istreambuf_iterator<char>());
}

void consider_name(std::vector<std::string>& found, const char* name) {
    if (!name || name[0] == '\0' || name[0] == '_') {
        return;
    }
    found.emplace_back(name);
}

uint32_t rva_to_offset(const unsigned char* buf, size_t size, uint32_t rva,
                       const IMAGE_SECTION_HEADER* sections, unsigned nsec) {
    for (unsigned i = 0; i < nsec; ++i) {
        uint32_t va = sections[i].VirtualAddress;
        uint32_t span = sections[i].Misc.VirtualSize;
        if (span < sections[i].SizeOfRawData) {
            span = sections[i].SizeOfRawData;
        }
        if (rva >= va && rva < va + span) {
            uint32_t off = sections[i].PointerToRawData + (rva - va);
            if (off >= size) {
                throw std::runtime_error("битый RVA в DLL");
            }
            return off;
        }
    }
    throw std::runtime_error("RVA вне секций DLL");
}

// Имя функции — единственный экспорт DLL, не имя файла.
std::vector<std::string> exported_functions(const fs::path& path) {
    auto buf = read_file(path);
    if (buf.size() < sizeof(IMAGE_DOS_HEADER)) {
        throw std::runtime_error("это не DLL");
    }
    IMAGE_DOS_HEADER dos{};
    std::memcpy(&dos, buf.data(), sizeof(dos));
    if (dos.e_magic != IMAGE_DOS_SIGNATURE) {
        throw std::runtime_error("это не DLL");
    }
    if (dos.e_lfanew <= 0 || static_cast<size_t>(dos.e_lfanew) + sizeof(IMAGE_NT_HEADERS64) > buf.size()) {
        throw std::runtime_error("обрезанный PE");
    }

    DWORD sig = 0;
    std::memcpy(&sig, buf.data() + dos.e_lfanew, sizeof(sig));
    if (sig != IMAGE_NT_SIGNATURE) {
        throw std::runtime_error("это не DLL");
    }

    IMAGE_FILE_HEADER file{};
    std::memcpy(&file, buf.data() + dos.e_lfanew + sizeof(DWORD), sizeof(file));
    size_t opt_off = static_cast<size_t>(dos.e_lfanew) + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER);
    if (opt_off + file.SizeOfOptionalHeader > buf.size()) {
        throw std::runtime_error("обрезанный PE");
    }

    WORD magic = 0;
    std::memcpy(&magic, buf.data() + opt_off, sizeof(magic));
    IMAGE_DATA_DIRECTORY exp_dir{};
    if (magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        IMAGE_OPTIONAL_HEADER64 opt{};
        std::memcpy(&opt, buf.data() + opt_off, sizeof(opt));
        exp_dir = opt.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    } else if (magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        IMAGE_OPTIONAL_HEADER32 opt{};
        std::memcpy(&opt, buf.data() + opt_off, sizeof(opt));
        exp_dir = opt.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    } else {
        throw std::runtime_error("неподдерживаемый PE");
    }

    size_t sec_off = opt_off + file.SizeOfOptionalHeader;
    if (sec_off + static_cast<size_t>(file.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER) > buf.size()) {
        throw std::runtime_error("обрезанный PE");
    }
    std::vector<IMAGE_SECTION_HEADER> sections(file.NumberOfSections);
    std::memcpy(sections.data(), buf.data() + sec_off, sections.size() * sizeof(IMAGE_SECTION_HEADER));

    if (exp_dir.VirtualAddress == 0 || exp_dir.Size == 0) {
        return {};
    }

    uint32_t exp_off = rva_to_offset(buf.data(), buf.size(), exp_dir.VirtualAddress, sections.data(),
                                     file.NumberOfSections);
    if (exp_off + sizeof(IMAGE_EXPORT_DIRECTORY) > buf.size()) {
        throw std::runtime_error("битая таблица экспорта");
    }
    IMAGE_EXPORT_DIRECTORY exp{};
    std::memcpy(&exp, buf.data() + exp_off, sizeof(exp));

    std::vector<std::string> found;
    if (exp.NumberOfNames == 0 || exp.AddressOfNames == 0) {
        return found;
    }
    uint32_t names_off = rva_to_offset(buf.data(), buf.size(), exp.AddressOfNames, sections.data(),
                                       file.NumberOfSections);
    for (DWORD i = 0; i < exp.NumberOfNames; ++i) {
        if (names_off + (i + 1) * sizeof(DWORD) > buf.size()) {
            break;
        }
        DWORD name_rva = 0;
        std::memcpy(&name_rva, buf.data() + names_off + i * sizeof(DWORD), sizeof(name_rva));
        uint32_t name_off = rva_to_offset(buf.data(), buf.size(), name_rva, sections.data(),
                                          file.NumberOfSections);
        const char* name = reinterpret_cast<const char*>(buf.data() + name_off);
        consider_name(found, name);
    }
    std::sort(found.begin(), found.end());
    found.erase(std::unique(found.begin(), found.end()), found.end());
    return found;
}

}  // namespace

void PluginRegistry::load_from(const std::string& directory) {
    std::error_code ec;
    if (!fs::exists(directory, ec) || !fs::is_directory(directory, ec)) {
        std::cerr << "каталог плагинов не найден (" << directory
                  << "): доступны только операции + - * / ^ ()\n";
        return;
    }

    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(directory, ec)) {
        if (ec) {
            break;
        }
        if (!entry.is_regular_file()) {
            continue;
        }
        auto ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        if (ext == ".dll") {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());

    if (files.empty()) {
        std::cerr << "в " << directory << " нет библиотек: доступны только + - * / ^ ()\n";
        return;
    }

    for (const auto& path : files) {
        try {
            auto names = exported_functions(path);
            if (names.size() != 1) {
                std::cerr << "пропуск " << path.filename().string()
                          << ": ожидалась ровно одна экспортируемая функция, найдено "
                          << names.size() << "\n";
                continue;
            }
            const std::string& fname = names[0];

            HMODULE raw = LoadLibraryW(path.c_str());
            if (!raw) {
                std::cerr << "пропуск " << path.filename().string()
                          << ": LoadLibrary, код " << GetLastError() << "\n";
                continue;
            }
            auto handle = std::make_unique<Handle>(raw);
            FARPROC sym = GetProcAddress(raw, fname.c_str());
            if (!sym) {
                std::cerr << "пропуск " << path.filename().string()
                          << ": символ " << fname << " не загрузился\n";
                continue;
            }
            if (functions.count(fname)) {
                std::cerr << "пропуск " << path.filename().string()
                          << ": функция '" << fname << "' уже зарегистрирована\n";
                continue;
            }

            functions.emplace(fname, reinterpret_cast<UnaryFn>(sym));
            std::cerr << "плагин " << path.filename().string() << " -> " << fname << "()\n";
            handles.push_back(std::move(handle));
        } catch (const std::exception& ex) {
            std::cerr << "пропуск " << path.filename().string() << ": " << ex.what() << "\n";
        }
    }
}
