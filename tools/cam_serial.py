#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
把图像打成 128x64 1bpp 帧，通过串口发给 STM32F103 显示在 SSD1306 上。

三种用法：
    # 1) 先验证串口链路，根本不需要摄像头（只要 pyserial）
    python tools/cam_serial.py --port COM3 --test

    # 2) 接真实摄像头（需要 opencv-python）
    #    注意 --camera 的编号：0 未必是你要的那颗，先用 --list-cameras 确认
    python tools/cam_serial.py --port COM3 --camera 1

    # 3) 不知道串口号？先列出来
    python tools/cam_serial.py --list

依赖：
    pip install pyserial            # 任何模式都要
    pip install opencv-python numpy # 摄像头/预览模式需要

按 q 或 Ctrl+C 退出。
"""

import argparse
import sys
import time

import serial
import serial.tools.list_ports

WIDTH = 128
HEIGHT = 64
PAYLOAD = 1024          # 8 页 x 128 列
SYNC1 = 0xAA
SYNC2 = 0x55
CMD_BITMAP = 0x01

WIN_NAME = "sending to OLED"   # --preview 的窗口名（上面带 threshold 滑动条）

try:
    import numpy as np
except ImportError:
    sys.exit("缺少 numpy，先执行： pip install numpy")

# Bayer 8x8 有序抖动矩阵（归一化到 0..1）
BAYER8 = np.array([
    [0, 32,  8, 40,  2, 34, 10, 42],
    [48, 16, 56, 24, 50, 18, 58, 26],
    [12, 44,  4, 36, 14, 46,  6, 38],
    [60, 28, 52, 20, 62, 30, 54, 22],
    [3, 35, 11, 43,  1, 33,  9, 41],
    [51, 19, 59, 27, 49, 17, 57, 25],
    [15, 47,  7, 39, 13, 45,  5, 37],
    [63, 31, 55, 23, 61, 29, 53, 21],
], dtype=np.float32) / 64.0


# ---------------------------------------------------------------- 帧打包
def build_frame(payload: bytes) -> bytes:
    """按固件协议打包：[AA 55][len_lo len_hi][cmd][payload...][checksum]"""
    body = len(payload).to_bytes(2, "little") + bytes([CMD_BITMAP]) + payload
    checksum = sum(body) & 0xFF
    return bytes([SYNC1, SYNC2]) + body + bytes([checksum])


def pack_bitmap(bw) -> bytes:
    """
    bw: numpy 数组，形状 (64, 128)，取值 0/1
    返回 1024 字节，格式与 SSD1306 显存一致：
      8 页 x 128 列，每字节 bit0 = 该页最上一行，bit7 = 最下一行
    """
    # axis=0 把 64 行压成 8 字节；bitorder='little' 让 bit0 对应 y 最小的那行
    packed = np.packbits(bw.astype(np.uint8), axis=0, bitorder="little")
    return packed.tobytes()


# ---------------------------------------------------------------- 二值化
def autocontrast(gray):
    """
    把灰阶拉伸到满量程：暗环境/逆光时能救回细节。
    必须限制增益——否则遇到低对比度画面（白墙、天花板）会把噪声放大，
    整幅图被推到全白或全黑，屏幕上就"一动不动"。
    """
    lo, hi = np.percentile(gray, (2.0, 98.0))
    span = float(hi) - float(lo)

    if span < 8.0:
        return gray                       # 本来就没对比度，别硬拉

    max_gain = 3.0
    gain = 255.0 / span
    if gain > max_gain:                   # 限制放大倍数
        gain = max_gain
        mid = (float(lo) + float(hi)) / 2.0
        half = 255.0 / (2.0 * max_gain)
        lo = mid - half
        hi = mid + half

    out = (gray.astype(np.float32) - float(lo)) * gain
    return np.clip(out, 0, 255).astype(np.uint8)


def to_binary(gray, mode="otsu", threshold=128, offset=0):
    """gray: (64,128) uint8 -> (64,128) 0/1；offset 由自动曝光给出，见 main()"""
    if offset:
        gray = np.clip(gray.astype(np.int16) + offset, 0, 255).astype(np.uint8)

    if mode == "edge":
        # 单色屏上最容易辨认的是"轮廓线"，而不是灰阶块
        import cv2
        g = cv2.GaussianBlur(autocontrast(gray), (3, 3), 0)
        e = cv2.Canny(g, 50, 150)
        return (e > 0).astype(np.uint8)

    if mode == "thresh":
        return (autocontrast(gray) > threshold).astype(np.uint8)

    if mode == "otsu":
        # 对光照变化比较鲁棒，室内首选
        import cv2  # 只有用到摄像头相关功能时才需要 opencv
        _, bw = cv2.threshold(autocontrast(gray), 0, 1,
                              cv2.THRESH_BINARY + cv2.THRESH_OTSU)
        return bw.astype(np.uint8)

    if mode == "dither":
        # Bayer 8x8 有序抖动：单色屏上能保留灰阶层次，
        # 而且完全可以向量化，比逐像素的 Floyd-Steinberg 快两个数量级
        h, w = gray.shape
        yy, xx = np.mgrid[0:h, 0:w]
        thr = BAYER8[yy % 8, xx % 8] * 255.0
        return (autocontrast(gray).astype(np.float32) > thr).astype(np.uint8)

    raise ValueError("unknown mode: %s" % mode)


# ---------------------------------------------------------------- 图像来源
def gen_test_frame(frame_idx, pattern="checker"):
    """不依赖摄像头的测试图案，用来确认链路/方向/刷新都对"""
    if pattern == "flash":
        # 每 10 帧整屏黑/白翻转一次：用来一眼确认屏幕在实时更新
        v = 1 if (frame_idx // 10) % 2 else 0
        return np.full((HEIGHT, WIDTH), v, dtype=np.uint8)

    yy, xx = np.mgrid[0:HEIGHT, 0:WIDTH]
    off = frame_idx % 16
    checker = (((xx + off) // 8) + ((yy + off) // 8)) % 2
    bw = checker.astype(np.uint8)
    bw[0, :] = 1            # 外边框，方便确认没有偏移/翻转
    bw[-1, :] = 1
    bw[:, 0] = 1
    bw[:, -1] = 1
    return bw


def to_gray(frame, zoom=1.0):
    """
    原始帧 -> (64,128) 灰度。
    zoom < 1 时只取画面中心的一小块再放大，等于数字变焦，
    拍人脸/小物件时能让主体占满屏幕，辨认度提升明显。
    """
    import cv2
    h, w = frame.shape[:2]
    side = int(min(h, w) * zoom)
    if side < 16:
        side = 16
    cy, cx = h // 2, w // 2
    crop = frame[cy - side // 2:cy + side // 2, cx - side // 2:cx + side // 2]
    small = cv2.resize(crop, (WIDTH, HEIGHT), interpolation=cv2.INTER_AREA)
    return cv2.cvtColor(small, cv2.COLOR_BGR2GRAY)


def grab_camera(cap, mirror):
    ok, frame = cap.read()
    if not ok:
        return None
    if mirror:
        import cv2
        frame = cv2.flip(frame, 1)
    return to_gray(frame)


# Haar 级联分类器，第一次用到时才加载
_CASCADE = None


def draw_face_page(frame):
    """
    不做"传画面"，而是传**识别结果**：
    检测到人脸就画笑脸，没检测到就画平静脸。
    128x64 单色屏上，这比原始画面有意义得多。
    """
    import os

    import cv2

    global _CASCADE
    if _CASCADE is None:
        # 用 cv2.__file__ 拼路径而不用 cv2.data：后者在部分版本里没有类型存根
        path = os.path.join(os.path.dirname(cv2.__file__), "data",
                            "haarcascade_frontalface_default.xml")
        if not os.path.isfile(path):
            raise RuntimeError("找不到人脸模型文件：%s" % path)
        _CASCADE = cv2.CascadeClassifier(path)

    gray = cv2.equalizeHist(cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY))
    # 检测前先缩小：Haar 在 640x480 上跑太慢，会拖垮帧率
    det = cv2.resize(gray, (320, 240), interpolation=cv2.INTER_AREA)
    faces = _CASCADE.detectMultiScale(det, scaleFactor=1.3, minNeighbors=5,
                                      minSize=(30, 30))

    canvas = np.zeros((HEIGHT, WIDTH), np.uint8)
    cv2.circle(canvas, (64, 32), 30, 255, 1)          # 脸
    cv2.circle(canvas, (52, 24), 4, 255, 1)           # 左眼
    cv2.circle(canvas, (76, 24), 4, 255, 1)           # 右眼

    if len(faces) > 0:
        cv2.ellipse(canvas, (64, 40), (14, 9), 0, 20, 160, 255, 2)   # 笑
        cv2.circle(canvas, (40, 40), 3, 255, 1)       # 腮红
        cv2.circle(canvas, (88, 40), 3, 255, 1)
    else:
        cv2.line(canvas, (52, 42), (76, 42), 255, 2)  # 一条嘴 = 平静

    return (canvas > 0).astype(np.uint8)


# ---------------------------------------------------------------- 串口
def list_cameras(max_index=6):
    """枚举摄像头：机器上往往不止一个，别想当然用 --camera 0"""
    import cv2

    print("可用摄像头：")
    found = 0
    for i in range(max_index):
        cap = cv2.VideoCapture(i)
        try:
            if not cap.isOpened():
                continue
            ok, f = cap.read()
            if ok and f is not None:
                w = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
                h = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
                print("  --camera %d   %dx%d" % (i, w, h))
                found += 1
            else:
                print("  --camera %d   能打开但读不到帧" % i)
        finally:
            cap.release()

    if found == 0:
        print("  一个摄像头都没找到（插好了吗？被别的程序占着吗？）")
    else:
        print("  注意：index 0 未必是你要的那颗，不确定的话逐个试")


def list_ports():
    ports = sorted(serial.tools.list_ports.comports())
    if not ports:
        print("没找到任何串口设备（USB-TTL 插上了吗？驱动装了吗？）")
        return
    print("可用串口：")
    for p in ports:
        print("  %-8s %s" % (p.device, p.description))


def main():
    ap = argparse.ArgumentParser(description="摄像头 -> 串口 -> STM32 -> OLED")
    ap.add_argument("--port", help="串口号，例如 COM3 / /dev/ttyUSB0")
    ap.add_argument("--baud", type=int, default=921600, help="波特率，默认 921600")
    ap.add_argument("--camera", type=int, default=None,
                    help="摄像头编号，0=默认摄像头；不给则用测试图案")
    ap.add_argument("--test", action="store_true", help="强制用测试图案（不用摄像头）")
    ap.add_argument("--mode", default="otsu",
                    choices=["thresh", "otsu", "dither", "edge", "face"],
                    help="二值化方式，默认 otsu；edge=只画轮廓线；face=检测人脸后画表情")
    ap.add_argument("--brightness", type=float, default=None,
                    help="摄像头亮度 0~1（部分摄像头支持），画面太暗就调大")
    ap.add_argument("--zoom", type=float, default=1.0,
                    help="数字变焦：<1 只取画面中心再放大，如 0.5 让主体占满屏幕")
    ap.add_argument("--pattern", default="checker", choices=["checker", "flash"],
                    help="--test 用的图案：checker=滚动棋盘格，flash=整屏黑白闪烁")
    ap.add_argument("--ascii", action="store_true",
                    help="把要发的画面用 ASCII 打到终端（不用开窗口也能看，每 15 帧一次）")
    ap.add_argument("--threshold", type=int, default=128, help="thresh/dither 模式的阈值")
    ap.add_argument("--fps", type=float, default=20.0, help="发送帧率，默认 20")
    ap.add_argument("--frames", type=int, default=0,
                    help="只发这么多帧就自动退出，0 = 一直发（默认）")
    ap.add_argument("--mirror", action="store_true", help="左右镜像（自拍视角更自然）")
    ap.add_argument("--invert", action="store_true", help="黑白反转")
    ap.add_argument("--preview", action="store_true", help="PC 上开个窗口预览要发的图")
    ap.add_argument("--list", action="store_true", help="只列出可用串口然后退出")
    ap.add_argument("--list-cameras", action="store_true",
                    help="列出所有可用摄像头编号（机器上不止一个时用它确认）")
    args = ap.parse_args()

    if args.list:
        list_ports()
        return

    if args.list_cameras:
        list_cameras()
        return

    use_camera = (args.camera is not None) and (not args.test)
    cam_index = args.camera if args.camera is not None else 0
    if use_camera or args.preview:
        global cv2
        try:
            import cv2
        except ImportError:
            print("摄像头/预览模式需要 opencv： pip install opencv-python")
            return 1

    if not args.port:
        print("请用 --port 指定串口号，或先跑 --list 看看有哪些")
        list_ports()
        return 1

    try:
        ser = serial.Serial(args.port, args.baud, timeout=1)
    except Exception as e:
        print("打不开串口 %s：%s" % (args.port, e))
        list_ports()
        return 1

    cap = None
    if use_camera:
        cap = cv2.VideoCapture(cam_index)
        if not cap.isOpened():
            print("打不开摄像头 %d" % cam_index)
            ser.close()
            return 1
        # 让摄像头自己跑 640x480 以上，我们再缩放，画质比直接读小分辨率好
        cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
        cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
        if args.brightness is not None:
            cap.set(cv2.CAP_PROP_BRIGHTNESS, args.brightness)

    if args.preview:
        cv2.namedWindow(WIN_NAME)
        cv2.resizeWindow(WIN_NAME, WIDTH * 4, HEIGHT * 4)
        # 滑动条只对 --mode thresh 生效（otsu 是自动阈值，edge/dither 不受它影响）
        cv2.createTrackbar("threshold", WIN_NAME, args.threshold, 255,
                           lambda _v: None)

    print("发送中：%s @ %d  |  %s  |  %.1f fps  |  Ctrl+C 退出"
          % (args.port, args.baud,
             "camera %d" % cam_index if use_camera else "test pattern",
             args.fps))

    interval = 1.0 / max(args.fps, 0.1)
    idx = 0
    t0 = time.time()
    sent = 0
    g_offset = 0      # 自动曝光的亮度偏移，运行时动态调整

    try:
        while True:
            t_start = time.time()

            if use_camera:
                if cap is None:
                    break
                ok, frame = cap.read()
                if not ok:
                    # 摄像头被拔掉/被别的程序占用：等一下自动重连，别直接退出，
                    # 否则屏幕会静默定格，看起来像"画面是假的"
                    print("读不到摄像头画面，0.5s 后重连...（插回去会自动恢复）")
                    cap.release()
                    time.sleep(0.5)
                    cap = cv2.VideoCapture(cam_index)
                    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
                    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
                    continue
                if args.mirror:
                    frame = cv2.flip(frame, 1)
                if args.mode == "face":
                    bw = draw_face_page(frame)
                else:
                    th = (cv2.getTrackbarPos("threshold", WIN_NAME)
                          if args.preview else args.threshold)
                    bw = to_binary(to_gray(frame, args.zoom), args.mode, th,
                                   g_offset)

                    # 自动曝光：把白点比例稳在 25%~75%，避免整屏全白/全黑
                    # （全白或全黑时，挥手也看不出任何变化）
                    wr = float(bw.mean())
                    if wr > 0.75:
                        g_offset -= 4
                    elif wr < 0.25:
                        g_offset += 4
                    if g_offset > 100:
                        g_offset = 100
                    elif g_offset < -100:
                        g_offset = -100
            else:
                bw = gen_test_frame(idx, args.pattern)

            if args.invert:
                bw = 1 - bw

            frame = build_frame(pack_bitmap(bw))
            ser.write(frame)
            sent += 1
            idx += 1

            if args.ascii and (idx % 15 == 0):
                small = bw.reshape(HEIGHT // 2, 2, WIDTH // 2, 2).max(axis=(1, 3))
                print("--- frame %d  白点 %.0f%% ---"
                      % (idx, float(bw.mean()) * 100))
                for row in small:
                    print("".join("#" if v else "." for v in row))

            if args.preview:
                # 放大 4 倍看清楚；注意这是"发送出去的内容"
                big = cv2.resize(bw * 255, (WIDTH * 4, HEIGHT * 4),
                                 interpolation=cv2.INTER_NEAREST)
                cv2.imshow(WIN_NAME, big)
                if cv2.waitKey(1) & 0xFF == ord("q"):
                    break

            dt = time.time() - t_start
            if dt < interval:
                time.sleep(interval - dt)

            if idx % 60 == 0:
                el = time.time() - t0
                print("  已发 %d 帧，实际 %.1f fps" % (sent, sent / el), end="\r")

            if args.frames and sent >= args.frames:
                break

    except KeyboardInterrupt:
        print("\n退出")
    finally:
        if cap is not None:
            cap.release()
        if args.preview:
            try:
                cv2.destroyAllWindows()
            except Exception:
                pass
        ser.close()

    return 0


if __name__ == "__main__":
    sys.exit(main())
