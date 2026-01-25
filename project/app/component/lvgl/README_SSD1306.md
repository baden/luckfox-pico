# LVGL з SSD1306 OLED (128x32x1bit) на Luckfox Pico

## Крок 1: Конфігурація SSD1306 драйвера

1. **Додайте підтримку SSD1306 в конфігурацію ядра:**
   ```bash
   make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- menuconfig
   ```
   
   Увімкніть:
   ```
   Device Drivers  --->
     Graphics support  --->
       Framebuffer Devices  --->
         <*>   Support for frame buffer devices  --->
         [*]   Enable firmware specific EDID
         [*]   Firmware EDID support
         [*]   Enable Videomode Helpers
         [*]   Staging drivers  --->
           <*>   Tiny Framebuffer support (FBTFT)
           <*>   SSD1306 OLED driver
   ```

2. **Додайте SSD1306 в device tree:**
   
   Додайте до вашого device tree файлу:
   ```dts
   &spi0 {
       status = "okay";
       
       ssd1306: oled@0 {
           compatible = "solomon,ssd1306";
           reg = <0>;
           spi-max-frequency = <4000000>;
           dc-gpios = <&gpio0 RK_PA1 GPIO_ACTIVE_HIGH>;
           reset-gpios = <&gpio0 RK_PA0 GPIO_ACTIVE_HIGH>;
           width = <128>;
           height = <32>;
           fps = <30>;
           buswidth = <8>;
           rotate = <0>;
           bgr = <0>;
           debug = <0>;
       };
   };
   ```

## Крок 2: Компіляція та завантаження ядра

```bash
# Зберіть ядро
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j4

# Встановіть нове ядро на плату
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- modules_install INSTALL_MOD_PATH=./output
```

## Крок 3: Перевірка framebuffer

Після завантаження перевірте наявність /dev/fb0:
```bash
ls -l /dev/fb0
cat /sys/class/graphics/fb0/virtual_size  # Повинно показати 128,32
cat /sys/class/graphics/fb0/bits_per_pixel # Повинно показати 1
```

## Крок 4: Скомпілюйте LVGL застосунок

```bash
cd /project/app/component/lvgl
make -f Makefile_ssd1306
```

## Крок 5: Запустіть застосунок

```bash
# Скопіюйте на плату
scp ssd1306_demo root@<IP>:/usr/bin/

# Запустіть
./ssd1306_demo
```

## Важливі примітки:

1. **Роздільна здатність:** 128x32 пікселів
2. **Глибина кольору:** 1 біт (монохромний)
3. **Пам'ять LVGL:** 32KB (налаштовано в lv_conf_ssd1306.h)
4. **Шрифти:** Використовуйте малі шрифти (montserrat 8-10pt)
5. **Продуктивність:** Обмежена через низьку роздільну здатність та SPI

## Усунення несправностей:

1. **Якщо /dev/fb0 відсутній:**
   - Перевірте конфігурацію ядра
   - Перевірте device tree налаштування
   - Перевірте підключення GPIO

2. **Якщо застосунок не запускається:**
   - Перевірте права доступу до /dev/fb0
   - Додайте користувача в групу video або запускайте з sudo

3. **Якщо зображення не відображається:**
   - Перевірте правильність flush функції
   - Перевірте орієнтацію екрану (rotate параметр)
   - Перевірте підключення SSD1306

## Налаштування:

Для зміни налаштувань відредагуйте `lv_conf_ssd1306.h`:
- Роздільна здатність: `LV_HOR_RES_MAX`, `LV_VER_RES_MAX`
- Пам'ять: `LV_MEM_SIZE`
- Шрифти: увімкніть потрібні шрифти в розділі FONT SETTINGS