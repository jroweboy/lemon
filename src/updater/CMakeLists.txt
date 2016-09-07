if (WIN_CREATE_INSTALLER)
    include_directories(${CPR_INCLUDE_DIRS})
    # fill in the necessary details into the nuspec file template
    set(VERSION_BUILD_NUMBER $ENV{APPVEYOR_BUILD_NUMBER})
    configure_file(${CMAKE_SOURCE_DIR}/citra.nuspec ${CMAKE_BINARY_DIR}/citra.nuspec)
    # Generate cpp with most recent tag info
    set(TAG_NAME "lemon-${VERSION_BUILD_NUMBER}")
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/tagname.cpp.in" "${CMAKE_CURRENT_SOURCE_DIR}/tagname.cpp" @ONLY)
    # set the source files to work with
    set (SRCS
        tagname.cpp
        updater.cpp
        updater.rc
        updater.manifest
        # need to compile getopt as part of this app since the getopt target uses an incompatible runtime dll (/MD)
        getopt.c
    )
    set (HEADERS
        getopt.h
        tagname.h
    )

    add_executable(updater ${SRCS} ${HEADERS})
    # Override the properties for this file to compile it with static runtime so it can install msvc runtime on the users machine
    target_compile_options(updater BEFORE PRIVATE "/MT")
    target_compile_options(cpr BEFORE PRIVATE "/MT")
    target_compile_definitions(updater PUBLIC STATIC_GETOPT)
    target_include_directories(updater INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
    set_target_properties(updater PROPERTIES LINK_FLAGS "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
    target_link_libraries(updater ${CPR_LIBRARIES})
    if (ENABLE_QT)
        add_dependencies(updater citra-qt)
    endif()
    if (ENABLE_SDL2)
        add_dependencies(updater citra)
    endif()
endif()
