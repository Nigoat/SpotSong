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

#include "TerminalUI.h"
#include "../core/DatabaseManager.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <signal.h>
#endif

namespace {
#ifndef _WIN32
    struct termios orig_termios;
    bool termios_saved = false;
#endif

    void resetTerminalOnSignal(int) {
#ifndef _WIN32
        if (termios_saved) {
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        }
#endif
        std::cout << "\033[?1049l\033[?25h\033[0m" << std::flush;
        _exit(0);
    }
}

TerminalUI::TerminalUI(MusicLibrary *library,
                       PlaylistManager *playlistMgr,
                       AudioPlayer *player,
                       QObject *parent)
    : QObject(parent)
    , m_library(library)
    , m_playlistMgr(playlistMgr)
    , m_player(player)
    , m_renderTimer(new QTimer(this))
    , m_statusTimer(new QTimer(this))
{
    m_statusTimer->setSingleShot(true);
    connect(m_statusTimer, &QTimer::timeout, this, [this]() {
        m_statusMessage.clear();
    });

    connect(&DatabaseManager::instance(), &DatabaseManager::databaseChanged, this, &TerminalUI::refreshData);
    connect(&DatabaseManager::instance(), &DatabaseManager::playlistChanged, this, &TerminalUI::refreshData);
    connect(m_library, &MusicLibrary::importFinished, this, [this](int count) {
        m_statusMessage = QString("Successfully imported %1 songs!").arg(count);
        m_statusTimer->start(4000);
        refreshData();
    });

    m_renderTimer->setInterval(150);
    connect(m_renderTimer, &QTimer::timeout, this, &TerminalUI::render);
}

TerminalUI::~TerminalUI()
{
    restoreTerminal();
}

void TerminalUI::initTerminal()
{
#ifndef _WIN32
    if (!termios_saved) {
        tcgetattr(STDIN_FILENO, &orig_termios);
        termios_saved = true;

        struct sigaction sa;
        sa.sa_handler = resetTerminalOnSignal;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction(SIGINT, &sa, nullptr);
        sigaction(SIGTERM, &sa, nullptr);
    }

    struct termios raw = orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    m_stdinNotifier = new QSocketNotifier(STDIN_FILENO, QSocketNotifier::Read, this);
    connect(m_stdinNotifier, &QSocketNotifier::activated, this, &TerminalUI::handleStdin);
#else
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif

    std::cout << "\033[?1049h\033[?25l\033[2J\033[H" << std::flush;
}

void TerminalUI::restoreTerminal()
{
#ifndef _WIN32
    if (termios_saved) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    }
#endif
    std::cout << "\033[?1049l\033[?25h\033[0m" << std::flush;
}

void TerminalUI::getTerminalSize(int &width, int &height)
{
    width = 100;
    height = 30;

#ifndef _WIN32
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0) {
        width = ws.ws_col;
        height = ws.ws_row;
    }
#else
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    }
#endif
}

void TerminalUI::start()
{
    initTerminal();
    refreshData();
    m_renderTimer->start();
    render();
}

void TerminalUI::refreshData()
{
    m_cachedMostPlayed = DatabaseManager::instance().getMostPlayedSongs(10);
    m_cachedAllSongs = DatabaseManager::instance().getAllSongs(m_library->searchQuery());
    m_cachedPlaylists = DatabaseManager::instance().getAllPlaylists();

    if (m_activePlaylistId > 0) {
        m_cachedPlaylistSongs = DatabaseManager::instance().getPlaylistSongs(m_activePlaylistId);
    }
}

void TerminalUI::handleStdin()
{
#ifndef _WIN32
    char c;
    while (read(STDIN_FILENO, &c, 1) > 0) {
        if (c == '\033') {
            char seq[5];
            if (read(STDIN_FILENO, &seq[0], 1) == 0) {
                processKey(27);
                continue;
            }
            if (seq[0] == '[') {
                if (read(STDIN_FILENO, &seq[1], 1) == 0) {
                    processKey(27);
                    continue;
                }
                if (seq[1] == 'A') processKey(1001);
                else if (seq[1] == 'B') processKey(1002);
                else if (seq[1] == 'C') processKey(1003);
                else if (seq[1] == 'D') processKey(1004);
                else if (seq[1] == 'Z') processKey(1005);
                else if (seq[1] == '3') {
                    char seq2;
                    if (read(STDIN_FILENO, &seq2, 1) > 0 && seq2 == '~') {
                        processKey(1006);
                    }
                }
            } else {
                processKey(27);
            }
        } else {
            processKey(static_cast<unsigned char>(c));
        }
    }
#endif
}

void TerminalUI::processKey(int key)
{
    if (m_inputMode != ModeNormal) {
        handleInputKey(key);
    } else {
        handleNormalKey(key);
    }
    render();
}

void TerminalUI::handleNormalKey(int key)
{
    switch (key) {
    case 'q':
    case 'Q':
        restoreTerminal();
        QCoreApplication::quit();
        break;

    case '\t':
        m_currentTab = static_cast<Tab>((m_currentTab + 1) % TabCount);
        if (m_currentTab == TabSearch) {
            m_inputMode = ModeSearch;
            m_inputBuffer.clear();
        }
        break;

    case 1005:
        m_currentTab = static_cast<Tab>((m_currentTab + TabCount - 1) % TabCount);
        break;

    case '1': m_currentTab = TabHome; break;
    case '2': m_currentTab = TabLibrary; break;
    case '3': m_currentTab = TabPlaylists; m_activePlaylistId = -1; break;
    case '4': m_currentTab = TabQueue; break;
    case '5':
        m_currentTab = TabSearch;
        m_inputMode = ModeSearch;
        m_inputBuffer.clear();
        break;

    case ' ':
        m_player->togglePlayPause();
        break;

    case 'n':
    case 'N':
        m_player->next();
        break;

    case 'p':
    case 'P':
        m_player->previous();
        break;

    case 1001:
    case 'k':
        if (m_currentTab == TabHome) {
            if (m_selectedHomeIndex > 0) m_selectedHomeIndex--;
        } else if (m_currentTab == TabLibrary || m_currentTab == TabSearch) {
            if (m_selectedLibraryIndex > 0) m_selectedLibraryIndex--;
        } else if (m_currentTab == TabPlaylists) {
            if (m_activePlaylistId > 0) {
                if (m_selectedPlaylistSongIndex > 0) m_selectedPlaylistSongIndex--;
            } else {
                if (m_selectedPlaylistIndex > 0) m_selectedPlaylistIndex--;
            }
        } else if (m_currentTab == TabQueue) {
            if (m_selectedQueueIndex > 0) m_selectedQueueIndex--;
        }
        break;

    case 1002:
    case 'j':
        if (m_currentTab == TabHome) {
            if (m_selectedHomeIndex < m_cachedMostPlayed.size() - 1) m_selectedHomeIndex++;
        } else if (m_currentTab == TabLibrary || m_currentTab == TabSearch) {
            if (m_selectedLibraryIndex < m_cachedAllSongs.size() - 1) m_selectedLibraryIndex++;
        } else if (m_currentTab == TabPlaylists) {
            if (m_activePlaylistId > 0) {
                if (m_selectedPlaylistSongIndex < m_cachedPlaylistSongs.size() - 1) m_selectedPlaylistSongIndex++;
            } else {
                if (m_selectedPlaylistIndex < m_cachedPlaylists.size() - 1) m_selectedPlaylistIndex++;
            }
        } else if (m_currentTab == TabQueue) {
            if (m_selectedQueueIndex < m_player->queue().size() - 1) m_selectedQueueIndex++;
        }
        break;

    case 1004:
    case '[':
        m_player->seek(std::max<qint64>(0, m_player->position() - 5000));
        break;

    case 1003:
    case ']':
        m_player->seek(std::min<qint64>(m_player->duration(), m_player->position() + 5000));
        break;

    case '+':
    case '=':
        m_player->setVolume(m_player->volume() + 0.05);
        break;

    case '-':
        m_player->setVolume(m_player->volume() - 0.05);
        break;

    case 'm':
    case 'M':
        m_player->setMuted(!m_player->isMuted());
        break;

    case 's':
    case 'S':
        m_player->setShuffle(!m_player->isShuffle());
        break;

    case 'r':
    case 'R':
        m_player->setRepeatMode((m_player->repeatMode() + 1) % 3);
        break;

    case '\r':
    case '\n':
        if (m_currentTab == TabPlaylists && m_activePlaylistId <= 0) {
            if (m_selectedPlaylistIndex >= 0 && m_selectedPlaylistIndex < m_cachedPlaylists.size()) {
                m_activePlaylistId = m_cachedPlaylists[m_selectedPlaylistIndex].id;
                m_activePlaylistName = m_cachedPlaylists[m_selectedPlaylistIndex].name;
                m_selectedPlaylistSongIndex = 0;
                refreshData();
            }
        } else {
            playSelectedSong();
        }
        break;

    case 27:
        if (m_currentTab == TabPlaylists && m_activePlaylistId > 0) {
            m_activePlaylistId = -1;
            refreshData();
        }
        break;

    case '/':
        m_currentTab = TabSearch;
        m_inputMode = ModeSearch;
        m_inputBuffer.clear();
        break;

    case 'i':
    case 'I':
        m_inputMode = ModeImport;
        m_inputBuffer.clear();
        break;

    case 'c':
    case 'C':
        m_inputMode = ModeCreatePlaylist;
        m_inputBuffer.clear();
        break;

    case 'a':
    case 'A':
        if (!m_cachedPlaylists.isEmpty()) {
            m_inputMode = ModeAddToPlaylist;
            m_selectedAddPlaylistIndex = 0;
        } else {
            m_statusMessage = "No playlists found! Press 'c' to create one first.";
            m_statusTimer->start(3000);
        }
        break;

    case 'd':
    case 'D':
    case 1006:
        m_inputMode = ModeConfirmDelete;
        break;

    default:
        break;
    }
}

void TerminalUI::handleInputKey(int key)
{
    if (m_inputMode == ModeAddToPlaylist) {
        if (key == 1001 || key == 'k') {
            if (m_selectedAddPlaylistIndex > 0) m_selectedAddPlaylistIndex--;
        } else if (key == 1002 || key == 'j') {
            if (m_selectedAddPlaylistIndex < m_cachedPlaylists.size() - 1) m_selectedAddPlaylistIndex++;
        } else if (key == '\r' || key == '\n') {
            int songId = -1;
            QString songTitle;
            if (m_currentTab == TabHome && m_selectedHomeIndex < m_cachedMostPlayed.size()) {
                songId = m_cachedMostPlayed[m_selectedHomeIndex].id;
                songTitle = m_cachedMostPlayed[m_selectedHomeIndex].title;
            } else if ((m_currentTab == TabLibrary || m_currentTab == TabSearch) && m_selectedLibraryIndex < m_cachedAllSongs.size()) {
                songId = m_cachedAllSongs[m_selectedLibraryIndex].id;
                songTitle = m_cachedAllSongs[m_selectedLibraryIndex].title;
            }

            if (songId > 0 && m_selectedAddPlaylistIndex < m_cachedPlaylists.size()) {
                int pId = m_cachedPlaylists[m_selectedAddPlaylistIndex].id;
                QString pName = m_cachedPlaylists[m_selectedAddPlaylistIndex].name;
                m_playlistMgr->addSongToPlaylist(pId, songId);
                m_statusMessage = QString("Added '%1' to '%2'!").arg(songTitle).arg(pName);
                m_statusTimer->start(3000);
            }
            m_inputMode = ModeNormal;
        } else if (key == 27) {
            m_inputMode = ModeNormal;
        }
        return;
    }

    if (m_inputMode == ModeConfirmDelete) {
        if (key == 'y' || key == 'Y' || key == '\r' || key == '\n') {
            deleteSelected();
            m_inputMode = ModeNormal;
        } else {
            m_inputMode = ModeNormal;
        }
        return;
    }

    if (key == 27) {
        if (m_inputMode == ModeSearch) {
            m_library->setSearchQuery("");
            refreshData();
        }
        m_inputMode = ModeNormal;
        m_inputBuffer.clear();
        return;
    }

    if (key == '\r' || key == '\n') {
        if (m_inputMode == ModeSearch) {
            m_library->setSearchQuery(m_inputBuffer);
            refreshData();
            m_inputMode = ModeNormal;
        } else if (m_inputMode == ModeImport) {
            QString path = m_inputBuffer.trimmed();
            if (!path.isEmpty()) {
                if (path.startsWith("~")) {
                    path = QDir::homePath() + path.mid(1);
                }
                QFileInfo fi(path);
                if (fi.isDir()) {
                    m_library->importFolder(path);
                    m_statusMessage = "Importing folder...";
                } else if (fi.isFile()) {
                    m_library->importFiles(QList<QUrl>() << QUrl::fromLocalFile(path));
                    m_statusMessage = "Importing file...";
                } else {
                    m_statusMessage = "Path does not exist!";
                }
                m_statusTimer->start(3000);
            }
            m_inputMode = ModeNormal;
            m_inputBuffer.clear();
        } else if (m_inputMode == ModeCreatePlaylist) {
            QString name = m_inputBuffer.trimmed();
            if (!name.isEmpty()) {
                int pId = m_playlistMgr->createPlaylist(name);
                m_statusMessage = QString("Created playlist '%1'!").arg(name);
                m_statusTimer->start(3000);
                if (pId > 0) {
                    m_currentTab = TabPlaylists;
                    m_activePlaylistId = pId;
                    m_activePlaylistName = name;
                }
                refreshData();
            }
            m_inputMode = ModeNormal;
            m_inputBuffer.clear();
        }
        return;
    }

    if (key == 127 || key == 8) {
        if (!m_inputBuffer.isEmpty()) {
            m_inputBuffer.chop(1);
            if (m_inputMode == ModeSearch) {
                m_library->setSearchQuery(m_inputBuffer);
                refreshData();
            }
        }
        return;
    }

    if (key >= 32 && key <= 126) {
        m_inputBuffer.append(static_cast<char>(key));
        if (m_inputMode == ModeSearch) {
            m_library->setSearchQuery(m_inputBuffer);
            refreshData();
        }
    }
}

void TerminalUI::playSelectedSong()
{
    if (m_currentTab == TabHome) {
        if (m_selectedHomeIndex >= 0 && m_selectedHomeIndex < m_cachedMostPlayed.size()) {
            QVariantList list;
            for (const auto &s : m_cachedMostPlayed) list.append(s.toMap());
            m_player->playQueue(list, m_selectedHomeIndex);
        }
    } else if (m_currentTab == TabLibrary || m_currentTab == TabSearch) {
        if (m_selectedLibraryIndex >= 0 && m_selectedLibraryIndex < m_cachedAllSongs.size()) {
            QVariantList list;
            for (const auto &s : m_cachedAllSongs) list.append(s.toMap());
            m_player->playQueue(list, m_selectedLibraryIndex);
        }
    } else if (m_currentTab == TabPlaylists) {
        if (m_activePlaylistId > 0 && m_selectedPlaylistSongIndex >= 0 && m_selectedPlaylistSongIndex < m_cachedPlaylistSongs.size()) {
            QVariantList list;
            for (const auto &s : m_cachedPlaylistSongs) list.append(s.toMap());
            m_player->playQueue(list, m_selectedPlaylistSongIndex);
        }
    } else if (m_currentTab == TabQueue) {
        if (m_selectedQueueIndex >= 0 && m_selectedQueueIndex < m_player->queue().size()) {
            m_player->playQueue(m_player->queue(), m_selectedQueueIndex);
        }
    }
}

void TerminalUI::deleteSelected()
{
    if (m_currentTab == TabPlaylists) {
        if (m_activePlaylistId > 0 && m_selectedPlaylistSongIndex < m_cachedPlaylistSongs.size()) {
            int songId = m_cachedPlaylistSongs[m_selectedPlaylistSongIndex].id;
            m_playlistMgr->removeSongFromPlaylist(m_activePlaylistId, songId);
            m_statusMessage = "Removed song from playlist.";
            m_statusTimer->start(3000);
            refreshData();
        } else if (m_activePlaylistId <= 0 && m_selectedPlaylistIndex < m_cachedPlaylists.size()) {
            int pId = m_cachedPlaylists[m_selectedPlaylistIndex].id;
            QString pName = m_cachedPlaylists[m_selectedPlaylistIndex].name;
            m_playlistMgr->deletePlaylist(pId);
            m_statusMessage = QString("Deleted playlist '%1'.").arg(pName);
            m_statusTimer->start(3000);
            refreshData();
        }
    } else if (m_currentTab == TabLibrary && m_selectedLibraryIndex < m_cachedAllSongs.size()) {
        int songId = m_cachedAllSongs[m_selectedLibraryIndex].id;
        QString title = m_cachedAllSongs[m_selectedLibraryIndex].title;
        m_library->deleteSong(songId);
        m_statusMessage = QString("Deleted '%1' from library.").arg(title);
        m_statusTimer->start(3000);
        refreshData();
    }
}

QString TerminalUI::truncateText(const QString &text, int maxWidth) const
{
    if (maxWidth <= 0) return "";
    if (text.length() <= maxWidth) return text;
    if (maxWidth <= 3) return text.left(maxWidth);
    return text.left(maxWidth - 3) + "...";
}

QString TerminalUI::padRight(const QString &text, int width) const
{
    QString t = truncateText(text, width);
    while (t.length() < width) {
        t.append(' ');
    }
    return t;
}

QString TerminalUI::buildProgressBar(qint64 currentMs, qint64 totalMs, int width) const
{
    if (width <= 0) return "";
    if (totalMs <= 0) {
        return QString(width, '-');
    }
    double frac = std::clamp(static_cast<double>(currentMs) / static_cast<double>(totalMs), 0.0, 1.0);
    int filled = static_cast<int>(frac * (width - 1));
    filled = std::clamp(filled, 0, width - 1);

    QString bar;
    for (int i = 0; i < filled; ++i) bar.append('=');
    bar.append('>');
    for (int i = filled + 1; i < width; ++i) bar.append('-');
    return bar;
}

void TerminalUI::render()
{
    int termWidth = 100;
    int termHeight = 30;
    getTerminalSize(termWidth, termHeight);
    if (termWidth < 50) termWidth = 50;
    if (termHeight < 12) termHeight = 12;

    int contentWidth = termWidth - 4;

    std::ostringstream out;
    out << "\033[H\033[?25l";

    const std::string reset = "\033[0m";
    const std::string bold = "\033[1m";
    const std::string borderCol = "\033[38;2;80;80;80m";
    const std::string headerText = "\033[1;37m";
    const std::string greenText = "\033[38;2;34;197;94m";
    const std::string grayText = "\033[38;2;160;160;160m";
    const std::string darkGray = "\033[38;2;100;100;100m";
    const std::string whiteText = "\033[38;2;240;240;240m";
    const std::string selectBg = "\033[48;2;45;45;45m\033[38;2;255;255;255m\033[1m";

    auto printLine = [&](const std::string &content) {
        out << borderCol << "| " << reset << content << "\033[K\r\n";
    };

    auto printDivider = [&]() {
        out << borderCol << "+-" << std::string(contentWidth, '-') << "-+\033[K\r\n" << reset;
    };

    std::string discordBadge = DiscordRPC::instance().isConnected() ?
        (greenText + "[RPC: Active]" + reset) :
        (darkGray + "[RPC: Offline]" + reset);

    std::string titleStr = " SpotSong - Music Player ";
    int topDashCount = contentWidth - static_cast<int>(titleStr.length()) - 14;
    if (topDashCount < 2) topDashCount = 2;

    out << borderCol << "+-" << headerText << titleStr << reset
        << borderCol << std::string(topDashCount, '-') << " " << discordBadge << " " << borderCol << "-+\033[K\r\n" << reset;

    std::string tabLine;
    const char *tabNames[] = { "[1] Home", "[2] Library", "[3] Playlists", "[4] Queue", "[5] Search" };
    for (int i = 0; i < TabCount; ++i) {
        if (m_currentTab == i) {
            tabLine += selectBg + " " + tabNames[i] + " " + reset + "  ";
        } else {
            tabLine += grayText + tabNames[i] + reset + "  ";
        }
    }
    printLine(tabLine);

    printDivider();

    int bodyHeight = termHeight - 12;
    if (bodyHeight < 6) bodyHeight = 6;

    if (m_inputMode == ModeAddToPlaylist) {
        printLine(bold + whiteText + "Select Playlist to Add Song:" + reset);

        for (int i = 0; i < bodyHeight - 1; ++i) {
            if (i < m_cachedPlaylists.size()) {
                bool isSel = (i == m_selectedAddPlaylistIndex);
                std::string line = QString(" [%1] %2 (%3 songs)")
                    .arg(i + 1)
                    .arg(m_cachedPlaylists[i].name)
                    .arg(m_cachedPlaylists[i].songCount).toStdString();
                if (isSel) {
                    printLine(selectBg + "> " + padRight(QString::fromStdString(line), contentWidth - 3).toStdString() + reset);
                } else {
                    printLine(whiteText + "  " + padRight(QString::fromStdString(line), contentWidth - 3).toStdString() + reset);
                }
            } else {
                printLine("");
            }
        }
    } else if (m_inputMode == ModeConfirmDelete) {
        printLine(bold + "\033[38;2;239;68;68m" + "Confirm Delete:" + reset);
        printLine(whiteText + "Are you sure you want to delete the selected item? (y/n)" + reset);

        for (int i = 0; i < bodyHeight - 2; ++i) {
            printLine("");
        }
    } else if (m_currentTab == TabHome) {
        printLine(bold + whiteText + "* Most Played Songs This Week" + reset);

        int numSongs = m_cachedMostPlayed.size();
        if (numSongs == 0) {
            printLine(grayText + "No songs played yet! Press [i] to import songs or [2] for Library." + reset);
            for (int i = 0; i < bodyHeight - 2; ++i) {
                printLine("");
            }
        } else {
            int scrollOffset = 0;
            if (m_selectedHomeIndex >= bodyHeight - 1) {
                scrollOffset = m_selectedHomeIndex - (bodyHeight - 2);
            }

            for (int i = 0; i < bodyHeight - 1; ++i) {
                int songIdx = i + scrollOffset;
                if (songIdx < numSongs) {
                    const auto &song = m_cachedMostPlayed[songIdx];
                    bool isSel = (songIdx == m_selectedHomeIndex);
                    bool isNowPlaying = (m_player->currentSong().value("id").toInt() == song.id);

                    QString numCol = QString("%1.").arg(songIdx + 1, 2);
                    QString playIcon = isNowPlaying ? (m_player->isPlaying() ? "> " : "||") : "  ";
                    int titleWidth = std::max(15, (contentWidth - 35) / 2);
                    int artistWidth = std::max(15, (contentWidth - 35) / 2);

                    QString tCol = padRight(song.title.isEmpty() ? "Unknown Title" : song.title, titleWidth);
                    QString aCol = padRight(song.artist.isEmpty() ? "Unknown Artist" : song.artist, artistWidth);
                    QString dCol = m_player->formatTime(song.durationMs);
                    QString pCol = QString("%1 plays").arg(song.playCount, 3);

                    QString line = QString("%1 %2%3  %4  %5  %6")
                        .arg(numCol)
                        .arg(playIcon)
                        .arg(tCol)
                        .arg(aCol)
                        .arg(dCol)
                        .arg(pCol);

                    if (isSel) {
                        printLine(selectBg + padRight(line, contentWidth - 1).toStdString() + reset);
                    } else if (isNowPlaying) {
                        printLine(greenText + bold + padRight(line, contentWidth - 1).toStdString() + reset);
                    } else {
                        printLine(whiteText + padRight(line, contentWidth - 1).toStdString() + reset);
                    }
                } else {
                    printLine("");
                }
            }
        }
    } else if (m_currentTab == TabLibrary || m_currentTab == TabSearch) {
        int titleWidth = std::max(18, (contentWidth - 32) * 45 / 100);
        int artistWidth = std::max(14, (contentWidth - 32) * 35 / 100);
        int albumWidth = std::max(10, (contentWidth - 32) * 20 / 100);

        QString header = QString("   #  %1  %2  %3  %4")
            .arg(padRight("Title", titleWidth))
            .arg(padRight("Artist", artistWidth))
            .arg(padRight("Album", albumWidth))
            .arg("Time");

        printLine(darkGray + bold + padRight(header, contentWidth - 1).toStdString() + reset);

        int numSongs = m_cachedAllSongs.size();
        if (numSongs == 0) {
            printLine(grayText + "No songs found. Press [i] to import files or folders." + reset);
            for (int i = 0; i < bodyHeight - 2; ++i) {
                printLine("");
            }
        } else {
            int scrollOffset = 0;
            if (m_selectedLibraryIndex >= bodyHeight - 2) {
                scrollOffset = m_selectedLibraryIndex - (bodyHeight - 3);
            }

            for (int i = 0; i < bodyHeight - 2; ++i) {
                int songIdx = i + scrollOffset;
                if (songIdx < numSongs) {
                    const auto &song = m_cachedAllSongs[songIdx];
                    bool isSel = (songIdx == m_selectedLibraryIndex);
                    bool isNowPlaying = (m_player->currentSong().value("id").toInt() == song.id);

                    QString numCol = QString("%1").arg(songIdx + 1, 3);
                    QString playIcon = isNowPlaying ? (m_player->isPlaying() ? ">" : "=") : " ";
                    QString tCol = padRight(song.title.isEmpty() ? "Unknown Title" : song.title, titleWidth);
                    QString aCol = padRight(song.artist.isEmpty() ? "Unknown Artist" : song.artist, artistWidth);
                    QString alCol = padRight(song.album.isEmpty() ? "Unknown Album" : song.album, albumWidth);
                    QString dCol = m_player->formatTime(song.durationMs);

                    QString line = QString("%1 %2 %3  %4  %5  %6")
                        .arg(playIcon)
                        .arg(numCol)
                        .arg(tCol)
                        .arg(aCol)
                        .arg(alCol)
                        .arg(dCol);

                    if (isSel) {
                        printLine(selectBg + padRight(line, contentWidth - 1).toStdString() + reset);
                    } else if (isNowPlaying) {
                        printLine(greenText + bold + padRight(line, contentWidth - 1).toStdString() + reset);
                    } else {
                        printLine(whiteText + padRight(line, contentWidth - 1).toStdString() + reset);
                    }
                } else {
                    printLine("");
                }
            }
        }
    } else if (m_currentTab == TabPlaylists) {
        if (m_activePlaylistId > 0) {
            QString plHeader = QString("< [Esc] Back to Playlists  |  Playlist: %1 (%2 songs)")
                .arg(m_activePlaylistName)
                .arg(m_cachedPlaylistSongs.size());
            printLine(bold + whiteText + padRight(plHeader, contentWidth - 1).toStdString() + reset);

            int numSongs = m_cachedPlaylistSongs.size();
            int scrollOffset = 0;
            if (m_selectedPlaylistSongIndex >= bodyHeight - 2) {
                scrollOffset = m_selectedPlaylistSongIndex - (bodyHeight - 3);
            }

            for (int i = 0; i < bodyHeight - 2; ++i) {
                int songIdx = i + scrollOffset;
                if (songIdx < numSongs) {
                    const auto &song = m_cachedPlaylistSongs[songIdx];
                    bool isSel = (songIdx == m_selectedPlaylistSongIndex);
                    bool isNowPlaying = (m_player->currentSong().value("id").toInt() == song.id);

                    int titleWidth = std::max(18, (contentWidth - 30) * 50 / 100);
                    int artistWidth = std::max(14, (contentWidth - 30) * 50 / 100);

                    QString numCol = QString("%1.").arg(songIdx + 1, 2);
                    QString playIcon = isNowPlaying ? (m_player->isPlaying() ? "> " : "||") : "  ";
                    QString tCol = padRight(song.title.isEmpty() ? "Unknown Title" : song.title, titleWidth);
                    QString aCol = padRight(song.artist.isEmpty() ? "Unknown Artist" : song.artist, artistWidth);
                    QString dCol = m_player->formatTime(song.durationMs);

                    QString line = QString("%1 %2%3  %4  %5")
                        .arg(numCol)
                        .arg(playIcon)
                        .arg(tCol)
                        .arg(aCol)
                        .arg(dCol);

                    if (isSel) {
                        printLine(selectBg + padRight(line, contentWidth - 1).toStdString() + reset);
                    } else if (isNowPlaying) {
                        printLine(greenText + bold + padRight(line, contentWidth - 1).toStdString() + reset);
                    } else {
                        printLine(whiteText + padRight(line, contentWidth - 1).toStdString() + reset);
                    }
                } else {
                    printLine("");
                }
            }
        } else {
            printLine(bold + whiteText + "Playlists (Press [c] to create, [Enter] to open, [d] to delete)" + reset);

            int numPl = m_cachedPlaylists.size();
            for (int i = 0; i < bodyHeight - 1; ++i) {
                if (i < numPl) {
                    const auto &pl = m_cachedPlaylists[i];
                    bool isSel = (i == m_selectedPlaylistIndex);

                    QString line = QString(" [%1]  %2  (%3 songs)")
                        .arg(i + 1, 2)
                        .arg(padRight(pl.name, std::max(15, contentWidth - 25)))
                        .arg(pl.songCount);

                    if (isSel) {
                        printLine(selectBg + padRight(line, contentWidth - 1).toStdString() + reset);
                    } else {
                        printLine(whiteText + padRight(line, contentWidth - 1).toStdString() + reset);
                    }
                } else {
                    printLine("");
                }
            }
        }
    } else if (m_currentTab == TabQueue) {
        printLine(bold + whiteText + "Play Queue" + reset);

        QVariantList q = m_player->queue();
        int numSongs = q.size();
        int scrollOffset = 0;
        if (m_selectedQueueIndex >= bodyHeight - 2) {
            scrollOffset = m_selectedQueueIndex - (bodyHeight - 3);
        }

        for (int i = 0; i < bodyHeight - 2; ++i) {
            int songIdx = i + scrollOffset;
            if (songIdx < numSongs) {
                QVariantMap song = q[songIdx].toMap();
                bool isSel = (songIdx == m_selectedQueueIndex);
                bool isCurrent = (songIdx == m_player->currentQueueIndex());

                int titleWidth = std::max(18, (contentWidth - 30) * 50 / 100);
                int artistWidth = std::max(14, (contentWidth - 30) * 50 / 100);

                QString numCol = QString("%1.").arg(songIdx + 1, 2);
                QString playIcon = isCurrent ? (m_player->isPlaying() ? "> " : "||") : "  ";
                QString tCol = padRight(song.value("title").toString(), titleWidth);
                QString aCol = padRight(song.value("artist").toString(), artistWidth);
                QString dCol = m_player->formatTime(song.value("durationMs").toLongLong());

                QString line = QString("%1 %2%3  %4  %5")
                    .arg(numCol)
                    .arg(playIcon)
                    .arg(tCol)
                    .arg(aCol)
                    .arg(dCol);

                if (isSel) {
                    printLine(selectBg + padRight(line, contentWidth - 1).toStdString() + reset);
                } else if (isCurrent) {
                    printLine(greenText + bold + padRight(line, contentWidth - 1).toStdString() + reset);
                } else {
                    printLine(whiteText + padRight(line, contentWidth - 1).toStdString() + reset);
                }
            } else {
                printLine("");
            }
        }
    }

    printDivider();

    QVariantMap cur = m_player->currentSong();
    QString curTitle = cur.value("title").toString();
    QString curArtist = cur.value("artist").toString();
    if (curTitle.isEmpty()) curTitle = "No song playing";
    if (curArtist.isEmpty()) curArtist = "-";

    std::string playStateIcon = m_player->isPlaying() ? (greenText + "[ > Playing ]" + reset) : (darkGray + "[ || Paused ]" + reset);
    std::string songInfo = bold + whiteText + curTitle.toStdString() + reset + grayText + " | " + curArtist.toStdString() + reset;

    printLine(playStateIcon + " " + songInfo);

    qint64 pos = m_player->position();
    qint64 dur = m_player->duration();
    QString timeStr = QString("%1 / %2")
        .arg(m_player->formatTime(pos))
        .arg(m_player->formatTime(dur));

    int barWidth = std::max(10, contentWidth - 45);
    QString pBar = QString("[%1]").arg(buildProgressBar(pos, dur, barWidth));

    int volPercent = static_cast<int>(m_player->volume() * 100);
    QString volStr = m_player->isMuted() ? "MUTED" : QString("Vol %1%").arg(volPercent);

    QString shufStr = m_player->isShuffle() ? "[SHUF: ON]" : "[SHUF: OFF]";
    QString repStr;
    if (m_player->repeatMode() == 1) repStr = "[REP: ALL]";
    else if (m_player->repeatMode() == 2) repStr = "[REP: ONE]";
    else repStr = "[REP: OFF]";

    QString playerControlLine = QString("%1 %2  %3  %4 %5")
        .arg(timeStr)
        .arg(pBar)
        .arg(volStr)
        .arg(shufStr)
        .arg(repStr);

    printLine(whiteText + padRight(playerControlLine, contentWidth - 1).toStdString() + reset);

    printDivider();

    if (!m_statusMessage.isEmpty()) {
        printLine(greenText + bold + padRight(m_statusMessage, contentWidth - 1).toStdString() + reset);
    } else if (m_inputMode == ModeSearch) {
        QString sLine = QString("Search > %1_").arg(m_inputBuffer);
        printLine(bold + whiteText + padRight(sLine, contentWidth - 1).toStdString() + reset);
    } else if (m_inputMode == ModeImport) {
        QString iLine = QString("Import Path > %1_ (Press Enter to import, Esc to cancel)").arg(m_inputBuffer);
        printLine(bold + whiteText + padRight(iLine, contentWidth - 1).toStdString() + reset);
    } else if (m_inputMode == ModeCreatePlaylist) {
        QString cLine = QString("New Playlist Name > %1_ (Press Enter to create, Esc to cancel)").arg(m_inputBuffer);
        printLine(bold + whiteText + padRight(cLine, contentWidth - 1).toStdString() + reset);
    } else {
        std::string keyHints = "[Space] Play/Pause  [k/j] Navigate  [Enter] Play  [n/p] Next/Prev  [+/-] Vol  [i] Import  [/] Search  [q] Quit";
        printLine(darkGray + padRight(QString::fromStdString(keyHints), contentWidth - 1).toStdString() + reset);
    }

    out << borderCol << "+-" << std::string(contentWidth, '-') << "-+\033[K\033[J" << reset;

    std::cout << out.str() << std::flush;
}
