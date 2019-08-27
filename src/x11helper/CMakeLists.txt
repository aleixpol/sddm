include_directories(
    "${CMAKE_SOURCE_DIR}/src/common"
    "${CMAKE_BINARY_DIR}/src/common"
)

set(SOURCES
    ${CMAKE_SOURCE_DIR}/src/common/Configuration.cpp
    ${CMAKE_SOURCE_DIR}/src/common/ConfigReader.cpp
    ${CMAKE_SOURCE_DIR}/src/common/SafeDataStream.cpp
    main.cpp
    runner.cpp
    runner.h
)

add_executable(sddm-x11-helper ${SOURCES})
target_link_libraries(sddm-x11-helper Qt5::Core Qt5::Network)
install(TARGETS sddm-x11-helper DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}")
