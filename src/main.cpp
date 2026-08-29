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

#include <QCoreApplication>
#include <QDir>

#include "core/DatabaseManager.h"
#include "core/MusicLibrary.h"
#include "core/PlaylistManager.h"
#include "core/AudioPlayer.h"
#include "core/SystemInfo.h"
#include "core/DiscordRPC.h"
#include "tui/TerminalUI.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("SpotSong");
    app.setOrganizationName("SpotSong");
    app.setOrganizationDomain("spotsong.org");

    DatabaseManager::instance().initialize();

    MusicLibrary musicLibrary;
    PlaylistManager playlistManager;
    AudioPlayer audioPlayer;
    SystemInfo systemInfo;
    DiscordRPC &discordRpc = DiscordRPC::instance();
    Q_UNUSED(discordRpc);
    Q_UNUSED(systemInfo);

    TerminalUI tui(&musicLibrary, &playlistManager, &audioPlayer);
    tui.start();

    return app.exec();
}
