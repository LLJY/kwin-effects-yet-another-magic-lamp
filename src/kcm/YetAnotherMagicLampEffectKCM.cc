/*
 * Copyright (C) 2018 Vlad Zagorodniy <vladzzag@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

// Own
#include "YetAnotherMagicLampEffectKCM.h"

// Auto-generated
#include "YetAnotherMagicLampConfig.h"
#include "kwineffects_interface.h"

#include <KPluginFactory>
#include <KSharedConfig>

K_PLUGIN_CLASS_WITH_JSON(YetAnotherMagicLampEffectKCM, "metadata.json")

YetAnotherMagicLampEffectKCM::YetAnotherMagicLampEffectKCM(QObject* parent, const KPluginMetaData& data)
    : KCModule(parent, data)
{
    m_ui.setupUi(widget());
    YetAnotherMagicLampConfig::self()->setSharedConfig(KSharedConfig::openConfig(QStringLiteral("kwinrc")));
    YetAnotherMagicLampConfig::self()->load();
    addConfig(YetAnotherMagicLampConfig::self(), widget());
}

YetAnotherMagicLampEffectKCM::~YetAnotherMagicLampEffectKCM()
{
}

void YetAnotherMagicLampEffectKCM::save()
{
    KCModule::save();
    OrgKdeKwinEffectsInterface interface(QStringLiteral("org.kde.KWin"),
        QStringLiteral("/Effects"),
        QDBusConnection::sessionBus());
    interface.reconfigureEffect(QStringLiteral("kwin4_effect_yetanothermagiclamp"));
}

#include "YetAnotherMagicLampEffectKCM.moc"

#include "moc_YetAnotherMagicLampEffectKCM.cpp"
