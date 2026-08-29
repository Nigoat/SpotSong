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

#include "AudioPlayer.h"
#include "DiscordRPC.h"
#include <QUrl>
#include <QRandomGenerator>
#include <algorithm>
#include <random>

AudioPlayer::AudioPlayer(QObject *parent)
    : QObject(parent)
    , m_player(new QMediaPlayer(this))
    , m_audioOutput(new QAudioOutput(this))
{
    m_player->setAudioOutput(m_audioOutput);
    m_audioOutput->setVolume(0.7);

    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, &AudioPlayer::handleMediaStatusChanged);
    connect(m_player, &QMediaPlayer::positionChanged, this, &AudioPlayer::handlePositionChanged);
    connect(m_player, &QMediaPlayer::durationChanged, this, &AudioPlayer::handleDurationChanged);
    connect(m_player, &QMediaPlayer::playbackStateChanged, this, &AudioPlayer::handlePlaybackStateChanged);
}

AudioPlayer::~AudioPlayer()
{
    stop();
}

bool AudioPlayer::isPlaying() const
{
    return m_player->playbackState() == QMediaPlayer::PlayingState;
}

qint64 AudioPlayer::position() const
{
    return m_player->position();
}

qint64 AudioPlayer::duration() const
{
    return m_player->duration();
}

qreal AudioPlayer::volume() const
{
    return m_audioOutput->volume();
}

bool AudioPlayer::isMuted() const
{
    return m_audioOutput->isMuted();
}

int AudioPlayer::repeatMode() const
{
    return static_cast<int>(m_repeatMode);
}

bool AudioPlayer::isShuffle() const
{
    return m_shuffle;
}

QVariantMap AudioPlayer::currentSong() const
{
    if (m_currentIndex >= 0 && m_currentIndex < m_queue.size()) {
        return m_queue.at(m_currentIndex).toMap();
    }
    return QVariantMap();
}

QVariantList AudioPlayer::queue() const
{
    QVariantList list;
    for (const auto &song : m_queue) {
        list.append(song.toMap());
    }
    return list;
}

int AudioPlayer::currentQueueIndex() const
{
    return m_currentIndex;
}

void AudioPlayer::playSong(const QVariantMap &song)
{
    SongRecord record;
    record.id = song.value("id").toInt();
    record.filePath = song.value("filePath").toString();
    record.title = song.value("title").toString();
    record.artist = song.value("artist").toString();
    record.album = song.value("album").toString();
    record.durationMs = song.value("durationMs").toLongLong();
    record.coverPath = song.value("coverPath").toString();
    record.playCount = song.value("playCount").toInt();

    m_queue.clear();
    m_queue.append(record);
    m_currentIndex = 0;
    emit queueChanged();
    emit currentQueueIndexChanged();

    playCurrentQueueIndex();
}

void AudioPlayer::playQueue(const QVariantList &songs, int startIndex)
{
    m_queue.clear();
    for (const auto &var : songs) {
        QVariantMap map = var.toMap();
        SongRecord record;
        record.id = map.value("id").toInt();
        record.filePath = map.value("filePath").toString();
        record.title = map.value("title").toString();
        record.artist = map.value("artist").toString();
        record.album = map.value("album").toString();
        record.durationMs = map.value("durationMs").toLongLong();
        record.coverPath = map.value("coverPath").toString();
        record.playCount = map.value("playCount").toInt();
        m_queue.append(record);
    }

    if (m_queue.isEmpty()) {
        m_currentIndex = -1;
        emit queueChanged();
        emit currentQueueIndexChanged();
        emit currentSongChanged();
        DiscordRPC::instance().clearActivity();
        return;
    }

    if (startIndex >= 0 && startIndex < m_queue.size()) {
        m_currentIndex = startIndex;
    } else {
        m_currentIndex = 0;
    }

    emit queueChanged();
    emit currentQueueIndexChanged();
    playCurrentQueueIndex();
}

void AudioPlayer::addToQueue(const QVariantMap &song)
{
    SongRecord record;
    record.id = song.value("id").toInt();
    record.filePath = song.value("filePath").toString();
    record.title = song.value("title").toString();
    record.artist = song.value("artist").toString();
    record.album = song.value("album").toString();
    record.durationMs = song.value("durationMs").toLongLong();
    record.coverPath = song.value("coverPath").toString();
    record.playCount = song.value("playCount").toInt();

    m_queue.append(record);
    emit queueChanged();

    if (m_currentIndex == -1 && m_queue.size() == 1) {
        m_currentIndex = 0;
        emit currentQueueIndexChanged();
        playCurrentQueueIndex();
    }
}

void AudioPlayer::removeFromQueue(int index)
{
    if (index < 0 || index >= m_queue.size()) return;

    if (index == m_currentIndex) {
        if (m_queue.size() == 1) {
            stop();
            m_queue.clear();
            m_currentIndex = -1;
        } else {
            next();
            m_queue.removeAt(index);
            if (m_currentIndex > index) {
                m_currentIndex--;
            }
        }
    } else {
        m_queue.removeAt(index);
        if (m_currentIndex > index) {
            m_currentIndex--;
        }
    }

    emit queueChanged();
    emit currentQueueIndexChanged();
    emit currentSongChanged();
}

void AudioPlayer::clearQueue()
{
    stop();
    m_queue.clear();
    m_currentIndex = -1;
    emit queueChanged();
    emit currentQueueIndexChanged();
    emit currentSongChanged();
    DiscordRPC::instance().clearActivity();
}

void AudioPlayer::togglePlayPause()
{
    if (isPlaying()) {
        pause();
    } else {
        play();
    }
}

void AudioPlayer::play()
{
    if (m_queue.isEmpty()) return;

    if (m_currentIndex == -1 && !m_queue.isEmpty()) {
        m_currentIndex = 0;
        playCurrentQueueIndex();
        return;
    }

    m_player->play();

    if (m_currentIndex >= 0 && m_currentIndex < m_queue.size()) {
        const SongRecord &song = m_queue.at(m_currentIndex);
        DiscordRPC::instance().updateActivity(song.title, song.artist, m_player->position(), m_player->duration(), true);
    }
}

void AudioPlayer::pause()
{
    m_player->pause();

    if (m_currentIndex >= 0 && m_currentIndex < m_queue.size()) {
        const SongRecord &song = m_queue.at(m_currentIndex);
        DiscordRPC::instance().updateActivity(song.title, song.artist, m_player->position(), m_player->duration(), false);
    }
}

void AudioPlayer::stop()
{
    m_player->stop();
    DiscordRPC::instance().clearActivity();
}

void AudioPlayer::next()
{
    if (m_queue.isEmpty()) return;

    if (m_repeatMode == RepeatOne) {
        seek(0);
        play();
        return;
    }

    if (m_shuffle) {
        if (m_queue.size() > 1) {
            int nextIdx = m_currentIndex;
            while (nextIdx == m_currentIndex) {
                nextIdx = QRandomGenerator::global()->bounded(m_queue.size());
            }
            m_currentIndex = nextIdx;
        }
    } else {
        if (m_currentIndex + 1 < m_queue.size()) {
            m_currentIndex++;
        } else if (m_repeatMode == RepeatAll) {
            m_currentIndex = 0;
        } else {
            stop();
            return;
        }
    }

    emit currentQueueIndexChanged();
    playCurrentQueueIndex();
}

void AudioPlayer::previous()
{
    if (m_queue.isEmpty()) return;

    if (m_player->position() > 3000) {
        seek(0);
        return;
    }

    if (m_currentIndex - 1 >= 0) {
        m_currentIndex--;
    } else if (m_repeatMode == RepeatAll) {
        m_currentIndex = m_queue.size() - 1;
    } else {
        seek(0);
        return;
    }

    emit currentQueueIndexChanged();
    playCurrentQueueIndex();
}

void AudioPlayer::seek(qint64 positionMs)
{
    m_player->setPosition(positionMs);

    if (m_currentIndex >= 0 && m_currentIndex < m_queue.size()) {
        const SongRecord &song = m_queue.at(m_currentIndex);
        DiscordRPC::instance().updateActivity(song.title, song.artist, positionMs, m_player->duration(), isPlaying());
    }
}

void AudioPlayer::setVolume(qreal volume)
{
    qreal clamped = std::clamp(volume, 0.0, 1.0);
    if (qAbs(m_audioOutput->volume() - clamped) > 0.001) {
        m_audioOutput->setVolume(clamped);
        emit volumeChanged();
    }
}

void AudioPlayer::setMuted(bool muted)
{
    if (m_audioOutput->isMuted() != muted) {
        m_audioOutput->setMuted(muted);
        emit mutedChanged();
    }
}

void AudioPlayer::setRepeatMode(int mode)
{
    RepeatMode rMode = static_cast<RepeatMode>(mode);
    if (m_repeatMode != rMode) {
        m_repeatMode = rMode;
        emit repeatModeChanged();
    }
}

void AudioPlayer::setShuffle(bool shuffle)
{
    if (m_shuffle != shuffle) {
        m_shuffle = shuffle;
        emit shuffleChanged();
    }
}

QString AudioPlayer::formatTime(qint64 positionMs) const
{
    qint64 totalSeconds = positionMs / 1000;
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

void AudioPlayer::playCurrentQueueIndex()
{
    if (m_currentIndex < 0 || m_currentIndex >= m_queue.size()) return;

    m_playCountCounted = false;
    const SongRecord &song = m_queue.at(m_currentIndex);
    m_player->setSource(QUrl::fromLocalFile(song.filePath));
    m_player->play();

    emit currentSongChanged();

    DiscordRPC::instance().updateActivity(song.title, song.artist, 0, song.durationMs, true);
}

void AudioPlayer::checkPlayCountThreshold(qint64 position)
{
    if (m_playCountCounted) return;

    if (m_currentIndex >= 0 && m_currentIndex < m_queue.size()) {
        qint64 dur = m_player->duration();
        if (position >= 30000 || (dur > 0 && position >= dur / 2)) {
            m_playCountCounted = true;
            DatabaseManager::instance().incrementPlayCount(m_queue.at(m_currentIndex).id);
        }
    }
}

void AudioPlayer::handleMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    if (status == QMediaPlayer::EndOfMedia) {
        emit songFinished();
        next();
    }
}

void AudioPlayer::handlePositionChanged(qint64 position)
{
    checkPlayCountThreshold(position);
    emit positionChanged();
}

void AudioPlayer::handleDurationChanged(qint64 duration)
{
    emit durationChanged();

    if (m_currentIndex >= 0 && m_currentIndex < m_queue.size()) {
        const SongRecord &song = m_queue.at(m_currentIndex);
        DiscordRPC::instance().updateActivity(song.title, song.artist, m_player->position(), duration, isPlaying());
    }
}

void AudioPlayer::handlePlaybackStateChanged(QMediaPlayer::PlaybackState state)
{
    emit isPlayingChanged();

    if (m_currentIndex >= 0 && m_currentIndex < m_queue.size()) {
        const SongRecord &song = m_queue.at(m_currentIndex);
        bool playing = (state == QMediaPlayer::PlayingState);
        DiscordRPC::instance().updateActivity(song.title, song.artist, m_player->position(), m_player->duration(), playing);
    }
}
