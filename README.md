# NRF Roll-Up Gate Controller

This project is a wireless control system based on NRF for opening a rolling gate / parking door. It includes two STM32-based modules: a `sender` unit and a `receiver` unit. The sender sends commands from a switch or remote interface, and the receiver processes those commands to operate the gate actuator.

## Project Structure

- `sender/` — transmitter module
- `receiver/` -- receiver unit
- `Schematic.pdf` — wiring and circuit diagrams
- `Remoot.ioc` — STM32CubeMX configuration

## Features

- NRF wireless communication
- STM32 microcontroller-based design
- Roller door / parking gate control
- Simple, compact embedded architecture

## Usage

1. Open the project in STM32CubeIDE.
2. Build the `sender` and `receiver` firmware.
3. Flash the boards according to the schematic.
4. Test the wireless command path between sender and receiver.

---

# کنترل درب رول آپ با NRF

این پروژه یک سیستم کنترل بی‌سیم مبتنی بر ماژول NRF برای باز کردن درب رول آپ / درب پارکینگ است. این پروژه شامل دو ماژول STM32 است: واحد فرستنده (`sender`) و واحد گیرنده (`receiver`). واحد فرستنده فرمان‌ها را از طریق سوئیچ یا رابط ریموت ارسال می‌کند و واحد گیرنده این فرمان‌ها را برای فعال‌سازی موتور/عملگر درب پردازش می‌کند.

## ساختار پروژه

- `sender/` — ماژول فرستنده
- `receiver/` — ماژول گیرنده
- `Schematic.pdf` — نقشه مدار و سیم‌کشی
- `Remoot.ioc` — پیکربندی STM32CubeMX

## قابلیت‌ها

- ارتباط بی‌سیم NRF
- طراحی مبتنی بر میکروکنترلر STM32
- کنترل درب رول آپ / درب پارکینگ
- معماری ساده و فشرده‌ی Embedded

## نحوه استفاده

1. پروژه را در STM32CubeIDE باز کنید.
2. Firmware مربوط به ماژول‌های `sender` و `receiver` را کامپایل کنید.
3. بردها را طبق نقشه مدار برنامه‌ریزی و فلش کنید.
4. مسیر فرمان بی‌سیم بین فرستنده و گیرنده را تست کنید.
