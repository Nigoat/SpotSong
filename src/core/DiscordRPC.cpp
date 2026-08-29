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

#include "DiscordRPC.h"
#include "DatabaseManager.h"
#include <QCoreApplication>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDateTime>
#include <QDir>
#include <QFile>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

DiscordRPC &DiscordRPC::instance()
{
    static DiscordRPC instance;
    return instance;
}

DiscordRPC::DiscordRPC(QObject *parent)
    : QObject(parent)
    , m_socket(new QLocalSocket(this))
    , m_reconnectTimer(new QTimer(this))
    , m_clientId("1543012834772914316")
{
    QString savedId = DatabaseManager::instance().getSetting("discord_client_id", "");
    if (!savedId.trimmed().isEmpty()) {
        m_clientId = savedId.trimmed();
    }

    QString savedEnabled = DatabaseManager::instance().getSetting("discord_rpc_enabled", "true");
    m_enabled = (savedEnabled == "true");

    connect(m_socket, &QLocalSocket::connected, this, &DiscordRPC::handleConnected);
    connect(m_socket, &QLocalSocket::disconnected, this, &DiscordRPC::handleDisconnected);
    connect(m_socket, &QLocalSocket::readyRead, this, &DiscordRPC::handleReadyRead);
    connect(m_socket, &QLocalSocket::errorOccurred, this, &DiscordRPC::handleSocketError);

    m_reconnectTimer->setInterval(6000);
    connect(m_reconnectTimer, &QTimer::timeout, this, &DiscordRPC::tryReconnect);

    if (m_enabled) {
        connectToDiscord();
    }
}

DiscordRPC::~DiscordRPC()
{
    clearActivity();
    disconnectFromDiscord();
}

bool DiscordRPC::isEnabled() const
{
    return m_enabled;
}

bool DiscordRPC::isConnected() const
{
    return m_socket->state() == QLocalSocket::ConnectedState && m_handshakeDone;
}

QString DiscordRPC::clientId() const
{
    return m_clientId;
}

void DiscordRPC::setEnabled(bool enabled)
{
    if (m_enabled != enabled) {
        m_enabled = enabled;
        DatabaseManager::instance().setSetting("discord_rpc_enabled", m_enabled ? "true" : "false");
        emit enabledChanged();

        if (m_enabled) {
            connectToDiscord();
            if (!m_lastTitle.isEmpty()) {
                updateActivity(m_lastTitle, m_lastArtist, m_lastPosition, m_lastDuration, m_lastIsPlaying);
            }
        } else {
            clearActivity();
            disconnectFromDiscord();
        }
    }
}

void DiscordRPC::setClientId(const QString &id)
{
    QString cleanId = id.trimmed();
    if (cleanId.isEmpty()) return;

    if (m_clientId != cleanId) {
        m_clientId = cleanId;
        DatabaseManager::instance().setSetting("discord_client_id", m_clientId);
        emit clientIdChanged();

        disconnectFromDiscord();
        if (m_enabled) {
            connectToDiscord();
        }
    }
}

QString DiscordRPC::findIpcPath(int index)
{
#ifdef _WIN32
    return QString("\\\\.\\pipe\\discord-ipc-%1").arg(index);
#else
    QString uidStr = QString::number(getuid());
    QString path1 = QString("/run/user/%1/discord-ipc-%2").arg(uidStr).arg(index);
    if (QFile::exists(path1)) return path1;

    QString xdg = QString::fromLocal8Bit(qgetenv("XDG_RUNTIME_DIR"));
    if (!xdg.isEmpty()) {
        QString path2 = QString("%1/discord-ipc-%2").arg(xdg).arg(index);
        if (QFile::exists(path2)) return path2;
    }

    QString path3 = QString("/tmp/discord-ipc-%1").arg(index);
    if (QFile::exists(path3)) return path3;

    return path1;
#endif
}

void DiscordRPC::connectToDiscord()
{
    if (!m_enabled) return;
    if (m_socket->state() == QLocalSocket::ConnectedState || m_socket->state() == QLocalSocket::ConnectingState) {
        return;
    }

    m_readBuffer.clear();
    m_handshakeDone = false;

    for (int i = 0; i < 10; ++i) {
        QString pipePath = findIpcPath(i);
        if (pipePath.isEmpty()) continue;
        m_socket->connectToServer(pipePath);
        if (m_socket->waitForConnected(250)) {
            break;
        }
    }

    if (m_socket->state() != QLocalSocket::ConnectedState) {
        if (!m_reconnectTimer->isActive()) {
            m_reconnectTimer->start();
        }
    }
}

void DiscordRPC::disconnectFromDiscord()
{
    m_reconnectTimer->stop();
    m_handshakeDone = false;
    m_readBuffer.clear();
    if (m_socket->isOpen()) {
        m_socket->disconnectFromServer();
    }
    emit connectedChanged();
}

void DiscordRPC::tryReconnect()
{
    if (m_enabled && m_socket->state() == QLocalSocket::UnconnectedState) {
        connectToDiscord();
    }
}

void DiscordRPC::handleConnected()
{
    m_reconnectTimer->stop();

    QJsonObject handshake;
    handshake["v"] = 1;
    handshake["client_id"] = m_clientId;

    QByteArray data = QJsonDocument(handshake).toJson(QJsonDocument::Compact);
    sendPacket(0, data);
}

void DiscordRPC::handleDisconnected()
{
    m_handshakeDone = false;
    m_readBuffer.clear();
    emit connectedChanged();

    if (m_enabled && !m_reconnectTimer->isActive()) {
        m_reconnectTimer->start();
    }
}

void DiscordRPC::handleReadyRead()
{
    m_readBuffer.append(m_socket->readAll());

    while (m_readBuffer.size() >= 8) {
        const char *raw = m_readBuffer.constData();
        quint32 op = static_cast<quint8>(raw[0]) |
                    (static_cast<quint8>(raw[1]) << 8) |
                    (static_cast<quint8>(raw[2]) << 16) |
                    (static_cast<quint8>(raw[3]) << 24);

        quint32 len = static_cast<quint8>(raw[4]) |
                     (static_cast<quint8>(raw[5]) << 8) |
                     (static_cast<quint8>(raw[6]) << 16) |
                     (static_cast<quint8>(raw[7]) << 24);

        if (static_cast<quint32>(m_readBuffer.size()) < 8 + len) {
            break;
        }

        QByteArray payload = m_readBuffer.mid(8, len);
        m_readBuffer.remove(0, 8 + len);

        processIncomingPacket(op, payload);
    }
}

void DiscordRPC::processIncomingPacket(int opcode, const QByteArray &payload)
{
    Q_UNUSED(opcode);
    QJsonDocument doc = QJsonDocument::fromJson(payload);
    if (!doc.isObject()) return;

    QJsonObject obj = doc.object();
    QString cmd = obj.value("cmd").toString();
    QString evt = obj.value("evt").toString();

    if (cmd == "DISPATCH" && evt == "READY") {
        m_handshakeDone = true;
        emit connectedChanged();

        if (!m_lastTitle.isEmpty()) {
            updateActivity(m_lastTitle, m_lastArtist, m_lastPosition, m_lastDuration, m_lastIsPlaying);
        }
    }
}

void DiscordRPC::handleSocketError(QLocalSocket::LocalSocketError error)
{
    Q_UNUSED(error);
    m_handshakeDone = false;
    emit connectedChanged();

    if (m_enabled && !m_reconnectTimer->isActive()) {
        m_reconnectTimer->start();
    }
}

void DiscordRPC::sendPacket(int opcode, const QByteArray &jsonData)
{
    if (m_socket->state() != QLocalSocket::ConnectedState) return;

    QByteArray packet;
    packet.resize(8 + jsonData.size());

    quint32 op = static_cast<quint32>(opcode);
    quint32 len = static_cast<quint32>(jsonData.size());

    packet[0] = static_cast<char>(op & 0xFF);
    packet[1] = static_cast<char>((op >> 8) & 0xFF);
    packet[2] = static_cast<char>((op >> 16) & 0xFF);
    packet[3] = static_cast<char>((op >> 24) & 0xFF);

    packet[4] = static_cast<char>(len & 0xFF);
    packet[5] = static_cast<char>((len >> 8) & 0xFF);
    packet[6] = static_cast<char>((len >> 16) & 0xFF);
    packet[7] = static_cast<char>((len >> 24) & 0xFF);

    memcpy(packet.data() + 8, jsonData.constData(), jsonData.size());

    m_socket->write(packet);
    m_socket->flush();
}

void DiscordRPC::updateActivity(const QString &title, const QString &artist, qint64 positionMs, qint64 durationMs, bool isPlaying)
{
    m_lastTitle = title;
    m_lastArtist = artist;
    m_lastPosition = positionMs;
    m_lastDuration = durationMs;
    m_lastIsPlaying = isPlaying;

    if (!m_enabled || !isConnected()) {
        if (m_enabled && m_socket->state() == QLocalSocket::UnconnectedState) {
            connectToDiscord();
        }
        return;
    }

    if (title.isEmpty()) {
        clearActivity();
        return;
    }

    qint64 currentEpochSec = QDateTime::currentSecsSinceEpoch();
    qint64 positionSec = positionMs / 1000;
    qint64 startTimestamp = currentEpochSec - positionSec;

    QJsonObject activity;
    activity["details"] = title;
    activity["state"] = artist.isEmpty() ? QString("Open source, made by me") : QString("%1 | Open source, made by me").arg(artist);

    QJsonObject timestamps;
    if (isPlaying) {
        timestamps["start"] = startTimestamp;
        if (durationMs > 0) {
            qint64 durationSec = durationMs / 1000;
            timestamps["end"] = startTimestamp + durationSec;
        }
        activity["timestamps"] = timestamps;
    }

    QJsonObject assets;
    assets["large_image"] = "spotsong_logo";
    assets["large_text"] = "SpotSong Music Player";
    activity["assets"] = assets;

    QJsonObject args;
#ifdef _WIN32
    args["pid"] = static_cast<qint64>(GetCurrentProcessId());
#else
    args["pid"] = static_cast<qint64>(getpid());
#endif
    args["activity"] = activity;

    QJsonObject payload;
    payload["cmd"] = "SET_ACTIVITY";
    payload["args"] = args;
    payload["nonce"] = QString::number(currentEpochSec);

    QByteArray data = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    sendPacket(1, data);
}

void DiscordRPC::clearActivity()
{
    if (!isConnected()) return;

    QJsonObject args;
#ifdef _WIN32
    args["pid"] = static_cast<qint64>(GetCurrentProcessId());
#else
    args["pid"] = static_cast<qint64>(getpid());
#endif
    args["activity"] = QJsonValue::Null;

    QJsonObject payload;
    payload["cmd"] = "SET_ACTIVITY";
    payload["args"] = args;
    payload["nonce"] = QString::number(QDateTime::currentSecsSinceEpoch());

    QByteArray data = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    sendPacket(1, data);
}
