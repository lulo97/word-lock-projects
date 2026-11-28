#include "app.h"

namespace wordlock {

void SeedTestData() {
    if (!IsTestDataMode()) {
        return;
    }

    ExecuteSql(
        "INSERT OR IGNORE INTO english_word "
        "(id, word, meaning_vi, examples, examples_vi, part_of_speech) VALUES "
        "(1, 'alpha', 'chữ cái đầu', '[\"alpha example\"]', '[\"ví dụ alpha\"]', 'noun'), "
        "(2, 'bravo', 'giỏi', '[\"bravo example\"]', '[\"ví dụ bravo\"]', 'adjective'), "
        "(3, 'charlie', 'tên riêng', '[\"charlie example\"]', '[\"ví dụ charlie\"]', 'noun'), "
        "(4, 'delta', 'sự thay đổi', '[\"delta example\"]', '[\"ví dụ delta\"]', 'noun')");

    ExecuteSql(
        "INSERT OR IGNORE INTO japanese_word "
        "(id, kanji, hiragana, katakana, romaji, meaning_en, examples, examples_en, "
        "examples_vi, part_of_speech) "
        "VALUES "
        "(1, '猫', 'ねこ', 'ネコ', 'neko', 'cat', '[\"猫がいます。\"]', "
        "'[\"There is a cat.\"]', '[\"Có một con mèo.\"]', 'noun'), "
        "(2, '犬', 'いぬ', 'イヌ', 'inu', 'dog', '[\"犬が走ります。\"]', "
        "'[\"The dog runs.\"]', '[\"Con chó chạy.\"]', 'noun'), "
        "(3, '魚', 'さかな', 'サカナ', 'sakana', 'fish', '[\"魚を食べます。\"]', "
        "'[\"I eat fish.\"]', '[\"Tôi ăn cá.\"]', 'noun'), "
        "(4, '山', 'やま', 'ヤマ', 'yama', 'mountain', '[\"山を見ます。\"]', "
        "'[\"I see a mountain.\"]', '[\"Tôi nhìn một ngọn núi.\"]', 'noun')");
}

}  // namespace wordlock
