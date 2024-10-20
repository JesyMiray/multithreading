// ����������� ������� 21�� 
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

//��������, �������� �� ������ ������������
bool isDelimiter(wchar_t c) {
    static const std::wstring delimiters = L" \t\n.,;!?()[]{}<>/\"'";
    return delimiters.find(c) != std::wstring::npos;
}

//�������������� UTF-8 � UTF-16 (wstring)
std::wstring utf8_to_wstring(const std::string& str) {
    if (str.empty()) return std::wstring();

    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

//��������� ����� ������
void processBuffer(const std::wstring& buffer, std::shared_ptr<std::wstring> longestWord, std::atomic<size_t>& wordCount) {
    std::wstring currentWord;

    for (size_t i = 0; i < buffer.length(); ++i) {
        wchar_t c = buffer[i];

        if (isDelimiter(c)) {
            if (!currentWord.empty()) {
                ++wordCount;
                if (currentWord.length() > longestWord->length()) {
                    *longestWord = currentWord;  //���������� ��������� �� ������
                }
                currentWord.clear();
            }
        }
        else {
            currentWord += c;
        }
    }

    //���� ��������� ����� �� ���������, ��������� ���
    if (!currentWord.empty()) {
        ++wordCount;
        if (currentWord.length() > longestWord->length()) {
            *longestWord = currentWord;
        }
    }
}

//����������� ������ ����� � ������������ ����������
void readAndProcessFile(const std::wstring& filename, std::shared_ptr<std::wstring> longestWord, std::atomic<size_t>& wordCount) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::wcerr << L"Error opening file!" << std::endl;
        return;
    }

    const size_t bufferSize = 16 * 1024 * 1024;  // ����� � 16 ��
    std::vector<char> buffer(bufferSize);
    std::vector<std::future<void>> futures;

    while (file) {
        file.read(buffer.data(), bufferSize);
        std::streamsize charsRead = file.gcount();

        if (charsRead > 0) {
            //����������� ����������� ����� �� UTF-8 � UTF-16 (wstring)
            std::wstring wbuffer = utf8_to_wstring(std::string(buffer.data(), charsRead));
            //������� ����� ��� ��������� �������� �����
            futures.push_back(std::async(std::launch::async, processBuffer, wbuffer, longestWord, std::ref(wordCount)));
        }
    }

    for (auto& f : futures) {
        f.get();  //������� ���������� ���� �������
    }

    file.close();
}

//������� ��� �������� ����������� ���� ���������� � ������ �����
std::wstring openFileDialog() {
    wchar_t filename[MAX_PATH] = L"";  //������ ��� �������� ���������� ���� � �����

    OPENFILENAMEW ofn;                 //��������� ��� ��������� ����������� ����
    ZeroMemory(&ofn, sizeof(ofn));     //�������� ������ ���������
    ofn.lStructSize = sizeof(ofn);     //������ ���������
    ofn.hwndOwner = NULL;              //�������� ����, NULL ��� ���������� ����������
    ofn.lpstrFilter = L"Text Files\0*.txt\0All Files\0*.*\0";  //������ ��� ������������ ������
    ofn.lpstrFile = filename;          //����� ��� ���� � �����
    ofn.nMaxFile = MAX_PATH;           //������������ ����� ����
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;  //��������� �������
    ofn.lpstrDefExt = L"txt";          //���������� �� ���������

    if (GetOpenFileNameW(&ofn)) {      //��������� ������ ������ �����
        return std::wstring(filename); //���������� ���� � ���������� �����
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

    //�������� ���� ����� ���������
    std::wstring filename = openFileDialog();
    if (filename.empty()) {
        std::wcerr << L"No file selected." << std::endl;
        return 1;
    }

    //������ ������ �������
    auto start = std::chrono::high_resolution_clock::now();

    //������ � ��������� �����
    readAndProcessFile(filename, longestWord, wordCount);

    //����� ������ �������
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    //����� �����������
    std::wcout.imbue(std::locale("ru_RU.UTF-8"));
    std::wcout << L"Total number of words: " << wordCount.load() << std::endl; 
    std::wcout << L"Longest word: " << *longestWord << std::endl;
    std::wcout << L"Time elapsed: " << elapsed.count() << L" seconds" << std::endl;

    return 0;
}
