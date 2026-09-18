# Projet ESP-IDF — MGF Gauge LVGL (ESP32-P4)
# Reproduction de l'écran RPM principal du projet Flutter MGF, en LVGL, pour
# évaluer la faisabilité d'un portage embarqué sur ESP32-P4 + écran MIPI-DSI.
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(mgf_gauge_lvgl)
