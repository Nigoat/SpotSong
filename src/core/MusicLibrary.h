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

#include <QAbstractListModel>
#include <QVariantList>
#include <QVariantMap>
#include <QStringList>
#include <QUrl>
#include "DatabaseManager.h"

class MusicLibrary : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QString searchQuery READ searchQuery WRITE setSearchQuery NOTIFY searchQueryChanged)
    Q_PROPERTY(QString sortBy READ sortBy WRITE setSortBy NOTIFY sortByChanged)
    Q_PROPERTY(bool sortAscending READ sortAscending WRITE setSortAscending NOTIFY sortAscendingChanged)
    Q_PROPERTY(bool isImporting READ isImporting NOTIFY isImportingChanged)

public:
    enum SongRoles {
        IdRole = Qt::UserRole + 1,
        FilePathRole,
        TitleRole,
        ArtistRole,
        AlbumRole,
        DurationMsRole,
        DurationFormattedRole,
        CoverPathRole,
        PlayCountRole,
        AddedAtRole
    };
    Q_ENUM(SongRoles)

    explicit MusicLibrary(QObject *parent = nullptr);
    ~MusicLibrary() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const;
    QString searchQuery() const;
    QString sortBy() const;
    bool sortAscending() const;
    bool isImporting() const;

public slots:
    void setSearchQuery(const QString &query);
    void setSortBy(const QString &sortBy);
    void setSortAscending(bool ascending);
    void refresh();

    void importFiles(const QList<QUrl> &urls);
    void importFilesFromPaths(const QStringList &filePaths);
    void importFolder(const QUrl &folderUrl);
    void deleteSong(int songId);
    bool updateSong(int songId, const QString &title, const QString &artist, const QString &album, const QString &coverPath);
    QString setCustomCoverImage(int songId, const QUrl &imageFileUrl);

    QVariantList getMostPlayedSongs(int limit = 10);
    QVariantList getAllSongsList();
    QVariantMap getSong(int songId);

signals:
    void countChanged();
    void searchQueryChanged();
    void sortByChanged();
    void sortAscendingChanged();
    void isImportingChanged();
    void importFinished(int importedCount);

private:
    void importSinglePath(const QString &path, int &count);
    void scanDirectory(const QString &dirPath, int &count);

    QList<SongRecord> m_songs;
    QString m_searchQuery;
    QString m_sortBy = "title";
    bool m_sortAscending = true;
    bool m_isImporting = false;
};
