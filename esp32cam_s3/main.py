# import cv2
# import numpy as np
# import requests
# from flask import Flask, Response, render_template, request

# app = Flask(__name__)

# # ================= CẤU HÌNH =================
# ESP32_IP = "192.168.4.1"

# ESP32_STREAM_URL = f"http://{ESP32_IP}/stream"
# ESP32_CONTROL_URL = f"http://{ESP32_IP}/control"

# TARGET_WIDTH = 1280
# TARGET_HEIGHT = 720
# # ===========================================


# def stream_from_esp32():

#     try:
#         response = requests.get(
#             ESP32_STREAM_URL,
#             stream=True,
#             timeout=(5, None)
#         )
#         print(response.status_code)
#         print(response.headers)
#         print("Connected:", response.status_code)

#         if response.status_code != 200:
#             return

#     except Exception as e:
#         print("Cannot connect to ESP32:", e)
#         return

#     buffer = b""

#     for chunk in response.iter_content(chunk_size=4096):

#         if not chunk:
#             continue

#         buffer += chunk

#         while True:

#             start = buffer.find(b"\xff\xd8")
#             end = buffer.find(b"\xff\xd9")

#             if start == -1 or end == -1:
#                 break

#             jpg = buffer[start:end + 2]
#             buffer = buffer[end + 2:]

#             frame = cv2.imdecode(
#                 np.frombuffer(jpg, dtype=np.uint8),
#                 cv2.IMREAD_COLOR
#             )

#             if frame is None:
#                 continue

#             # Upscale
#             frame = cv2.resize(
#                 frame,
#                 (TARGET_WIDTH, TARGET_HEIGHT),
#                 interpolation=cv2.INTER_LANCZOS4
#             )

#             # Sharpen
#             blur = cv2.GaussianBlur(frame, (0, 0), 3)
#             frame = cv2.addWeighted(frame, 1.5, blur, -0.5, 0)

#             success, jpeg = cv2.imencode(
#                 ".jpg",
#                 frame,
#                 [cv2.IMWRITE_JPEG_QUALITY, 85]
#             )

#             if not success:
#                 continue

#             yield (
#                 b'--frame\r\n'
#                 b'Content-Type: image/jpeg\r\n\r\n'
#                 + jpeg.tobytes() +
#                 b'\r\n'
#             )


# @app.route("/")
# def index():
#     return render_template("index.html")


# @app.route("/video_feed")
# def video_feed():

#     r = requests.get(
#         ESP32_STREAM_URL,
#         stream=True
#     )

#     return Response(
#         r.iter_content(chunk_size=4096),
#         content_type=r.headers["Content-Type"]
#     )


# @app.route("/move", methods=["POST"])
# def move():

#     direction = request.form.get("direction")

#     print("Command:", direction)

#     try:

#         r = requests.post(
#             ESP32_CONTROL_URL,
#             data=direction,
#             timeout=2
#         )

#         return r.text

#     except Exception as e:

#         print(e)

#         return "ESP32 Error", 500


# if __name__ == "__main__":

#     app.run(
#         host="0.0.0.0",
#         port=5000,
#         threaded=True,
#         debug=True
#     )
"""
Kiểm tra nhanh 1 file JPEG có hợp lệ không (magic bytes SOI/EOI).
Dùng: python check_jpeg.py uploads/ten_file.jpg
"""
import sys

def check_jpeg(path):
    with open(path, "rb") as f:
        data = f.read()

    size = len(data)
    print(f"Kích thước file: {size} bytes")

    if size < 4:
        print("❌ File quá nhỏ, không thể là JPEG hợp lệ")
        return

    header = data[0:2]
    footer = data[-2:]

    print(f"2 byte đầu (phải là FFD8): {header.hex().upper()}")
    print(f"2 byte cuối (phải là FFD9): {footer.hex().upper()}")

    if header == b'\xff\xd8' and footer == b'\xff\xd9':
        print("✅ File có cấu trúc JPEG hợp lệ (SOI/EOI đúng)")
    else:
        print("❌ File KHÔNG có cấu trúc JPEG hợp lệ -> dữ liệu bị hỏng/thiếu")

    # Thử mở bằng PIL nếu có cài Pillow
    try:
        from PIL import Image
        img = Image.open(path)
        img.verify()
        print(f"✅ PIL mở và verify thành công. Kích thước ảnh: {img.size}")
    except ImportError:
        print("(Chưa cài Pillow, bỏ qua bước verify bằng PIL. Cài bằng: pip install Pillow)")
    except Exception as e:
        print(f"❌ PIL báo lỗi khi mở ảnh: {e}")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Dùng: python check_jpeg.py <đường_dẫn_file.jpg>")
        sys.exit(1)
    check_jpeg(sys.argv[1])