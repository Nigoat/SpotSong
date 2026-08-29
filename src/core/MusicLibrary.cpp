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

#include "MusicLibrary.h"
#include "TagReader.h"
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QStandardPaths>
#include <QCryptographicHash>
#include <QImage>

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

MusicLibrary::MusicLibrary(QObject *parent)
    : QAbstractListModel(parent)
{
    connect(&DatabaseManager::instance(), &DatabaseManager::databaseChanged, this, &MusicLibrary::refresh);
    refresh();
}

MusicLibrary::~MusicLibrary()
{
}

int MusicLibrary::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_songs.size();
}

QVariant MusicLibrary::data(const QModelIndex &index, int role) const
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
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> MusicLibrary::roleNames() const
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
    return roles;
}

int MusicLibrary::count() const
{
    return m_songs.size();
}

QString MusicLibrary::searchQuery() const
{
    return m_searchQuery;
}

QString MusicLibrary::sortBy() const
{
    return m_sortBy;
}

bool MusicLibrary::sortAscending() const
{
    return m_sortAscending;
}

bool MusicLibrary::isImporting() const
{
    return m_isImporting;
}

void MusicLibrary::setSearchQuery(const QString &query)
{
    if (m_searchQuery != query) {
        m_searchQuery = query;
        emit searchQueryChanged();
        refresh();
    }
}

void MusicLibrary::setSortBy(const QString &sortBy)
{
    if (m_sortBy != sortBy) {
        m_sortBy = sortBy;
        emit sortByChanged();
        refresh();
    }
}

void MusicLibrary::setSortAscending(bool ascending)
{
    if (m_sortAscending != ascending) {
        m_sortAscending = ascending;
        emit sortAscendingChanged();
        refresh();
    }
}

void MusicLibrary::refresh()
{
    beginResetModel();
    m_songs = DatabaseManager::instance().getAllSongs(m_searchQuery, m_sortBy, m_sortAscending);
    endResetModel();
    emit countChanged();
}

void MusicLibrary::importFiles(const QList<QUrl> &urls)
{
    QStringList paths;
    for (const auto &url : urls) {
        paths.append(url.toLocalFile());
    }
    importFilesFromPaths(paths);
}

void MusicLibrary::importFilesFromPaths(const QStringList &filePaths)
{
    m_isImporting = true;
    emit isImportingChanged();

    int importedCount = 0;
    for (const auto &path : filePaths) {
        QFileInfo info(path);
        if (info.isDir()) {
            scanDirectory(path, importedCount);
        } else if (info.isFile()) {
            importSinglePath(path, importedCount);
        }
    }

    m_isImporting = false;
    emit isImportingChanged();
    emit importFinished(importedCount);

    refresh();
}

void MusicLibrary::importFolder(const QUrl &folderUrl)
{
    QString path = folderUrl.toLocalFile();
    if (path.isEmpty()) return;

    m_isImporting = true;
    emit isImportingChanged();

    int importedCount = 0;
    scanDirectory(path, importedCount);

    m_isImporting = false;
    emit isImportingChanged();
    emit importFinished(importedCount);

    refresh();
}

void MusicLibrary::importSinglePath(const QString &path, int &count)
{
    QStringList validExts = {"mp3", "flac", "ogg", "wav", "m4a", "aac", "opus", "wma"};
    QString ext = QFileInfo(path).suffix().toLower();
    if (!validExts.contains(ext)) {
        return;
    }

    SongRecord record = TagReader::readMetadata(path);
    if (record.filePath.isEmpty()) {
        return;
    }

    int id = DatabaseManager::instance().insertOrUpdateSong(record);
    if (id > 0) {
        count++;
    }
}

void MusicLibrary::scanDirectory(const QString &dirPath, int &count)
{
    QStringList validFilters = {"*.mp3", "*.flac", "*.ogg", "*.wav", "*.m4a", "*.aac", "*.opus", "*.wma"};
    QDirIterator it(dirPath, validFilters, QDir::Files, QDirIterator::Subdirectories);

    while (it.hasNext()) {
        QString filePath = it.next();
        importSinglePath(filePath, count);
    }
}

void MusicLibrary::deleteSong(int songId)
{
    DatabaseManager::instance().deleteSong(songId);
}

bool MusicLibrary::updateSong(int songId, const QString &title, const QString &artist, const QString &album, const QString &coverPath)
{
    SongRecord current = DatabaseManager::instance().getSongById(songId);
    if (current.id == -1) return false;

    QString cleanCoverPath = coverPath;
    if (cleanCoverPath.startsWith("file://")) {
        cleanCoverPath = QUrl(cleanCoverPath).toLocalFile();
    }

    bool dbOk = DatabaseManager::instance().updateSongMetadata(songId, title, artist, album, cleanCoverPath);
    TagReader::writeMetadata(current.filePath, title, artist, album);
    return dbOk;
}

QString MusicLibrary::setCustomCoverImage(int songId, const QUrl &imageFileUrl)
{
    QString localImagePath = imageFileUrl.toLocalFile();
    if (localImagePath.isEmpty()) return QString();

    QImage img(localImagePath);
    if (img.isNull()) return QString();

    QString coversDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/covers";
    QDir().mkpath(coversDir);

    QByteArray hash = QCryptographicHash::hash(localImagePath.toUtf8() + QByteArray::number(songId), QCryptographicHash::Md5).toHex();
    QString targetCoverPath = coversDir + "/custom_" + QString::fromLatin1(hash) + ".jpg";

    img.scaled(500, 500, Qt::KeepAspectRatio, Qt::SmoothTransformation).save(targetCoverPath, "JPG", 90);

    SongRecord current = DatabaseManager::instance().getSongById(songId);
    if (current.id != -1) {
        DatabaseManager::instance().updateSongMetadata(songId, current.title, current.artist, current.album, targetCoverPath);
    }

    return targetCoverPath;
}

QVariantList MusicLibrary::getMostPlayedSongs(int limit)
{
    QVariantList list;
    QList<SongRecord> songs = DatabaseManager::instance().getMostPlayedSongs(limit);
    for (const auto &song : songs) {
        list.append(song.toMap());
    }
    return list;
}

QVariantList MusicLibrary::getAllSongsList()
{
    QVariantList list;
    for (const auto &song : m_songs) {
        list.append(song.toMap());
    }
    return list;
}

QVariantMap MusicLibrary::getSong(int songId)
{
    return DatabaseManager::instance().getSongById(songId).toMap();
}
