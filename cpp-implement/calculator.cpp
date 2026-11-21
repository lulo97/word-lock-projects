#include "ui/ui_framework.h"
#include <string>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <fstream>

std::wstring display = L"0";
double firstNumber = 0;
wchar_t currentOperation = 0;
bool waitingForNumber = false;
UIWindow* app = nullptr; // Pointer to access set_text from lambdas

std::wstring formatNumber(double value) {
    std::wostringstream out;
    if (std::abs(value - std::round(value)) < 0.000000001) {
        out << static_cast<long long>(std::round(value));
    } else {
        out << std::setprecision(12) << value;
    }
    return out.str();
}

std::wstring readHTMLFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) return L"<div class='bg-red-500 p-4 text-white'>Error: Could not open index.html</div>";
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (content.empty()) return L"";
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &content[0], (int)content.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &content[0], (int)content.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

void update_display() {
    app->set_text(L"display", display); // <--- Added L here
}

void input_number(wchar_t c) {
    if (waitingForNumber || display == L"0") {
        display = c;
        waitingForNumber = false;
    } else {
        display += c;
    }
    update_display();
}

void input_operator(wchar_t op) {
    if (currentOperation != 0 && !waitingForNumber) {
        double secondNumber = std::stod(display);
        if (currentOperation == L'+') firstNumber += secondNumber;
        if (currentOperation == L'-') firstNumber -= secondNumber;
        display = formatNumber(firstNumber);
    } else {
        firstNumber = std::stod(display);
    }
    currentOperation = op;
    waitingForNumber = true;
    update_display();
}

void calculate_equals() {
    if (currentOperation != 0) {
        double secondNumber = std::stod(display);
        double result = firstNumber;
        if (currentOperation == L'+') result += secondNumber;
        if (currentOperation == L'-') result -= secondNumber;
        display = formatNumber(result);
        currentOperation = 0;
        waitingForNumber = true;
    }
    update_display();
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
    UIWindow window(L"C++ Calculator", 460, 620);
    app = &window;

    window.load_html(readHTMLFile("index.html"));

    // Bind all the number buttons cleanly
    window.bind_click(L"btn-0", [] { input_number(L'0'); }); // <--- Added L
    window.bind_click(L"btn-1", [] { input_number(L'1'); });
    window.bind_click(L"btn-2", [] { input_number(L'2'); });
    window.bind_click(L"btn-3", [] { input_number(L'3'); });
    window.bind_click(L"btn-4", [] { input_number(L'4'); });
    window.bind_click(L"btn-5", [] { input_number(L'5'); });
    window.bind_click(L"btn-6", [] { input_number(L'6'); });
    window.bind_click(L"btn-7", [] { input_number(L'7'); });
    window.bind_click(L"btn-8", [] { input_number(L'8'); });
    window.bind_click(L"btn-9", [] { input_number(L'9'); });

    // Bind operators
    window.bind_click(L"btn-add", [] { input_operator(L'+'); });
    window.bind_click(L"btn-sub", [] { input_operator(L'-'); });
    window.bind_click(L"btn-eq",  [] { calculate_equals(); });

    // Bind Clear
    window.bind_click(L"btn-clear", [] { 
        display = L"0";
        firstNumber = 0;
        currentOperation = 0;
        waitingForNumber = false;
        update_display();
    });

    window.run();
    return 0;
}