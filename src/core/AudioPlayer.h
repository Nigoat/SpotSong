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
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QVariantList>
#include <QVariantMap>
#include "DatabaseManager.h"

class AudioPlayer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY isPlayingChanged)
    Q_PROPERTY(qint64 position READ position NOTIFY positionChanged)
    Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(qreal volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool isMuted READ isMuted WRITE setMuted NOTIFY mutedChanged)
    Q_PROPERTY(int repeatMode READ repeatMode WRITE setRepeatMode NOTIFY repeatModeChanged)
    Q_PROPERTY(bool isShuffle READ isShuffle WRITE setShuffle NOTIFY shuffleChanged)
    Q_PROPERTY(QVariantMap currentSong READ currentSong NOTIFY currentSongChanged)
    Q_PROPERTY(QVariantList queue READ queue NOTIFY queueChanged)
    Q_PROPERTY(int currentQueueIndex READ currentQueueIndex NOTIFY currentQueueIndexChanged)

public:
    enum RepeatMode {
        RepeatOff = 0,
        RepeatOne = 1,
        RepeatAll = 2
    };
    Q_ENUM(RepeatMode)

    explicit AudioPlayer(QObject *parent = nullptr);
    ~AudioPlayer() override;

    bool isPlaying() const;
    qint64 position() const;
    qint64 duration() const;
    qreal volume() const;
    bool isMuted() const;
    int repeatMode() const;
    bool isShuffle() const;
    QVariantMap currentSong() const;
    QVariantList queue() const;
    int currentQueueIndex() const;

public slots:
    void playSong(const QVariantMap &song);
    void playQueue(const QVariantList &songs, int startIndex = 0);
    void addToQueue(const QVariantMap &song);
    void removeFromQueue(int index);
    void clearQueue();
    void togglePlayPause();
    void play();
    void pause();
    void stop();
    void next();
    void previous();
    void seek(qint64 positionMs);
    void setVolume(qreal volume);
    void setMuted(bool muted);
    void setRepeatMode(int mode);
    void setShuffle(bool shuffle);
    QString formatTime(qint64 positionMs) const;

signals:
    void isPlayingChanged();
    void positionChanged();
    void durationChanged();
    void volumeChanged();
    void mutedChanged();
    void repeatModeChanged();
    void shuffleChanged();
    void currentSongChanged();
    void queueChanged();
    void currentQueueIndexChanged();
    void songFinished();

private slots:
    void handleMediaStatusChanged(QMediaPlayer::MediaStatus status);
    void handlePositionChanged(qint64 position);
    void handleDurationChanged(qint64 duration);
    void handlePlaybackStateChanged(QMediaPlayer::PlaybackState state);

private:
    void playCurrentQueueIndex();
    void checkPlayCountThreshold(qint64 position);

    QMediaPlayer *m_player;
    QAudioOutput *m_audioOutput;

    QList<SongRecord> m_queue;
    QList<int> m_shuffledIndices;
    int m_currentIndex = -1;
    RepeatMode m_repeatMode = RepeatOff;
    bool m_shuffle = false;
    bool m_playCountCounted = false;
};
