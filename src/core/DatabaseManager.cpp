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

#include "DatabaseManager.h"
#include <QStandardPaths>
#include <QDir>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QDateTime>
#include <QMutexLocker>

DatabaseManager &DatabaseManager::instance()
{
    static DatabaseManager instance;
    return instance;
}

DatabaseManager::DatabaseManager(QObject *parent)
    : QObject(parent)
{
}

DatabaseManager::~DatabaseManager()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
}

bool DatabaseManager::initialize(const QString &customDbPath)
{
    QMutexLocker locker(&m_mutex);
    if (m_initialized) {
        return true;
    }

    QString dbPath = customDbPath;
    if (dbPath.isEmpty()) {
        QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(appDataDir);
        dbPath = appDataDir + "/spotsong.db";
    }

    m_db = QSqlDatabase::addDatabase("QSQLITE", "SpotSongConnection");
    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        return false;
    }

    QSqlQuery query(m_db);
    query.exec("PRAGMA foreign_keys = ON;");
    query.exec("PRAGMA journal_mode = WAL;");

    query.exec(
        "CREATE TABLE IF NOT EXISTS songs ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "file_path TEXT UNIQUE NOT NULL, "
        "title TEXT NOT NULL, "
        "artist TEXT, "
        "album TEXT, "
        "duration_ms INTEGER DEFAULT 0, "
        "cover_path TEXT, "
        "play_count INTEGER DEFAULT 0, "
        "added_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");"
    );

    query.exec(
        "CREATE TABLE IF NOT EXISTS playlists ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "name TEXT UNIQUE NOT NULL, "
        "cover_path TEXT, "
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");"
    );

    query.exec(
        "CREATE TABLE IF NOT EXISTS playlist_songs ("
        "playlist_id INTEGER NOT NULL, "
        "song_id INTEGER NOT NULL, "
        "position INTEGER NOT NULL, "
        "added_at DATETIME DEFAULT CURRENT_TIMESTAMP, "
        "PRIMARY KEY (playlist_id, song_id), "
        "FOREIGN KEY(playlist_id) REFERENCES playlists(id) ON DELETE CASCADE, "
        "FOREIGN KEY(song_id) REFERENCES songs(id) ON DELETE CASCADE"
        ");"
    );

    query.exec(
        "CREATE TABLE IF NOT EXISTS settings ("
        "key TEXT PRIMARY KEY, "
        "value TEXT"
        ");"
    );

    m_initialized = true;
    return true;
}

int DatabaseManager::insertOrUpdateSong(const SongRecord &song)
{
    QMutexLocker locker(&m_mutex);
    if (!m_initialized) return -1;

    QSqlQuery checkQuery(m_db);
    checkQuery.prepare("SELECT id, play_count FROM songs WHERE file_path = :file_path;");
    checkQuery.bindValue(":file_path", song.filePath);

    if (checkQuery.exec() && checkQuery.next()) {
        int existingId = checkQuery.value("id").toInt();
        QSqlQuery updateQuery(m_db);
        updateQuery.prepare(
            "UPDATE songs SET "
            "title = :title, "
            "artist = :artist, "
            "album = :album, "
            "duration_ms = :duration_ms, "
            "cover_path = :cover_path "
            "WHERE id = :id;"
        );
        updateQuery.bindValue(":title", song.title);
        updateQuery.bindValue(":artist", song.artist);
        updateQuery.bindValue(":album", song.album);
        updateQuery.bindValue(":duration_ms", song.durationMs);
        updateQuery.bindValue(":cover_path", song.coverPath);
        updateQuery.bindValue(":id", existingId);
        updateQuery.exec();
        return existingId;
    }

    QSqlQuery insertQuery(m_db);
    insertQuery.prepare(
        "INSERT INTO songs (file_path, title, artist, album, duration_ms, cover_path, play_count) "
        "VALUES (:file_path, :title, :artist, :album, :duration_ms, :cover_path, :play_count);"
    );
    insertQuery.bindValue(":file_path", song.filePath);
    insertQuery.bindValue(":title", song.title.isEmpty() ? "Unknown Title" : song.title);
    insertQuery.bindValue(":artist", song.artist.isEmpty() ? "Unknown Artist" : song.artist);
    insertQuery.bindValue(":album", song.album.isEmpty() ? "Unknown Album" : song.album);
    insertQuery.bindValue(":duration_ms", song.durationMs);
    insertQuery.bindValue(":cover_path", song.coverPath);
    insertQuery.bindValue(":play_count", song.playCount);

    if (insertQuery.exec()) {
        int newId = insertQuery.lastInsertId().toInt();
        emit databaseChanged();
        return newId;
    }

    return -1;
}

bool DatabaseManager::updateSongMetadata(int id, const QString &title, const QString &artist, const QString &album, const QString &coverPath)
{
    QMutexLocker locker(&m_mutex);
    if (!m_initialized) return false;

    QSqlQuery query(m_db);
    query.prepare(
        "UPDATE songs SET "
        "title = :title, "
        "artist = :artist, "
        "album = :album, "
        "cover_path = :cover_path "
        "WHERE id = :id;"
    );
    query.bindValue(":title", title);
    query.bindValue(":artist", artist);
    query.bindValue(":album", album);
    query.bindValue(":cover_path", coverPath);
    query.bindValue(":id", id);

    bool ok = query.exec();
    if (ok) {
        emit songUpdated(id);
        emit databaseChanged();
    }
    return ok;
}

bool DatabaseManager::deleteSong(int id)
{
    QMutexLocker locker(&m_mutex);
    if (!m_initialized) return false;

    QSqlQuery query(m_db);
    query.prepare("DELETE FROM songs WHERE id = :id;");
    query.bindValue(":id", id);
    bool ok = query.exec();
    if (ok) {
        emit databaseChanged();
    }
    return ok;
}

bool DatabaseManager::incrementPlayCount(int songId)
{
    QMutexLocker locker(&m_mutex);
    if (!m_initialized) return false;

    QSqlQuery query(m_db);
    query.prepare("UPDATE songs SET play_count = play_count + 1 WHERE id = :id;");
    query.bindValue(":id", songId);
    bool ok = query.exec();
    if (ok) {
        emit songUpdated(songId);
        emit databaseChanged();
    }
    return ok;
}

SongRecord DatabaseManager::getSongById(int id)
{
    QMutexLocker locker(&m_mutex);
    SongRecord song;
    if (!m_initialized) return song;

    QSqlQuery query(m_db);
    query.prepare("SELECT * FROM songs WHERE id = :id;");
    query.bindValue(":id", id);

    if (query.exec() && query.next()) {
        song.id = query.value("id").toInt();
        song.filePath = query.value("file_path").toString();
        song.title = query.value("title").toString();
        song.artist = query.value("artist").toString();
        song.album = query.value("album").toString();
        song.durationMs = query.value("duration_ms").toLongLong();
        song.coverPath = query.value("cover_path").toString();
        song.playCount = query.value("play_count").toInt();
        song.addedAt = query.value("added_at").toString();
    }
    return song;
}

SongRecord DatabaseManager::getSongByPath(const QString &filePath)
{
    QMutexLocker locker(&m_mutex);
    SongRecord song;
    if (!m_initialized) return song;

    QSqlQuery query(m_db);
    query.prepare("SELECT * FROM songs WHERE file_path = :file_path;");
    query.bindValue(":file_path", filePath);

    if (query.exec() && query.next()) {
        song.id = query.value("id").toInt();
        song.filePath = query.value("file_path").toString();
        song.title = query.value("title").toString();
        song.artist = query.value("artist").toString();
        song.album = query.value("album").toString();
        song.durationMs = query.value("duration_ms").toLongLong();
        song.coverPath = query.value("cover_path").toString();
        song.playCount = query.value("play_count").toInt();
        song.addedAt = query.value("added_at").toString();
    }
    return song;
}

QList<SongRecord> DatabaseManager::getAllSongs(const QString &searchQuery, const QString &sortBy, bool ascending)
{
    QMutexLocker locker(&m_mutex);
    QList<SongRecord> list;
    if (!m_initialized) return list;

    QString orderColumn = "title";
    if (sortBy == "artist") orderColumn = "artist";
    else if (sortBy == "album") orderColumn = "album";
    else if (sortBy == "duration") orderColumn = "duration_ms";
    else if (sortBy == "play_count") orderColumn = "play_count";
    else if (sortBy == "added_at") orderColumn = "added_at";

    QString sql = "SELECT * FROM songs";
    if (!searchQuery.trimmed().isEmpty()) {
        sql += " WHERE title LIKE :q OR artist LIKE :q OR album LIKE :q";
    }
    sql += QString(" ORDER BY %1 COLLATE NOCASE %2;").arg(orderColumn, ascending ? "ASC" : "DESC");

    QSqlQuery query(m_db);
    query.prepare(sql);
    if (!searchQuery.trimmed().isEmpty()) {
        query.bindValue(":q", QString("%%1%").arg(searchQuery.trimmed()));
    }

    if (query.exec()) {
        while (query.next()) {
            SongRecord song;
            song.id = query.value("id").toInt();
            song.filePath = query.value("file_path").toString();
            song.title = query.value("title").toString();
            song.artist = query.value("artist").toString();
            song.album = query.value("album").toString();
            song.durationMs = query.value("duration_ms").toLongLong();
            song.coverPath = query.value("cover_path").toString();
            song.playCount = query.value("play_count").toInt();
            song.addedAt = query.value("added_at").toString();
            list.append(song);
        }
    }
    return list;
}

QList<SongRecord> DatabaseManager::getMostPlayedSongs(int limit)
{
    QMutexLocker locker(&m_mutex);
    QList<SongRecord> list;
    if (!m_initialized) return list;

    QSqlQuery query(m_db);
    query.prepare("SELECT * FROM songs ORDER BY play_count DESC, id DESC LIMIT :limit;");
    query.bindValue(":limit", limit);

    if (query.exec()) {
        while (query.next()) {
            SongRecord song;
            song.id = query.value("id").toInt();
            song.filePath = query.value("file_path").toString();
            song.title = query.value("title").toString();
            song.artist = query.value("artist").toString();
            song.album = query.value("album").toString();
            song.durationMs = query.value("duration_ms").toLongLong();
            song.coverPath = query.value("cover_path").toString();
            song.playCount = query.value("play_count").toInt();
            song.addedAt = query.value("added_at").toString();
            list.append(song);
        }
    }
    return list;
}

int DatabaseManager::createPlaylist(const QString &name, const QString &coverPath)
{
    QMutexLocker locker(&m_mutex);
    if (!m_initialized || name.trimmed().isEmpty()) return -1;

    QSqlQuery query(m_db);
    query.prepare("INSERT INTO playlists (name, cover_path) VALUES (:name, :cover_path);");
    query.bindValue(":name", name.trimmed());
    query.bindValue(":cover_path", coverPath);

    if (query.exec()) {
        int id = query.lastInsertId().toInt();
        emit databaseChanged();
        return id;
    }
    return -1;
}

bool DatabaseManager::renamePlaylist(int playlistId, const QString &newName)
{
    QMutexLocker locker(&m_mutex);
    if (!m_initialized || newName.trimmed().isEmpty()) return false;

    QSqlQuery query(m_db);
    query.prepare("UPDATE playlists SET name = :name WHERE id = :id;");
    query.bindValue(":name", newName.trimmed());
    query.bindValue(":id", playlistId);

    bool ok = query.exec();
    if (ok) {
        emit playlistChanged(playlistId);
        emit databaseChanged();
    }
    return ok;
}

bool DatabaseManager::deletePlaylist(int playlistId)
{
    QMutexLocker locker(&m_mutex);
    if (!m_initialized) return false;

    QSqlQuery query(m_db);
    query.prepare("DELETE FROM playlists WHERE id = :id;");
    query.bindValue(":id", playlistId);

    bool ok = query.exec();
    if (ok) {
        emit databaseChanged();
    }
    return ok;
}

QList<PlaylistRecord> DatabaseManager::getAllPlaylists()
{
    QMutexLocker locker(&m_mutex);
    QList<PlaylistRecord> list;
    if (!m_initialized) return list;

    QSqlQuery query(m_db);
    query.prepare(
        "SELECT p.id, p.name, p.cover_path, p.created_at, COUNT(ps.song_id) AS song_count "
        "FROM playlists p "
        "LEFT JOIN playlist_songs ps ON p.id = ps.playlist_id "
        "GROUP BY p.id "
        "ORDER BY p.name COLLATE NOCASE ASC;"
    );

    if (query.exec()) {
        while (query.next()) {
            PlaylistRecord pl;
            pl.id = query.value("id").toInt();
            pl.name = query.value("name").toString();
            pl.coverPath = query.value("cover_path").toString();
            pl.createdAt = query.value("created_at").toString();
            pl.songCount = query.value("song_count").toInt();
            list.append(pl);
        }
    }
    return list;
}

PlaylistRecord DatabaseManager::getPlaylistById(int playlistId)
{
    QMutexLocker locker(&m_mutex);
    PlaylistRecord pl;
    if (!m_initialized) return pl;

    QSqlQuery query(m_db);
    query.prepare(
        "SELECT p.id, p.name, p.cover_path, p.created_at, COUNT(ps.song_id) AS song_count "
        "FROM playlists p "
        "LEFT JOIN playlist_songs ps ON p.id = ps.playlist_id "
        "WHERE p.id = :id "
        "GROUP BY p.id;"
    );
    query.bindValue(":id", playlistId);

    if (query.exec() && query.next()) {
        pl.id = query.value("id").toInt();
        pl.name = query.value("name").toString();
        pl.coverPath = query.value("cover_path").toString();
        pl.createdAt = query.value("created_at").toString();
        pl.songCount = query.value("song_count").toInt();
    }
    return pl;
}

bool DatabaseManager::addSongToPlaylist(int playlistId, int songId)
{
    QMutexLocker locker(&m_mutex);
    if (!m_initialized) return false;

    QSqlQuery maxPosQuery(m_db);
    maxPosQuery.prepare("SELECT COALESCE(MAX(position), -1) + 1 AS next_pos FROM playlist_songs WHERE playlist_id = :p_id;");
    maxPosQuery.bindValue(":p_id", playlistId);
    
    int nextPos = 0;
    if (maxPosQuery.exec() && maxPosQuery.next()) {
        nextPos = maxPosQuery.value("next_pos").toInt();
    }

    QSqlQuery insertQuery(m_db);
    insertQuery.prepare(
        "INSERT OR IGNORE INTO playlist_songs (playlist_id, song_id, position) "
        "VALUES (:playlist_id, :song_id, :position);"
    );
    insertQuery.bindValue(":playlist_id", playlistId);
    insertQuery.bindValue(":song_id", songId);
    insertQuery.bindValue(":position", nextPos);

    bool ok = insertQuery.exec();
    if (ok) {
        emit playlistChanged(playlistId);
        emit databaseChanged();
    }
    return ok;
}

bool DatabaseManager::removeSongFromPlaylist(int playlistId, int songId)
{
    QMutexLocker locker(&m_mutex);
    if (!m_initialized) return false;

    QSqlQuery query(m_db);
    query.prepare("DELETE FROM playlist_songs WHERE playlist_id = :playlist_id AND song_id = :song_id;");
    query.bindValue(":playlist_id", playlistId);
    query.bindValue(":song_id", songId);

    bool ok = query.exec();
    if (ok) {
        emit playlistChanged(playlistId);
        emit databaseChanged();
    }
    return ok;
}

bool DatabaseManager::reorderPlaylistSongs(int playlistId, const QList<int> &songIds)
{
    QMutexLocker locker(&m_mutex);
    if (!m_initialized) return false;

    m_db.transaction();
    QSqlQuery query(m_db);
    query.prepare("UPDATE playlist_songs SET position = :pos WHERE playlist_id = :playlist_id AND song_id = :song_id;");

    for (int i = 0; i < songIds.size(); ++i) {
        query.bindValue(":pos", i);
        query.bindValue(":playlist_id", playlistId);
        query.bindValue(":song_id", songIds.at(i));
        query.exec();
    }

    bool ok = m_db.commit();
    if (ok) {
        emit playlistChanged(playlistId);
    }
    return ok;
}

QList<SongRecord> DatabaseManager::getPlaylistSongs(int playlistId)
{
    QMutexLocker locker(&m_mutex);
    QList<SongRecord> list;
    if (!m_initialized) return list;

    QSqlQuery query(m_db);
    query.prepare(
        "SELECT s.* FROM songs s "
        "INNER JOIN playlist_songs ps ON s.id = ps.song_id "
        "WHERE ps.playlist_id = :playlist_id "
        "ORDER BY ps.position ASC;"
    );
    query.bindValue(":playlist_id", playlistId);

    if (query.exec()) {
        while (query.next()) {
            SongRecord song;
            song.id = query.value("id").toInt();
            song.filePath = query.value("file_path").toString();
            song.title = query.value("title").toString();
            song.artist = query.value("artist").toString();
            song.album = query.value("album").toString();
            song.durationMs = query.value("duration_ms").toLongLong();
            song.coverPath = query.value("cover_path").toString();
            song.playCount = query.value("play_count").toInt();
            song.addedAt = query.value("added_at").toString();
            list.append(song);
        }
    }
    return list;
}

bool DatabaseManager::isSongInPlaylist(int playlistId, int songId)
{
    QMutexLocker locker(&m_mutex);
    if (!m_initialized) return false;

    QSqlQuery query(m_db);
    query.prepare("SELECT 1 FROM playlist_songs WHERE playlist_id = :p_id AND song_id = :s_id LIMIT 1;");
    query.bindValue(":p_id", playlistId);
    query.bindValue(":s_id", songId);

    return query.exec() && query.next();
}

void DatabaseManager::setSetting(const QString &key, const QString &value)
{
    QMutexLocker locker(&m_mutex);
    if (!m_initialized) return;

    QSqlQuery query(m_db);
    query.prepare("INSERT OR REPLACE INTO settings (key, value) VALUES (:key, :value);");
    query.bindValue(":key", key);
    query.bindValue(":value", value);
    query.exec();
}

QString DatabaseManager::getSetting(const QString &key, const QString &defaultValue)
{
    QMutexLocker locker(&m_mutex);
    if (!m_initialized) return defaultValue;

    QSqlQuery query(m_db);
    query.prepare("SELECT value FROM settings WHERE key = :key;");
    query.bindValue(":key", key);

    if (query.exec() && query.next()) {
        return query.value("value").toString();
    }
    return defaultValue;
}
