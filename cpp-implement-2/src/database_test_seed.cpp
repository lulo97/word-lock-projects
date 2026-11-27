#include "app.h"

namespace wordlock {

void SeedTestData() {
    if (!IsTestDataMode()) {
        return;
    }

    ExecuteSql(
        "INSERT OR IGNORE INTO english_word "
        "(id, word, meaning_vi, examples, examples_vi) VALUES "
        "(1, 'alpha', 'chữ cái đầu', '[\"alpha example\"]', '[\"ví dụ alpha\"]'), "
        "(2, 'bravo', 'giỏi', '[\"bravo example\"]', '[\"ví dụ bravo\"]'), "
        "(3, 'charlie', 'tên riêng', '[\"charlie example\"]', '[\"ví dụ charlie\"]'), "
        "(4, 'delta', 'sự thay đổi', '[\"delta example\"]', '[\"ví dụ delta\"]')");

    ExecuteSql(
        "INSERT OR IGNORE INTO japanese_word "
        "(id, kanji, hiragana, katakana, romaji, meaning_en, examples, examples_en, examples_vi) "
        "VALUES "
        "(1, '猫', 'ねこ', 'ネコ', 'neko', 'cat', '[\"猫がいます。\"]', "
        "'[\"There is a cat.\"]', '[\"Có một con mèo.\"]'), "
        "(2, '犬', 'いぬ', 'イヌ', 'inu', 'dog', '[\"犬が走ります。\"]', "
        "'[\"The dog runs.\"]', '[\"Con chó chạy.\"]'), "
        "(3, '魚', 'さかな', 'サカナ', 'sakana', 'fish', '[\"魚を食べます。\"]', "
        "'[\"I eat fish.\"]', '[\"Tôi ăn cá.\"]'), "
        "(4, '山', 'やま', 'ヤマ', 'yama', 'mountain', '[\"山を見ます。\"]', "
        "'[\"I see a mountain.\"]', '[\"Tôi nhìn một ngọn núi.\"]')");
}

}  // namespace wordlock
