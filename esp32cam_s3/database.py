import sqlite3
from datetime import datetime

DB_PATH = "attendance.db"

def init_db():
    conn = sqlite3.connect(DB_PATH)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS attendance (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            uid TEXT NOT NULL,
            timestamp TEXT NOT NULL,
            image_path TEXT NOT NULL
        )
    """)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS persons (
            uid TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            created_at TEXT NOT NULL
        )
    """)
    conn.commit()
    conn.close()


def insert_attendance(uid: str, image_path: str) -> int:
    conn = sqlite3.connect(DB_PATH)
    cursor = conn.execute(
        "INSERT INTO attendance (uid, timestamp, image_path) VALUES (?, ?, ?)",
        (uid, datetime.now().isoformat(timespec="seconds"), image_path),
    )
    conn.commit()
    record_id = cursor.lastrowid
    conn.close()
    return record_id


def upsert_person(uid: str, name: str):
    conn = sqlite3.connect(DB_PATH)
    conn.execute("""
        INSERT INTO persons (uid, name, created_at) VALUES (?, ?, ?)
        ON CONFLICT(uid) DO UPDATE SET name = excluded.name
    """, (uid, name, datetime.now().isoformat(timespec="seconds")))
    conn.commit()
    conn.close()


def get_all_attendance(limit: int = 200):
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    rows = conn.execute("""
        SELECT a.id, a.uid, a.timestamp, a.image_path,
               COALESCE(p.name, '(Chưa đăng ký)') AS name
        FROM attendance a
        LEFT JOIN persons p ON a.uid = p.uid
        ORDER BY a.id DESC
        LIMIT ?
    """, (limit,)).fetchall()
    conn.close()
    return [dict(row) for row in rows]


def get_all_persons():
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    rows = conn.execute("SELECT * FROM persons ORDER BY name ASC").fetchall()
    conn.close()
    return [dict(row) for row in rows]


def get_person_name(uid: str):
    conn = sqlite3.connect(DB_PATH)
    row = conn.execute("SELECT name FROM persons WHERE uid = ?", (uid,)).fetchone()
    conn.close()
    return row[0] if row else None