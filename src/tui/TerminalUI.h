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

#pragma once

#include <QObject>
#include <QTimer>
#include <QSocketNotifier>
#include <QString>
#include <QList>
#include <QVariantMap>

#include "../core/MusicLibrary.h"
#include "../core/PlaylistManager.h"
#include "../core/AudioPlayer.h"
#include "../core/SystemInfo.h"
#include "../core/DiscordRPC.h"

class TerminalUI : public QObject
{
    Q_OBJECT

public:
    enum Tab {
        TabHome = 0,
        TabLibrary,
        TabPlaylists,
        TabQueue,
        TabSearch,
        TabCount
    };

    enum InputMode {
        ModeNormal = 0,
        ModeSearch,
        ModeImport,
        ModeCreatePlaylist,
        ModeAddToPlaylist,
        ModeConfirmDelete
    };

    explicit TerminalUI(MusicLibrary *library,
                        PlaylistManager *playlistMgr,
                        AudioPlayer *player,
                        QObject *parent = nullptr);
    ~TerminalUI() override;

    void start();

public slots:
    void render();

private slots:
    void handleStdin();
    void refreshData();

private:
    void initTerminal();
    void restoreTerminal();
    void getTerminalSize(int &width, int &height);

    void processKey(int key);
    void handleNormalKey(int key);
    void handleInputKey(int key);

    void playSelectedSong();
    void deleteSelected();

    QString truncateText(const QString &text, int maxWidth) const;
    QString padRight(const QString &text, int width) const;
    QString buildProgressBar(qint64 currentMs, qint64 totalMs, int width) const;

    MusicLibrary *m_library;
    PlaylistManager *m_playlistMgr;
    AudioPlayer *m_player;

    QTimer *m_renderTimer;
    QSocketNotifier *m_stdinNotifier = nullptr;

    Tab m_currentTab = TabHome;
    InputMode m_inputMode = ModeNormal;

    int m_selectedHomeIndex = 0;
    int m_selectedLibraryIndex = 0;
    int m_selectedPlaylistIndex = 0;
    int m_selectedPlaylistSongIndex = 0;
    int m_selectedQueueIndex = 0;
    int m_selectedAddPlaylistIndex = 0;

    int m_activePlaylistId = -1;
    QString m_activePlaylistName;

    QString m_inputBuffer;
    QString m_statusMessage;
    QTimer *m_statusTimer;

    QList<SongRecord> m_cachedMostPlayed;
    QList<SongRecord> m_cachedAllSongs;
    QList<PlaylistRecord> m_cachedPlaylists;
    QList<SongRecord> m_cachedPlaylistSongs;
};
