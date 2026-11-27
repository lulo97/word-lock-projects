use rusqlite::{params_from_iter, types::ValueRef, Connection};
use serde_json::{json, Map, Value};
use std::fs;
use std::path::PathBuf;

pub struct Database {
    conn: Connection,
}

fn base64(bytes: &[u8]) -> String {
    const TABLE: &[u8; 64] = b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    let mut out = String::with_capacity((bytes.len() + 2) / 3 * 4);

    for chunk in bytes.chunks(3) {
        let a = chunk[0];
        let b = *chunk.get(1).unwrap_or(&0);
        let c = *chunk.get(2).unwrap_or(&0);
        out.push(TABLE[(a >> 2) as usize] as char);
        out.push(TABLE[(((a & 0x03) << 4) | (b >> 4)) as usize] as char);
        if chunk.len() > 1 {
            out.push(TABLE[(((b & 0x0f) << 2) | (c >> 6)) as usize] as char);
        } else {
            out.push('=');
        }
        if chunk.len() > 2 {
            out.push(TABLE[(c & 0x3f) as usize] as char);
        } else {
            out.push('=');
        }
    }

    out
}

fn json_value(v: ValueRef<'_>) -> Value {
    match v {
        ValueRef::Null => Value::Null,
        ValueRef::Integer(i) => json!(i),
        ValueRef::Real(f) => json!(f),
        ValueRef::Text(t) => json!(String::from_utf8_lossy(t)),
        ValueRef::Blob(b) => Value::String(base64(b)),
    }
}

fn make_row(
    columns: &[String],
    row: &rusqlite::Row<'_>,
    json_cols: &[&str],
) -> Result<Value, String> {
    let mut map = Map::new();
    for (i, name) in columns.iter().enumerate() {
        let value = json_value(row.get_ref(i).map_err(|e| e.to_string())?);
        let converted = if json_cols.contains(&name.as_str()) && value.is_string() {
            // SQLite LOWER() is ASCII-only, and malformed JSON in an existing
            // database should behave like the Electron code's empty list.
            serde_json::from_str(value.as_str().unwrap_or("[]")).unwrap_or_else(|_| json!([]))
        } else {
            value
        };
        map.insert(name.clone(), converted);
    }
    Ok(Value::Object(map))
}

fn ignored(params: &Value, key: &str) -> Vec<String> {
    params
        .get(key)
        .and_then(Value::as_array)
        .map(|items| {
            items
                .iter()
                .filter_map(Value::as_str)
                .map(|s| s.to_ascii_lowercase())
                .filter(|s| !s.is_empty())
                .collect()
        })
        .unwrap_or_default()
}

fn placeholders(n: usize) -> String {
    (0..n).map(|_| "?").collect::<Vec<_>>().join(", ")
}

impl Database {
    pub fn open(test: bool) -> Result<Self, String> {
        let conn = if test {
            Connection::open_in_memory()
        } else {
            let home =
                std::env::var("USERPROFILE").unwrap_or_else(|_| "C:\\Users\\User".to_string());
            let dir = PathBuf::from(home).join("pomodoro_lock");
            fs::create_dir_all(&dir).map_err(|e| e.to_string())?;
            Connection::open(dir.join("pomodoro_lock.db"))
        }
        .map_err(|e| e.to_string())?;

        let db = Self { conn };
        db.create_schema()?;
        db.seed()?;
        Ok(db)
    }

    fn create_schema(&self) -> Result<(), String> {
        self.conn
            .execute_batch(
                r#"
            CREATE TABLE IF NOT EXISTS english_word (
              id INTEGER PRIMARY KEY,
              word TEXT NOT NULL,
              meaning_vi TEXT NOT NULL,
              examples TEXT NOT NULL,
              examples_vi TEXT NOT NULL DEFAULT '[]'
            );
            CREATE TABLE IF NOT EXISTS mcq (
              id INTEGER PRIMARY KEY,
              question TEXT NOT NULL,
              answers TEXT NOT NULL,
              correct_answer_idx INTEGER NOT NULL,
              explanation TEXT NOT NULL
            );
            CREATE TABLE IF NOT EXISTS japanese_word (
              id INTEGER PRIMARY KEY,
              kanji TEXT NOT NULL,
              hiragana TEXT NOT NULL,
              katakana TEXT NOT NULL,
              romaji TEXT NOT NULL DEFAULT '',
              meaning_en TEXT NOT NULL,
              examples TEXT NOT NULL,
              examples_en TEXT NOT NULL DEFAULT '[]',
              examples_vi TEXT NOT NULL DEFAULT '[]'
            );
            "#,
            )
            .map_err(|e| e.to_string())?;

        for (table, column, definition) in [
            ("english_word", "examples_vi", "TEXT NOT NULL DEFAULT '[]'"),
            ("japanese_word", "romaji", "TEXT NOT NULL DEFAULT ''"),
            ("japanese_word", "examples_en", "TEXT NOT NULL DEFAULT '[]'"),
            ("japanese_word", "examples_vi", "TEXT NOT NULL DEFAULT '[]'"),
        ] {
            let mut stmt = self
                .conn
                .prepare(&format!("PRAGMA table_info({})", table))
                .map_err(|e| e.to_string())?;
            let names: Vec<String> = stmt
                .query_map([], |row| row.get::<_, String>(1))
                .map_err(|e| e.to_string())?
                .filter_map(Result::ok)
                .collect();
            drop(stmt);
            if !names.iter().any(|n| n == column) {
                self.conn
                    .execute(
                        &format!("ALTER TABLE {} ADD COLUMN {} {}", table, column, definition),
                        [],
                    )
                    .map_err(|e| e.to_string())?;
            }
        }
        Ok(())
    }

    fn seed(&self) -> Result<(), String> {
        self.conn.execute_batch(
            r#"
            INSERT INTO english_word (id, word, meaning_vi, examples, examples_vi)
            VALUES (0, 'apple', 'quả táo',
              '["he has an apple","apple is a popular company","an apple a day keep the doctor away"]',
              '["anh ấy có một quả táo","Apple là một công ty nổi tiếng","một quả táo mỗi ngày giúp không cần gặp bác sĩ"]')
            ON CONFLICT(id) DO UPDATE SET
              word = excluded.word, meaning_vi = excluded.meaning_vi,
              examples = excluded.examples, examples_vi = excluded.examples_vi;

            INSERT OR IGNORE INTO mcq (id, question, answers, correct_answer_idx, explanation)
            VALUES (0, '1+1=?', '["1","2","3","4"]', 1, '1+1=2');

            INSERT INTO japanese_word
              (id, kanji, hiragana, katakana, romaji, meaning_en, examples, examples_en, examples_vi)
            VALUES (0, '桜', 'さくら', 'サクラ', 'sakura', 'cherry blossom',
              '["桜が咲きました。","春に桜を見ます。"]',
              '["The cherry blossoms bloomed.","I see cherry blossoms in spring."]',
              '["Hoa anh đào đã nở.","Tôi nhìn hoa anh đào vào mùa xuân."]')
            ON CONFLICT(id) DO UPDATE SET
              kanji = excluded.kanji, hiragana = excluded.hiragana,
              katakana = excluded.katakana, romaji = excluded.romaji,
              meaning_en = excluded.meaning_en, examples = excluded.examples,
              examples_en = excluded.examples_en, examples_vi = excluded.examples_vi;
            "#,
        )
        .map_err(|e| e.to_string())
    }

    pub fn call(&self, method: &str, params: &Value) -> Result<Value, String> {
        let id = params.get("id").and_then(Value::as_i64).unwrap_or(-1);

        let (sql, values, json_cols): (String, Vec<Box<dyn rusqlite::ToSql>>, Vec<&str>) =
            match method {
                "db.englishWord" => (
                    "SELECT * FROM english_word WHERE id = ?".into(),
                    vec![Box::new(id)],
                    vec!["examples", "examples_vi"],
                ),
                "db.mcq" => (
                    "SELECT * FROM mcq WHERE id = ?".into(),
                    vec![Box::new(id)],
                    vec!["answers"],
                ),
                "db.japaneseWord" => (
                    "SELECT * FROM japanese_word WHERE id = ?".into(),
                    vec![Box::new(id)],
                    vec!["examples", "examples_en", "examples_vi"],
                ),
                "db.randomMcq" => (
                    "SELECT * FROM mcq ORDER BY RANDOM() LIMIT 1".into(),
                    vec![],
                    vec!["answers"],
                ),
                "db.randomEnglishWord" => {
                    let ignore = ignored(params, "ignore");
                    if ignore.is_empty() {
                        (
                            "SELECT * FROM english_word ORDER BY RANDOM() LIMIT 1".into(),
                            vec![],
                            vec!["examples", "examples_vi"],
                        )
                    } else {
                        let sql = format!(
                        "SELECT * FROM english_word WHERE LOWER(word) NOT IN ({}) ORDER BY RANDOM() LIMIT 1",
                        placeholders(ignore.len())
                    );
                        let values = ignore
                            .iter()
                            .map(|w| Box::new(w.clone()) as Box<dyn rusqlite::ToSql>)
                            .collect();
                        (sql, values, vec!["examples", "examples_vi"])
                    }
                }
                "db.randomJapaneseWord" => {
                    let ignore = ignored(params, "ignore");
                    if ignore.is_empty() {
                        (
                            "SELECT * FROM japanese_word ORDER BY RANDOM() LIMIT 1".into(),
                            vec![],
                            vec!["examples", "examples_en", "examples_vi"],
                        )
                    } else {
                        let conditions = [
                            "LOWER(kanji) NOT IN (PLACEHOLDERS)",
                            "LOWER(hiragana) NOT IN (PLACEHOLDERS)",
                            "LOWER(katakana) NOT IN (PLACEHOLDERS)",
                            "LOWER(romaji) NOT IN (PLACEHOLDERS)",
                        ]
                        .join(" AND ");
                        let ph = placeholders(ignore.len());
                        let sql = format!(
                            "SELECT * FROM japanese_word WHERE {} ORDER BY RANDOM() LIMIT 1",
                            conditions.replace("PLACEHOLDERS", &ph)
                        );
                        let mut values: Vec<Box<dyn rusqlite::ToSql>> = Vec::new();
                        for _ in 0..4 {
                            for word in &ignore {
                                values.push(Box::new(word.clone()));
                            }
                        }
                        (sql, values, vec!["examples", "examples_en", "examples_vi"])
                    }
                }
                "db.exec" => {
                    let sql = params.get("sql").and_then(Value::as_str).unwrap_or("");
                    self.conn.execute_batch(sql).map_err(|e| e.to_string())?;
                    return Ok(json!({ "rows": 0 }));
                }
                _ => return Err(format!("unknown db method: {}", method)),
            };

        let refs: Vec<&dyn rusqlite::ToSql> = values.iter().map(|v| v.as_ref()).collect();
        let mut stmt = self.conn.prepare(&sql).map_err(|e| e.to_string())?;
        // Capture column names before query() mutably borrows stmt for rows.
        let columns: Vec<String> = (0..stmt.column_count())
            .map(|i| stmt.column_name(i).unwrap_or("").to_string())
            .collect();
        let mut rows = stmt
            .query(params_from_iter(refs))
            .map_err(|e| e.to_string())?;
        let row = match rows.next().map_err(|e| e.to_string())? {
            Some(row) => make_row(&columns, row, &json_cols)?,
            None => Value::Null,
        };
        Ok(json!({ "row": row }))
    }
}
