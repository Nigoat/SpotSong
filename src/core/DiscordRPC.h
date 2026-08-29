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
#include <QLocalSocket>
#include <QTimer>
#include <QString>
#include <QByteArray>

class DiscordRPC : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(bool isConnected READ isConnected NOTIFY connectedChanged)
    Q_PROPERTY(QString clientId READ clientId WRITE setClientId NOTIFY clientIdChanged)

public:
    static DiscordRPC &instance();

    bool isEnabled() const;
    bool isConnected() const;
    QString clientId() const;

public slots:
    void setEnabled(bool enabled);
    void setClientId(const QString &id);
    void connectToDiscord();
    void disconnectFromDiscord();
    void updateActivity(const QString &title, const QString &artist, qint64 positionMs, qint64 durationMs, bool isPlaying);
    void clearActivity();

signals:
    void enabledChanged();
    void connectedChanged();
    void clientIdChanged();

private slots:
    void handleConnected();
    void handleDisconnected();
    void handleReadyRead();
    void handleSocketError(QLocalSocket::LocalSocketError error);
    void tryReconnect();

private:
    explicit DiscordRPC(QObject *parent = nullptr);
    ~DiscordRPC() override;
    DiscordRPC(const DiscordRPC &) = delete;
    DiscordRPC &operator=(const DiscordRPC &) = delete;

    void sendPacket(int opcode, const QByteArray &jsonData);
    void processIncomingPacket(int opcode, const QByteArray &payload);
    QString findIpcPath(int index = 0);

    QLocalSocket *m_socket;
    QTimer *m_reconnectTimer;
    QString m_clientId;
    bool m_enabled = true;
    bool m_handshakeDone = false;
    QByteArray m_readBuffer;

    QString m_lastTitle;
    QString m_lastArtist;
    qint64 m_lastPosition = 0;
    qint64 m_lastDuration = 0;
    bool m_lastIsPlaying = false;
};
