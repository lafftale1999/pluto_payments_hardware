file(GLOB_RECURSE SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c)

idf_component_register(
    SRCS 
        "main.c"
        {SRC_FILES}
    INCLUDE_DIRS 
        "."
)