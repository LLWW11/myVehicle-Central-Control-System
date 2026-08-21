import asyncio
import serial
import math

# ===== 模拟参数 =====
START_LAT = 39.9          # 起始纬度
START_LON = 116.4         # 起始经度
SPEED_KPH = 60.0          # 速度km/h
UPDATE_INTERVAL = 1.0     # 位置更新间隔
SEND_INTERVAL = 1.0       # 串口发送间隔

# ===== 串口参数 =====
SERIAL_PORT = "COM4"
BAUD_RATE = 115200

# ===== 全局变量 =====
latest_position = (START_LAT, START_LON, 5.0)  # (lat, lon, accuracy)

# ============================================================
# 模拟位置更新任务（每秒更新一次）
# ============================================================
async def position_update_task():
    global latest_position

    lat = START_LAT
    lon = START_LON
    accuracy = 5.0  # 固定精度（米）

    # 速度转换为米/秒
    speed_mps = SPEED_KPH * 1000 / 3600

    # 计算经度增量（度/秒）
    # 纬度不变，经度1度 ≈ 111320 * cos(lat) 米
    delta_lon_per_sec = speed_mps / (111320 * math.cos(math.radians(lat)))

    print(f"[SIM] 模拟开始：lat={lat:.6f}, lon={lon:.6f}, 速度={SPEED_KPH} km/h 向东")

    while True:
        # 更新经度（向东增加）
        lon += delta_lon_per_sec * UPDATE_INTERVAL

        # 更新全局位置
        latest_position = (lat, lon, accuracy)

        print(f"[POS] lat={lat:.6f}, lon={lon:.6f}, accuracy={accuracy:.1f}m")

        await asyncio.sleep(UPDATE_INTERVAL)

# ============================================================
# 串口发送任务（固定周期发送）
# ============================================================
async def serial_task(ser):
    while True:
        if latest_position is not None:
            latitude, longitude, accuracy = latest_position
            data = (
                f"$LOC,"
                f"{latitude:.6f},"
                f"{longitude:.6f},"
                f"{accuracy:.1f}"
                f"\r\n"
            )
            try:
                ser.write(data.encode("ascii"))
                ser.flush()
                print("[TX]", data.strip())
            except Exception as e:
                print("[串口发送失败]", e)
        else:
            print("[TX] 等待定位数据...")

        await asyncio.sleep(SEND_INTERVAL)

# ============================================================
# 主程序
# ============================================================
async def main():
    # 打开串口
    try:
        ser = serial.Serial(
            port=SERIAL_PORT,
            baudrate=BAUD_RATE,
            timeout=1
        )
    except Exception as e:
        print("串口打开失败：", e)
        return

    print(f"串口打开成功: {SERIAL_PORT}, {BAUD_RATE}")
    print(f"发送周期: {SEND_INTERVAL} 秒")

    try:
        # 并发执行位置更新和串口发送
        await asyncio.gather(
            position_update_task(),
            serial_task(ser)
        )
    finally:
        ser.close()

# ============================================================
# 程序入口
# ============================================================
if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n程序退出")