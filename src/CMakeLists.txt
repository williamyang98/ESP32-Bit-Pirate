# file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/*.cpp")
# idf_component_register(SRCS ${SOURCE_FILES})
idf_component_register(SRCS "main.cpp")