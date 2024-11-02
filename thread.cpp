// Александров Дмитрий 21ПМ 
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <future>
#include <chrono>
#include <memory>
#include <windows.h>
#include <commdlg.h>
#include <atomic>

std::mutex mtx;

//проверка, является ли символ разделителем
bool isDelimiter(wchar_t c) {
    static const std::wstring delimiters = L" \t\n.,;!?()[]{}<>/\"'";
    return delimiters.find(c) != std::wstring::npos;
}

//преобразование UTF-8 в UTF-16 (wstring)
std::wstring utf8_to_wstring(const std::string& str) {
    if (str.empty()) return std::wstring();

    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

//обработка блока текста
void processBuffer(const std::wstring& buffer, std::shared_ptr<std::wstring> longestWord, std::atomic<size_t>& wordCount) {
    std::wstring currentWord;

    for (size_t i = 0; i < buffer.length(); ++i) {
        wchar_t c = buffer[i];

        if (isDelimiter(c)) {
            if (!currentWord.empty()) {
                ++wordCount;
                if (currentWord.length() > longestWord->length()) {
                    *longestWord = currentWord;  //используем указатель на строку
                }
                currentWord.clear();
            }
        }
        else {
            currentWord += c;
        }
    }

    //если последнее слово не завершено, добавляем его
    if (!currentWord.empty()) {
        ++wordCount;
        if (currentWord.length() > longestWord->length()) {
            *longestWord = currentWord;
        }
    }
}

//асинхронное чтение файла с параллельной обработкой
void readAndProcessFile(const std::wstring& filename, std::shared_ptr<std::wstring> longestWord, std::atomic<size_t>& wordCount) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::wcerr << L"Error opening file!" << std::endl;
        return;
    }

    const size_t bufferSize = 16 * 1024 * 1024;  // буфер в 16 МБ
    std::vector<char> buffer(bufferSize);
    std::vector<std::future<void>> futures;

    while (file) {
        file.read(buffer.data(), bufferSize);
        std::streamsize charsRead = file.gcount();

        if (charsRead > 0) {
            //преобразуем прочитанный буфер из UTF-8 в UTF-16 (wstring)
            std::wstring wbuffer = utf8_to_wstring(std::string(buffer.data(), charsRead));
            //создаем поток для обработки сегмента файла
            futures.push_back(std::async(std::launch::async, processBuffer, wbuffer, longestWord, std::ref(wordCount)));
        }
    }

    for (auto& f : futures) {
        f.get();  //ожидаем завершения всех потоков
    }

    file.close();
}

//функция для открытия диалогового окна проводника и выбора файла
std::wstring openFileDialog() {
    wchar_t filename[MAX_PATH] = L"";  //массив для хранения выбранного пути к файлу

    OPENFILENAMEW ofn;                 //структура для настройки диалогового окна
    ZeroMemory(&ofn, sizeof(ofn));     //обнуляем память структуры
    ofn.lStructSize = sizeof(ofn);     //Размер структуры
    ofn.hwndOwner = NULL;              //владелец окна, NULL для консольных приложений
    ofn.lpstrFilter = L"Text Files\0*.txt\0All Files\0*.*\0";  //фильтр для отображаемых файлов
    ofn.lpstrFile = filename;          //буфер для пути к файлу
    ofn.nMaxFile = MAX_PATH;           //максимальная длина пути
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;  //настройки диалога
    ofn.lpstrDefExt = L"txt";          //расширение по умолчанию

    if (GetOpenFileNameW(&ofn)) {      //открываем диалог выбора файла
        return std::wstring(filename); //возвращаем путь к выбранному файлу
    }
    else {
        std::wcerr << L"No file selected or error occurred." << std::endl;
        return L"";
    }
}

int main() {
    std::locale::global(std::locale("ru_RU.UTF-8"));

    auto longestWord = std::make_shared<std::wstring>();
    std::atomic<size_t> wordCount(0); 

    //выбираем файл через проводник
    std::wstring filename = openFileDialog();
    if (filename.empty()) {
        std::wcerr << L"No file selected." << std::endl;
        return 1;
    }

    //начало замера времени
    auto start = std::chrono::high_resolution_clock::now();

    //чтение и обработка файла
    readAndProcessFile(filename, longestWord, wordCount);

    //конец замера времени
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    //вывод результатов
    std::wcout.imbue(std::locale("ru_RU.UTF-8"));
    std::wcout << L"Total number of words: " << wordCount.load() << std::endl; 
    std::wcout << L"Longest word: " << *longestWord << std::endl;
    std::wcout << L"Time elapsed: " << elapsed.count() << L" seconds" << std::endl;

    return 0;
}
