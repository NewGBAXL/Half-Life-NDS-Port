qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(hlds-convert-gui
    main.cpp
    MainWindow.cpp
    MainWindow.hpp
    CookWorker.cpp
    CookWorker.hpp
)

target_link_libraries(hlds-convert-gui
    PRIVATE
        hlds_convert_core
        Qt6::Widgets
)

set_target_properties(hlds-convert-gui PROPERTIES
    WIN32_EXECUTABLE ON
)
