# Projet ESP-IDF — MGF Gauge LVGL (ESP32-S3)
# Reproduction de l'écran RPM principal du projet Flutter MGF, en LVGL, pour
# la carte LILYGO T-RGB 2.1 Full Circle H597 + écran RGB ST7701S.
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(mgf_gauge_lvgl)

# Un assert LVGL (CONFIG_LV_USE_ASSERT_NULL/MALLOC) exécute LV_ASSERT_HANDLER,
# par défaut `while(1);` : l'écran resterait figé jusqu'à la coupure du contact
# (le watchdog de tâche ne panique pas). Le Kconfig LVGL 9.2.2 n'expose que
# l'en-tête (CONFIG_LV_ASSERT_HANDLER_INCLUDE="stdlib.h", sdkconfig.defaults) ;
# le gestionnaire est donc défini ici, PUBLIC pour que les en-têtes LVGL vus par
# main/ concordent. abort() -> panic handler -> redémarrage
# (CONFIG_ESP_SYSTEM_PANIC_PRINT_REBOOT). La macro est développée comme une
# instruction complète, mais un `;` ne survit pas au passage CMake -> ligne de
# commande : la forme `while((abort(), 1)) {}` est une instruction sans `;`.
idf_component_get_property(mgf_lvgl_lib lvgl__lvgl COMPONENT_LIB)
target_compile_definitions(${mgf_lvgl_lib} PUBLIC
    "LV_ASSERT_HANDLER=while((abort(), 1)) {}")
