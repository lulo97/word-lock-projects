#pragma once

#include <memory>
#include <string>
#include "app/window_host.h"
#include "word_db.h"

void bring_to_front(ui::WindowHost& host);

// Load an embedded HTML file by name (e.g. L"countdown.html").
// Returns empty wstring if not found.
std::wstring load_embedded_html(const wchar_t* name);

// Fallback HTML shown when a file is missing.
std::wstring missing_html_page(const wchar_t* name);

// Create a WindowHost from an embedded HTML file, optionally filling
// {{key}} templates with word data.
bool create_host(
    std::unique_ptr<ui::WindowHost>& host,
    const ui::WindowOptions& options,
    const wchar_t* htmlName,
    const WordData* word = nullptr
);

// Same, but from an HTML string already loaded from any asset (themes).
bool create_host_from_html(
    std::unique_ptr<ui::WindowHost>& host,
    const ui::WindowOptions& options,
    const std::wstring& html,
    const WordData* word = nullptr
);

// Position a window at the bottom-right of the work area with a margin.
void position_bottom_right(ui::WindowHost& host, float marginLogical);

// Position a window near the cursor, clamped to the work area.
void position_near_cursor(ui::WindowHost& host, float gapLogical);
