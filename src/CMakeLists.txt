add_subdirectory(kcm)

set(effect_SRCS
    Model.cc
    YetAnotherMagicLampEffect.cc
    main.cpp
)

kconfig_add_kcfg_files(effect_SRCS
    YetAnotherMagicLampConfig.kcfgc
)

add_library(kwin4_effect_yetanothermagiclamp MODULE ${effect_SRCS})

target_link_libraries(kwin4_effect_yetanothermagiclamp PRIVATE
    KF6::ConfigGui
    KWin::kwin
)

install(
    TARGETS
        kwin4_effect_yetanothermagiclamp

    DESTINATION
        ${KDE_INSTALL_PLUGINDIR}/kwin/effects/plugins
)
