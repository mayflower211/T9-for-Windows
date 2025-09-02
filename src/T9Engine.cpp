// src/T9Engine.cpp
#include "T9Engine.h"
#include <fstream>
#include <locale>
#include <algorithm>
#include <windows.h> // Подключаем для нативных функций конвертации

// --- НОВЫЕ, НАДЕЖНЫЕ ФУНКЦИИ КОНВЕРТАЦИИ ---
// wchar_t* (широкая строка) -> char* (UTF-8 строка)
std::string to_utf8(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

// Конструктор: остался без изменений
T9Engine::T9Engine() {
    m_root_ru = std::make_unique<TrieNode>();
    m_root_en = std::make_unique<TrieNode>();
    // Карты T9 ... (остальной код конструктора тот же)
    m_map_ru[L'а'] = m_map_ru[L'б'] = m_map_ru[L'в'] = m_map_ru[L'г'] = '2';
    m_map_ru[L'д'] = m_map_ru[L'е'] = m_map_ru[L'ж'] = m_map_ru[L'з'] = '3';
    m_map_ru[L'и'] = m_map_ru[L'й'] = m_map_ru[L'к'] = m_map_ru[L'л'] = '4';
    m_map_ru[L'м'] = m_map_ru[L'н'] = m_map_ru[L'о'] = m_map_ru[L'п'] = '5';
    m_map_ru[L'р'] = m_map_ru[L'с'] = m_map_ru[L'т'] = m_map_ru[L'у'] = '6';
    m_map_ru[L'ф'] = m_map_ru[L'х'] = m_map_ru[L'ц'] = m_map_ru[L'ч'] = '7';
    m_map_ru[L'ш'] = m_map_ru[L'щ'] = m_map_ru[L'ъ'] = m_map_ru[L'ы'] = '8';
    m_map_ru[L'ь'] = m_map_ru[L'э'] = m_map_ru[L'ю'] = m_map_ru[L'я'] = '9';
    m_map_en[L'a'] = m_map_en[L'b'] = m_map_en[L'c'] = '2';
    m_map_en[L'd'] = m_map_en[L'e'] = m_map_en[L'f'] = '3';
    m_map_en[L'g'] = m_map_en[L'h'] = m_map_en[L'i'] = '4';
    m_map_en[L'j'] = m_map_en[L'k'] = m_map_en[L'l'] = '5';
    m_map_en[L'm'] = m_map_en[L'n'] = m_map_en[L'o'] = '6';
    m_map_en[L'p'] = m_map_en[L'q'] = m_map_en[L'r'] = m_map_en[L's'] = '7';
    m_map_en[L't'] = m_map_en[L'u'] = m_map_en[L'v'] = '8';
    m_map_en[L'w'] = m_map_en[L'x'] = m_map_en[L'y'] = m_map_en[L'z'] = '9';
}

void T9Engine::loadDictionaries(
    const std::string& base_ru_path, const std::string& user_ru_path,
    const std::string& base_en_path, const std::string& user_en_path
) {
    m_root_ru = std::make_unique<TrieNode>();
    m_root_en = std::make_unique<TrieNode>();
    m_user_path_ru = user_ru_path;
    m_user_path_en = user_en_path;

    auto load_from_file = [&](const std::string& path, T9Language lang) {
        // --- ИЗМЕНЕНИЕ: Читаем UTF-8 файл как обычный текстовый, без wifstream ---
        std::ifstream file(path);
        if (!file.is_open()) return;
        
        std::string line_utf8;
        while (std::getline(file, line_utf8)) {
            if (!line_utf8.empty() && line_utf8.back() == '\r') line_utf8.pop_back();
            if (!line_utf8.empty()) {
                // Конвертируем прочитанную UTF-8 строку в wstring для дальнейшей работы
                int size_needed = MultiByteToWideChar(CP_UTF8, 0, &line_utf8[0], (int)line_utf8.size(), NULL, 0);
                std::wstring wline(size_needed, 0);
                MultiByteToWideChar(CP_UTF8, 0, &line_utf8[0], (int)line_utf8.size(), &wline[0], size_needed);
                insertWord(wline, lang);
            }
        }
    };

    load_from_file(base_ru_path, T9Language::Russian);
    load_from_file(user_ru_path, T9Language::Russian);
    load_from_file(base_en_path, T9Language::English);
    load_from_file(user_en_path, T9Language::English);
}

void T9Engine::insertWord(const std::wstring& word, T9Language lang) {
    const auto& target_map = (lang == T9Language::Russian) ? m_map_ru : m_map_en;
    TrieNode* current_node = (lang == T9Language::Russian) ? m_root_ru.get() : m_root_en.get();

    for (wchar_t ch : word) {
        wchar_t lower_ch = towlower(ch);
        auto it = target_map.find(lower_ch);
        if (it == target_map.end()) return;

        char digit = it->second;
        if (current_node->children.find(digit) == current_node->children.end()) {
            current_node->children[digit] = std::make_unique<TrieNode>();
        }
        current_node = current_node->children[digit].get();
    }
    current_node->words.push_back(word);
}

std::vector<std::wstring> T9Engine::getSuggestions(const std::wstring& sequence, T9Language lang) {
    std::vector<std::wstring> result;
    if (sequence.empty()) return result;

    TrieNode* current_node = (lang == T9Language::Russian) ? m_root_ru.get() : m_root_en.get();
    
    std::string sequence_str = to_utf8(sequence);
    
    for (char digit : sequence_str) {
        if (current_node->children.find(digit) == current_node->children.end()) {
            return result;
        }
        current_node = current_node->children[digit].get();
    }

    collectWords(current_node, result);
    std::sort(result.begin(), result.end());
    return result;
}

void T9Engine::collectWords(TrieNode* node, std::vector<std::wstring>& result) {
    if (!node) return;
    for (const auto& word : node->words) {
        result.push_back(word);
        if (result.size() >= 10) return;
    }
    for (auto const& [key, val] : node->children) {
        collectWords(val.get(), result);
        if (result.size() >= 10) return;
    }
}

void T9Engine::addWord(const std::wstring& word, T9Language lang) {
    const std::string& path = (lang == T9Language::Russian) ? m_user_path_ru : m_user_path_en;
    
    insertWord(word, lang);

    // --- ИЗМЕНЕНИЕ: Используем ofstream и нашу UTF-8 конвертацию ---
    std::ofstream file(path, std::ios_base::app);
    if (!file.is_open()) {
        file.open(path);
        if (!file.is_open()) return;
    }
    file << std::endl << to_utf8(word);
}