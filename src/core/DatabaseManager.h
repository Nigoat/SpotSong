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
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QSqlDatabase>
#include <QRecursiveMutex>

struct SongRecord {
    int id = -1;
    QString filePath;
    QString title;
    QString artist;
    QString album;
    qint64 durationMs = 0;
    QString coverPath;
    int playCount = 0;
    QString addedAt;

    QVariantMap toMap() const {
        QVariantMap map;
        map["id"] = id;
        map["filePath"] = filePath;
        map["title"] = title;
        map["artist"] = artist;
        map["album"] = album;
        map["durationMs"] = durationMs;
        map["coverPath"] = coverPath;
        map["playCount"] = playCount;
        map["addedAt"] = addedAt;
        return map;
    }
};

struct PlaylistRecord {
    int id = -1;
    QString name;
    QString coverPath;
    int songCount = 0;
    QString createdAt;

    QVariantMap toMap() const {
        QVariantMap map;
        map["id"] = id;
        map["name"] = name;
        map["coverPath"] = coverPath;
        map["songCount"] = songCount;
        map["createdAt"] = createdAt;
        return map;
    }
};

class DatabaseManager : public QObject
{
    Q_OBJECT

public:
    static DatabaseManager &instance();

    bool initialize(const QString &customDbPath = QString());
    
    int insertOrUpdateSong(const SongRecord &song);
    bool updateSongMetadata(int id, const QString &title, const QString &artist, const QString &album, const QString &coverPath);
    bool deleteSong(int id);
    bool incrementPlayCount(int songId);
    
    SongRecord getSongById(int id);
    SongRecord getSongByPath(const QString &filePath);
    QList<SongRecord> getAllSongs(const QString &searchQuery = QString(), const QString &sortBy = "title", bool ascending = true);
    QList<SongRecord> getMostPlayedSongs(int limit = 10);

    int createPlaylist(const QString &name, const QString &coverPath = QString());
    bool renamePlaylist(int playlistId, const QString &newName);
    bool deletePlaylist(int playlistId);
    QList<PlaylistRecord> getAllPlaylists();
    PlaylistRecord getPlaylistById(int playlistId);

    bool addSongToPlaylist(int playlistId, int songId);
    bool removeSongFromPlaylist(int playlistId, int songId);
    bool reorderPlaylistSongs(int playlistId, const QList<int> &songIds);
    QList<SongRecord> getPlaylistSongs(int playlistId);
    bool isSongInPlaylist(int playlistId, int songId);

    void setSetting(const QString &key, const QString &value);
    QString getSetting(const QString &key, const QString &defaultValue = QString());

signals:
    void databaseChanged();
    void playlistChanged(int playlistId);
    void songUpdated(int songId);

private:
    explicit DatabaseManager(QObject *parent = nullptr);
    ~DatabaseManager() override;
    DatabaseManager(const DatabaseManager &) = delete;
    DatabaseManager &operator=(const DatabaseManager &) = delete;

    QSqlDatabase m_db;
    QRecursiveMutex m_mutex;
    bool m_initialized = false;
};
