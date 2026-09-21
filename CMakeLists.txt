# Projet ESP-IDF — MGF Gauge LVGL (ESP32-S3)
# Reproduction de l'écran RPM principal du projet Flutter MGF, en LVGL, pour
# la carte Waveshare ESP32-S3-Touch-LCD-2.1 + écran RGB ST7701.
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(mgf_gauge_lvgl)
