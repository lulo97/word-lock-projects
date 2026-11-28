#include "app.h"
#include "default_icons.h"

namespace wordlock {

void SeedTestData() {
    if (!IsTestDataMode()) {
        return;
    }

    char* sql = sqlite3_mprintf(
        "INSERT OR IGNORE INTO english_word "
        "(id, word, meaning_vi, examples, examples_vi, part_of_speech, svg_icon) VALUES "
        "(1, 'alpha', 'chữ cái đầu', '[\"alpha example\"]', '[\"ví dụ alpha\"]', 'noun', %Q), "
        "(2, 'bravo', 'giỏi', '[\"bravo example\"]', '[\"ví dụ bravo\"]', 'adjective', %Q), "
        "(3, 'charlie', 'tên riêng', '[\"charlie example\"]', '[\"ví dụ charlie\"]', 'noun', %Q), "
        "(4, 'delta', 'sự thay đổi', '[\"delta example\"]', '[\"ví dụ delta\"]', 'noun', %Q)",
        kAlphaIcon, kBravoIcon, kCharlieIcon, kDeltaIcon);
    ExecuteSql(sql);
    sqlite3_free(sql);

    sql = sqlite3_mprintf(
        "INSERT OR IGNORE INTO japanese_word "
        "(id, kanji, hiragana, katakana, romaji, meaning_en, examples, examples_en, "
        "examples_vi, part_of_speech, svg_icon) "
        "VALUES "
        "(1, '猫', 'ねこ', 'ネコ', 'neko', 'cat', '[\"猫がいます。\"]', "
        "'[\"There is a cat.\"]', '[\"Có một con mèo.\"]', 'noun', %Q), "
        "(2, '犬', 'いぬ', 'イヌ', 'inu', 'dog', '[\"犬が走ります。\"]', "
        "'[\"The dog runs.\"]', '[\"Con chó chạy.\"]', 'noun', %Q), "
        "(3, '魚', 'さかな', 'サカナ', 'sakana', 'fish', '[\"魚を食べます。\"]', "
        "'[\"I eat fish.\"]', '[\"Tôi ăn cá.\"]', 'noun', %Q), "
        "(4, '山', 'やま', 'ヤマ', 'yama', 'mountain', '[\"山を見ます。\"]', "
        "'[\"I see a mountain.\"]', '[\"Tôi nhìn một ngọn núi.\"]', 'noun', %Q)",
        kCatIcon, kDogIcon, kFishIcon, kMountainIcon);
    ExecuteSql(sql);
    sqlite3_free(sql);
}

}  // namespace wordlock
