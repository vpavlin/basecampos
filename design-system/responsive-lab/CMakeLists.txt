cmake_minimum_required(VERSION 3.16)
project(LogosResponsiveLab VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Qml)

# The responsive lab (docs/RESPONSIVE.md): common app layouts built only from
# design-system controls, plus a shot mode that renders them at a given width
# and reports horizontal overflow and small touch targets. Same STATIC
# design-system linkage as the storybook.
add_executable(LogosResponsiveLab main.cpp)
target_link_libraries(LogosResponsiveLab PRIVATE
    Qt6::Core Qt6::Gui Qt6::Quick Qt6::QuickControls2 Qt6::Qml
    Logos::DesignSystem
)
# QML stays loose so screens can be edited without a rebuild.
target_compile_definitions(LogosResponsiveLab PRIVATE
    LAB_QML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml"
)
