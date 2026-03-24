set(kcm_SRCS
    YetAnotherMagicLampEffectKCM.cc
)

kconfig_add_kcfg_files(kcm_SRCS
    ../YetAnotherMagicLampConfig.kcfgc
)

qt_wrap_ui(kcm_SRCS YetAnotherMagicLampEffectKCM.ui)

qt_add_dbus_interface(kcm_SRCS ${KWIN_EFFECTS_INTERFACE} kwineffects_interface)

add_library(kwin_yetanothermagiclamp_config MODULE ${kcm_SRCS})

target_link_libraries(kwin_yetanothermagiclamp_config
    Qt6::Core
    Qt6::DBus
    Qt6::Gui
    KF6::ConfigCore
    KF6::ConfigGui
    KF6::ConfigWidgets
    KF6::KCMUtils
    KF6::CoreAddons
)

install(
    TARGETS
        kwin_yetanothermagiclamp_config

    DESTINATION
        ${KDE_INSTALL_PLUGINDIR}/kwin/effects/configs
)
