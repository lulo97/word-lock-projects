#pragma once

// Small fallback illustrations for seeded rows. They contain only shapes: no
// text, labels, or words are baked into the SVG image itself.
namespace wordlock {

inline constexpr char kAppleIcon[] =
    R"SVG(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64"><path d="M32 21C21 13 9 20 9 35c0 14 9 23 17 23 3 0 4-2 6-2s3 2 6 2c8 0 17-9 17-23 0-15-12-22-23-14z" fill="#dc2626"/><path d="M32 21c0-7 4-12 10-14" fill="none" stroke="#7c2d12" stroke-width="4" stroke-linecap="round"/><path d="M32 19c5-8 13-9 18-4-5 6-12 8-18 4z" fill="#16a34a"/></svg>)SVG";

inline constexpr char kCherryBlossomIcon[] =
    R"SVG(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64"><g fill="#f472b6"><ellipse cx="32" cy="15" rx="8" ry="12"/><ellipse cx="48" cy="26" rx="8" ry="12" transform="rotate(72 48 26)"/><ellipse cx="42" cy="45" rx="8" ry="12" transform="rotate(144 42 45)"/><ellipse cx="22" cy="45" rx="8" ry="12" transform="rotate(216 22 45)"/><ellipse cx="16" cy="26" rx="8" ry="12" transform="rotate(288 16 26)"/></g><circle cx="32" cy="32" r="6" fill="#fbbf24"/></svg>)SVG";

inline constexpr char kAlphaIcon[] =
    R"SVG(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64"><path d="M24 10l6 16h4l6-16z" fill="#dc2626"/><circle cx="32" cy="40" r="16" fill="#fbbf24" stroke="#b45309" stroke-width="3"/><path d="M24 40l5 5 11-11" fill="none" stroke="#92400e" stroke-width="3" stroke-linecap="round" stroke-linejoin="round"/></svg>)SVG";

inline constexpr char kBravoIcon[] =
    R"SVG(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64"><path d="M18 30h9l6-14c2-5 9-3 8 3l-3 11h10c4 0 6 4 4 7l-7 15H18z" fill="#2563eb"/><rect x="8" y="30" width="10" height="22" rx="3" fill="#1d4ed8"/></svg>)SVG";

inline constexpr char kCharlieIcon[] =
    R"SVG(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64"><rect x="9" y="17" width="46" height="32" rx="6" fill="#e0f2fe" stroke="#0284c7" stroke-width="3"/><path d="M37 17v-6M18 29h21M18 38h14" fill="none" stroke="#0284c7" stroke-width="3" stroke-linecap="round"/><circle cx="45" cy="42" r="3" fill="#0284c7"/></svg>)SVG";

inline constexpr char kDeltaIcon[] =
    R"SVG(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64"><path d="M8 49L29 15l12 21 6-10 9 23z" fill="#0ea5e9"/><path d="M8 49h48" fill="none" stroke="#075985" stroke-width="3" stroke-linecap="round"/></svg>)SVG";

inline constexpr char kCatIcon[] =
    R"SVG(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64"><path d="M13 27l3-15 13 8h6l13-8 3 15c3 14-6 25-21 25S10 41 13 27z" fill="#f59e0b"/><circle cx="24" cy="34" r="3" fill="#1f2937"/><circle cx="40" cy="34" r="3" fill="#1f2937"/><path d="M29 42l3 3 3-3M32 45v3" fill="none" stroke="#1f2937" stroke-width="2" stroke-linecap="round"/></svg>)SVG";

inline constexpr char kDogIcon[] =
    R"SVG(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64"><path d="M14 25c0-9 8-15 18-15s18 6 18 15v11c0 12-8 19-18 19S14 48 14 36z" fill="#a16207"/><path d="M14 26L7 13l12 5M50 26l7-13-12 5" fill="#854d0e"/><circle cx="25" cy="34" r="3" fill="#1f2937"/><circle cx="39" cy="34" r="3" fill="#1f2937"/><ellipse cx="32" cy="43" rx="4" ry="3" fill="#1f2937"/><path d="M32 46v3" fill="none" stroke="#1f2937" stroke-width="2" stroke-linecap="round"/></svg>)SVG";

inline constexpr char kFishIcon[] =
    R"SVG(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64"><path d="M12 32c9-12 25-14 36 0-11 14-27 12-36 0z" fill="#38bdf8"/><path d="M48 32l10-10v20z" fill="#0284c7"/><circle cx="23" cy="30" r="2" fill="#0f172a"/></svg>)SVG";

inline constexpr char kMountainIcon[] =
    R"SVG(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64"><path d="M6 52L27 18l11 18 7-11 13 27z" fill="#64748b"/><path d="M21 27l6-9 6 10-6-3z" fill="#f8fafc"/><path d="M6 52h52" fill="none" stroke="#334155" stroke-width="3" stroke-linecap="round"/></svg>)SVG";

}  // namespace wordlock
