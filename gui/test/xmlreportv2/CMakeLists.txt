qt5_wrap_cpp(test-xmlreportv2_SRC testxmlreportv2.h)
add_custom_target(build-xmlreportv2-deps SOURCES ${test-xmlreportv2_SRC})
add_dependencies(gui-build-deps build-xmlreportv2-deps)
if(USE_BUNDLED_TINYXML2)
    list(APPEND test-xmlreportv2_SRC $<TARGET_OBJECTS:tinyxml2_objs>)
endif()
add_executable(test-xmlreportv2
        ${test-xmlreportv2_SRC}
        testxmlreportv2.cpp
        ${CMAKE_SOURCE_DIR}/gui/erroritem.cpp
        ${CMAKE_SOURCE_DIR}/gui/report.cpp
        ${CMAKE_SOURCE_DIR}/gui/xmlreport.cpp
        ${CMAKE_SOURCE_DIR}/gui/xmlreportv2.cpp
        $<TARGET_OBJECTS:lib_objs>
        $<TARGET_OBJECTS:simplecpp_objs>
        )
target_include_directories(test-xmlreportv2 PRIVATE ${CMAKE_SOURCE_DIR}/gui ${CMAKE_SOURCE_DIR}/lib)
target_compile_definitions(test-xmlreportv2 PRIVATE SRCDIR="${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(test-xmlreportv2 Qt5::Core Qt5::Test)
if (HAVE_RULES)
    target_link_libraries(test-xmlreportv2 ${PCRE_LIBRARY})
endif()
if (USE_Z3)
    target_link_libraries(test-xmlreportv2 ${Z3_LIBRARIES})
endif()