cmake_minimum_required(VERSION 3.20)
project(kwin-effects-yet-another-magic-lamp)

set(KF_MIN_VERSION "6.0.0")
set(QT_MIN_VERSION "6.6.0")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH
    ${CMAKE_MODULE_PATH}
    ${ECM_MODULE_PATH}
)

include(FeatureSummary)
include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
    Core
    DBus
    Gui
)

find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS
    Config
    ConfigWidgets
    CoreAddons
    KCMUtils
)

find_package(KWin REQUIRED COMPONENTS
    kwineffects
)

find_package(KWinDBusInterface CONFIG REQUIRED)

add_subdirectory(src)

feature_summary(WHAT ALL)
