/*
 * Copyright (C) 2026 SpotSong Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "SystemInfo.h"
#include <QDir>
#include <QSysInfo>
#include <cstdlib>

SystemInfo::SystemInfo(QObject *parent)
    : QObject(parent)
{
}

QString SystemInfo::username() const
{
    QString name = QString::fromLocal8Bit(qgetenv("USER"));
    if (name.isEmpty()) {
        name = QString::fromLocal8Bit(qgetenv("USERNAME"));
    }
    if (name.isEmpty()) {
        name = QDir::home().dirName();
    }
    if (name.isEmpty()) {
        name = "Music Lover";
    }
    return name;
}

QString SystemInfo::platformName() const
{
    return QSysInfo::prettyProductName();
}
