set(srcs "main.c"
         "gatt_svr.c"
         "led_task.c")

idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS ".")
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")
