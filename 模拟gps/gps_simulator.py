import asyncio
import serial

from winrt.windows.devices.geolocation import (
    Geolocator,
    GeolocationAccessStatus,
    PositionAccuracy
)

SERIAL_PORT = "COM4"      
BAUD_RATE = 115200

SEND_INTERVAL = 2.0      

latest_position = None

async def location_task(locator):

    global latest_position

    while True:

        try:
            position = await locator.get_geoposition_async()

            latitude = position.coordinate.latitude
            longitude = position.coordinate.longitude
            accuracy = position.coordinate.accuracy

            latest_position = (
                latitude,
                longitude,
                accuracy
            )

            print(
                f"[LOC] "
                f"lat={latitude:.6f}, "
                f"lon={longitude:.6f}, "
                f"accuracy={accuracy:.1f}m"
            )

        except Exception as e:
            print("[定位失败]", e)

        await asyncio.sleep(1) #每秒定位一次


# ==============================
# 固定周期串口发送
# ==============================

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

        # 固定发送周期
        await asyncio.sleep(SEND_INTERVAL)


# ==============================
# 主程序
# ==============================

async def main():
    print("正在请求 Windows 定位权限...")
    status = await Geolocator.request_access_async()
    if status != GeolocationAccessStatus.ALLOWED:
        print("没有获得 Windows 定位权限")
        return

    print("定位权限获取成功")

    locator = Geolocator()
    locator.desired_accuracy = PositionAccuracy.HIGH


    try:

        ser = serial.Serial(
            port=SERIAL_PORT,
            baudrate=BAUD_RATE,
            timeout=1
        )

    except Exception as e:

        print("串口打开失败：", e)
        return

    print(
        f"串口打开成功: "
        f"{SERIAL_PORT}, "
        f"{BAUD_RATE}"
    )

    print(
        f"发送周期: "
        f"{SEND_INTERVAL} 秒"
    )

    try:

        await asyncio.gather(

            # 持续更新定位
            location_task(locator),

            # 固定周期发送
            serial_task(ser)

        )

    finally:

        ser.close()


# ==============================
# 程序入口
# ==============================

if __name__ == "__main__":

    try:
        asyncio.run(main())

    except KeyboardInterrupt:
        print("\n程序退出")