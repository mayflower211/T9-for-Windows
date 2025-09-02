// src/T9Engine.h
#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory> // для std::unique_ptr

// Структура для узла нашего Префиксного Дерева
struct TrieNode {
    // Каждая ветка - это следующая цифра T9 (от '2' до '9')
    std::map<char, std::unique_ptr<TrieNode>> children;
    // Список слов, которые заканчиваются в этом узле
    std::vector<std::wstring> words;
};

enum class T9Language {
    Russian,
    English
};

class T9Engine {
public:
    T9Engine();

    // Загрузка словарей (основного и пользовательского)
    void loadDictionaries(
        const std::string& base_ru_path, const std::string& user_ru_path,
        const std::string& base_en_path, const std::string& user_en_path
    );

    // Поиск слов-кандидатов с использованием дерева
    std::vector<std::wstring> getSuggestions(const std::wstring& sequence, T9Language lang);

    // Добавление нового слова в пользовательский словарь
    void addWord(const std::wstring& word, T9Language lang);

private:
    // Функция-помощник для вставки одного слова в дерево
    void insertWord(const std::wstring& word, T9Language lang);
    
    // Функция-помощник для рекурсивного обхода дерева и сбора всех дочерних слов
    void collectWords(TrieNode* node, std::vector<std::wstring>& result);

    // Корневые узлы деревьев для каждого языка
    std::unique_ptr<TrieNode> m_root_ru;
    std::unique_ptr<TrieNode> m_root_en;

    // Карта "буква -> цифра"
    std::map<wchar_t, char> m_map_ru;
    std::map<wchar_t, char> m_map_en;

    // Пути к пользовательским словарям
    std::string m_user_path_ru;
    std::string m_user_path_en;
};