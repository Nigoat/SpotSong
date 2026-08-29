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

#include "PlaylistManager.h"
#include <QUrl>

static QString formatDuration(qint64 ms)
{
    qint64 totalSeconds = ms / 1000;
    qint64 seconds = totalSeconds % 60;
    qint64 minutes = (totalSeconds / 60) % 60;
    qint64 hours = totalSeconds / 3600;

    if (hours > 0) {
        return QString("%1:%2:%3")
            .arg(hours)
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'));
    }
    return QString("%1:%2")
        .arg(minutes)
        .arg(seconds, 2, 10, QChar('0'));
}

PlaylistModel::PlaylistModel(QObject *parent)
    : QAbstractListModel(parent)
{
    connect(&DatabaseManager::instance(), &DatabaseManager::databaseChanged, this, &PlaylistModel::refresh);
    refresh();
}

int PlaylistModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_playlists.size();
}

QVariant PlaylistModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_playlists.size()) {
        return QVariant();
    }

    const PlaylistRecord &pl = m_playlists.at(index.row());

    switch (role) {
    case IdRole:
        return pl.id;
    case NameRole:
        return pl.name;
    case CoverPathRole:
        return pl.coverPath.isEmpty() ? QString() : QUrl::fromLocalFile(pl.coverPath).toString();
    case SongCountRole:
        return pl.songCount;
    case CreatedAtRole:
        return pl.createdAt;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> PlaylistModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IdRole] = "id";
    roles[NameRole] = "name";
    roles[CoverPathRole] = "coverPath";
    roles[SongCountRole] = "songCount";
    roles[CreatedAtRole] = "createdAt";
    return roles;
}

int PlaylistModel::count() const
{
    return m_playlists.size();
}

void PlaylistModel::refresh()
{
    beginResetModel();
    m_playlists = DatabaseManager::instance().getAllPlaylists();
    endResetModel();
    emit countChanged();
}

PlaylistSongsModel::PlaylistSongsModel(QObject *parent)
    : QAbstractListModel(parent)
{
    connect(&DatabaseManager::instance(), &DatabaseManager::playlistChanged, this, [this](int id) {
        if (m_playlistId == id) {
            refresh();
        }
    });
}

int PlaylistSongsModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_songs.size();
}

QVariant PlaylistSongsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_songs.size()) {
        return QVariant();
    }

    const SongRecord &song = m_songs.at(index.row());

    switch (role) {
    case IdRole:
        return song.id;
    case FilePathRole:
        return song.filePath;
    case TitleRole:
        return song.title;
    case ArtistRole:
        return song.artist;
    case AlbumRole:
        return song.album;
    case DurationMsRole:
        return song.durationMs;
    case DurationFormattedRole:
        return formatDuration(song.durationMs);
    case CoverPathRole:
        return song.coverPath.isEmpty() ? QString() : QUrl::fromLocalFile(song.coverPath).toString();
    case PlayCountRole:
        return song.playCount;
    case AddedAtRole:
        return song.addedAt;
    case TrackNumberRole:
        return index.row() + 1;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> PlaylistSongsModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IdRole] = "id";
    roles[FilePathRole] = "filePath";
    roles[TitleRole] = "title";
    roles[ArtistRole] = "artist";
    roles[AlbumRole] = "album";
    roles[DurationMsRole] = "durationMs";
    roles[DurationFormattedRole] = "durationFormatted";
    roles[CoverPathRole] = "coverPath";
    roles[PlayCountRole] = "playCount";
    roles[AddedAtRole] = "addedAt";
    roles[TrackNumberRole] = "trackNumber";
    return roles;
}

int PlaylistSongsModel::playlistId() const
{
    return m_playlistId;
}

QString PlaylistSongsModel::playlistName() const
{
    return m_playlistName;
}

int PlaylistSongsModel::count() const
{
    return m_songs.size();
}

void PlaylistSongsModel::setPlaylistId(int id)
{
    if (m_playlistId != id) {
        m_playlistId = id;
        PlaylistRecord pl = DatabaseManager::instance().getPlaylistById(id);
        m_playlistName = pl.name;
        emit playlistIdChanged();
        emit playlistNameChanged();
        refresh();
    }
}

void PlaylistSongsModel::refresh()
{
    beginResetModel();
    if (m_playlistId > 0) {
        m_songs = DatabaseManager::instance().getPlaylistSongs(m_playlistId);
        PlaylistRecord pl = DatabaseManager::instance().getPlaylistById(m_playlistId);
        m_playlistName = pl.name;
        emit playlistNameChanged();
    } else {
        m_songs.clear();
        m_playlistName.clear();
        emit playlistNameChanged();
    }
    endResetModel();
    emit countChanged();
}

void PlaylistSongsModel::removeSong(int songId)
{
    if (m_playlistId <= 0) return;
    DatabaseManager::instance().removeSongFromPlaylist(m_playlistId, songId);
}

void PlaylistSongsModel::moveSong(int fromIndex, int toIndex)
{
    if (fromIndex < 0 || fromIndex >= m_songs.size() || toIndex < 0 || toIndex >= m_songs.size() || fromIndex == toIndex) {
        return;
    }

    beginResetModel();
    m_songs.move(fromIndex, toIndex);
    QList<int> ids;
    for (const auto &s : m_songs) {
        ids.append(s.id);
    }
    DatabaseManager::instance().reorderPlaylistSongs(m_playlistId, ids);
    endResetModel();
}

QVariantList PlaylistSongsModel::getSongsList()
{
    QVariantList list;
    for (const auto &song : m_songs) {
        list.append(song.toMap());
    }
    return list;
}

PlaylistManager::PlaylistManager(QObject *parent)
    : QObject(parent)
    , m_playlistsModel(new PlaylistModel(this))
    , m_activePlaylistModel(new PlaylistSongsModel(this))
{
}

PlaylistManager::~PlaylistManager()
{
}

PlaylistModel* PlaylistManager::playlistsModel() const
{
    return m_playlistsModel;
}

PlaylistSongsModel* PlaylistManager::activePlaylistModel() const
{
    return m_activePlaylistModel;
}

int PlaylistManager::createPlaylist(const QString &name)
{
    int id = DatabaseManager::instance().createPlaylist(name);
    if (id > 0) {
        emit playlistCreated(id);
    }
    return id;
}

bool PlaylistManager::renamePlaylist(int playlistId, const QString &newName)
{
    return DatabaseManager::instance().renamePlaylist(playlistId, newName);
}

bool PlaylistManager::deletePlaylist(int playlistId)
{
    bool ok = DatabaseManager::instance().deletePlaylist(playlistId);
    if (ok) {
        if (m_activePlaylistModel->playlistId() == playlistId) {
            m_activePlaylistModel->setPlaylistId(-1);
        }
        emit playlistDeleted(playlistId);
    }
    return ok;
}

bool PlaylistManager::addSongToPlaylist(int playlistId, int songId)
{
    return DatabaseManager::instance().addSongToPlaylist(playlistId, songId);
}

bool PlaylistManager::removeSongFromPlaylist(int playlistId, int songId)
{
    return DatabaseManager::instance().removeSongFromPlaylist(playlistId, songId);
}

bool PlaylistManager::isSongInPlaylist(int playlistId, int songId)
{
    return DatabaseManager::instance().isSongInPlaylist(playlistId, songId);
}

void PlaylistManager::openPlaylist(int playlistId)
{
    m_activePlaylistModel->setPlaylistId(playlistId);
    emit playlistOpened(playlistId);
}

QVariantList PlaylistManager::getAllPlaylists()
{
    QVariantList list;
    QList<PlaylistRecord> pls = DatabaseManager::instance().getAllPlaylists();
    for (const auto &pl : pls) {
        list.append(pl.toMap());
    }
    return list;
}

QVariantMap PlaylistManager::getPlaylist(int playlistId)
{
    return DatabaseManager::instance().getPlaylistById(playlistId).toMap();
}
