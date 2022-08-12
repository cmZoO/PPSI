file(GLOB_RECURSE SRC_FRONTEND ${CMAKE_CURRENT_LIST_DIR}/*.cpp)

add_executable(frontend ${SRC_FRONTEND})

target_link_libraries(frontend oc::libPSI gemini SCI-Cheetah)
target_compile_definitions(frontend PUBLIC SCI_OT=1 USE_CHEETAH=${USE_CHEETAH})