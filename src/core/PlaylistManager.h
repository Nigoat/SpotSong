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
#include "DatabaseManager.h"

class PlaylistModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum PlaylistRoles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        CoverPathRole,
        SongCountRole,
        CreatedAtRole
    };
    Q_ENUM(PlaylistRoles)

    explicit PlaylistModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const;

public slots:
    void refresh();

signals:
    void countChanged();

private:
    QList<PlaylistRecord> m_playlists;
};

class PlaylistSongsModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int playlistId READ playlistId WRITE setPlaylistId NOTIFY playlistIdChanged)
    Q_PROPERTY(QString playlistName READ playlistName NOTIFY playlistNameChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

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
        AddedAtRole,
        TrackNumberRole
    };
    Q_ENUM(SongRoles)

    explicit PlaylistSongsModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int playlistId() const;
    QString playlistName() const;
    int count() const;

public slots:
    void setPlaylistId(int id);
    void refresh();
    void removeSong(int songId);
    void moveSong(int fromIndex, int toIndex);
    QVariantList getSongsList();

signals:
    void playlistIdChanged();
    void playlistNameChanged();
    void countChanged();

private:
    int m_playlistId = -1;
    QString m_playlistName;
    QList<SongRecord> m_songs;
};

class PlaylistManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(PlaylistModel* playlistsModel READ playlistsModel CONSTANT)
    Q_PROPERTY(PlaylistSongsModel* activePlaylistModel READ activePlaylistModel CONSTANT)

public:
    explicit PlaylistManager(QObject *parent = nullptr);
    ~PlaylistManager() override;

    PlaylistModel* playlistsModel() const;
    PlaylistSongsModel* activePlaylistModel() const;

public slots:
    int createPlaylist(const QString &name);
    bool renamePlaylist(int playlistId, const QString &newName);
    bool deletePlaylist(int playlistId);
    bool addSongToPlaylist(int playlistId, int songId);
    bool removeSongFromPlaylist(int playlistId, int songId);
    bool isSongInPlaylist(int playlistId, int songId);
    void openPlaylist(int playlistId);
    QVariantList getAllPlaylists();
    QVariantMap getPlaylist(int playlistId);

signals:
    void playlistCreated(int playlistId);
    void playlistDeleted(int playlistId);
    void playlistOpened(int playlistId);

private:
    PlaylistModel *m_playlistsModel;
    PlaylistSongsModel *m_activePlaylistModel;
};
