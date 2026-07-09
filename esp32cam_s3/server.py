"""
Attendance Server - nhận ảnh + UID từ ESP32-S3-CAM, lưu vào SQLite,
kèm giao diện web quản lý danh tính và xem lịch sử điểm danh.
"""
import os
from datetime import datetime
from flask import Flask, request, jsonify, send_from_directory, render_template, redirect, url_for

# Import các hàm từ database
from database import (
    init_db,
    insert_attendance,
    upsert_person,
    get_all_attendance,
    get_all_persons,
    get_person_name
)

# ================== CẤU HÌNH ==================
UPLOAD_FOLDER = "uploads"
HOST = "0.0.0.0"
PORT = 5000

app = Flask(__name__)
os.makedirs(UPLOAD_FOLDER, exist_ok=True)


# ================== ROUTES: API CHO ESP32 ==================
@app.route("/upload", methods=["POST"])
def upload():
    uid = request.args.get("uid", "").strip()
    if not uid:
        return jsonify({"status": "error", "message": "Missing 'uid' parameter"}), 400

    image_data = request.get_data()
    if not image_data:
        return jsonify({"status": "error", "message": "Empty image data"}), 400

    safe_uid = "".join(c for c in uid if c.isalnum() or c in ("-", "_"))
    if not safe_uid:
        return jsonify({"status": "error", "message": "Invalid 'uid' value"}), 400

    timestamp_str = datetime.now().strftime("%Y%m%d_%H%M%S")
    filename = f"{safe_uid}_{timestamp_str}.jpg"
    filepath = os.path.join(UPLOAD_FOLDER, filename)

    with open(filepath, "wb") as f:
        f.write(image_data)

    record_id = insert_attendance(safe_uid, filename)
    name = get_person_name(safe_uid)

    print(f"[UPLOAD] uid={safe_uid} name={name or '(chưa đăng ký)'} size={len(image_data)} bytes -> {filename} (id={record_id})")

    return jsonify({
        "status": "ok",
        "id": record_id,
        "uid": safe_uid,
        "name": name,
        "image": filename,
    }), 200


@app.route("/attendance", methods=["GET"])
def attendance_json():
    uid_filter = request.args.get("uid")
    records = get_all_attendance()
    if uid_filter:
        records = [r for r in records if r["uid"] == uid_filter]
    return jsonify(records)


@app.route("/images/<path:filename>", methods=["GET"])
def get_image(filename):
    return send_from_directory(UPLOAD_FOLDER, filename)


# ================== ROUTES: GIAO DIỆN WEB ==================
@app.route("/", methods=["GET"])
def dashboard():
    records = get_all_attendance()
    return render_template("dashboard.html", records=records, active="dashboard")


@app.route("/register", methods=["GET", "POST"])
def register():
    if request.method == "POST":
        uid = request.form.get("uid", "").strip()
        name = request.form.get("name", "").strip()
        if uid and name:
            safe_uid = "".join(c for c in uid if c.isalnum() or c in ("-", "_"))
            upsert_person(safe_uid, name)
        return redirect(url_for("register"))

    persons = get_all_persons()

    # Lấy các UID đã quẹt nhưng chưa đăng ký (gợi ý)
    import sqlite3
    conn = sqlite3.connect("attendance.db")
    conn.row_factory = sqlite3.Row
    unregistered = conn.execute("""
        SELECT DISTINCT a.uid
        FROM attendance a
        LEFT JOIN persons p ON a.uid = p.uid
        WHERE p.uid IS NULL
        ORDER BY a.id DESC
        LIMIT 20
    """).fetchall()
    conn.close()
    unregistered_uids = [row["uid"] for row in unregistered]

    return render_template("register.html",
                           persons=persons,
                           unregistered_uids=unregistered_uids,
                           active="register")


# ================== KHỞI ĐỘNG SERVER ==================
if __name__ == "__main__":
    init_db()
    print(f"[SYSTEM] Attendance Server starting on http://{HOST}:{PORT}")
    print(f"[SYSTEM] Dashboard: http://<IP_MAY_NAY>:{PORT}/")
    print(f"[SYSTEM] Đăng ký thẻ: http://<IP_MAY_NAY>:{PORT}/register")
    app.run(host=HOST, port=PORT, debug=False)