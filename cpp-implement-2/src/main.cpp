#include "app.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    return wordlock::Run(hInstance, nCmdShow);
}
