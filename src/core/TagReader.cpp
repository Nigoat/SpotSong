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

#include "TagReader.h"
#include <QFileInfo>
#include <QDir>
#include <QCryptographicHash>
#include <QStandardPaths>
#include <QImage>
#include <QFile>

#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/tbytevector.h>
#include <taglib/mpegfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/id3v2frame.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/flacfile.h>
#include <taglib/flacpicture.h>
#include <taglib/mp4file.h>
#include <taglib/mp4tag.h>
#include <taglib/mp4coverart.h>
#include <taglib/vorbisfile.h>
#include <taglib/xiphcomment.h>

QString TagReader::sanitizeString(const QString &str)
{
    return str.trimmed();
}

SongRecord TagReader::readMetadata(const QString &filePath)
{
    SongRecord record;
    record.filePath = filePath;

    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        return record;
    }

    record.title = fileInfo.completeBaseName();
    record.artist = "Unknown Artist";
    record.album = "Unknown Album";
    record.durationMs = 0;

#ifdef _WIN32
    std::wstring wpath = filePath.toStdWString();
    TagLib::FileRef fileRef(wpath.c_str());
#else
    QByteArray localPath = filePath.toLocal8Bit();
    TagLib::FileRef fileRef(localPath.constData());
#endif

    if (!fileRef.isNull() && fileRef.tag()) {
        TagLib::Tag *tag = fileRef.tag();

        QString title = QString::fromUtf8(tag->title().toCString(true));
        if (!title.trimmed().isEmpty()) {
            record.title = sanitizeString(title);
        }

        QString artist = QString::fromUtf8(tag->artist().toCString(true));
        if (!artist.trimmed().isEmpty()) {
            record.artist = sanitizeString(artist);
        }

        QString album = QString::fromUtf8(tag->album().toCString(true));
        if (!album.trimmed().isEmpty()) {
            record.album = sanitizeString(album);
        }

        if (fileRef.audioProperties()) {
            record.durationMs = static_cast<qint64>(fileRef.audioProperties()->lengthInMilliseconds());
            if (record.durationMs <= 0) {
                record.durationMs = static_cast<qint64>(fileRef.audioProperties()->length()) * 1000;
            }
        }
    }

    record.coverPath = extractAndSaveCoverArt(filePath);

    return record;
}

bool TagReader::writeMetadata(const QString &filePath, const QString &title, const QString &artist, const QString &album)
{
#ifdef _WIN32
    std::wstring wpath = filePath.toStdWString();
    TagLib::FileRef fileRef(wpath.c_str());
#else
    QByteArray localPath = filePath.toLocal8Bit();
    TagLib::FileRef fileRef(localPath.constData());
#endif

    if (fileRef.isNull() || !fileRef.tag()) {
        return false;
    }

    TagLib::Tag *tag = fileRef.tag();
    tag->setTitle(TagLib::String(title.toUtf8().constData(), TagLib::String::UTF8));
    tag->setArtist(TagLib::String(artist.toUtf8().constData(), TagLib::String::UTF8));
    tag->setAlbum(TagLib::String(album.toUtf8().constData(), TagLib::String::UTF8));

    return fileRef.save();
}

QString TagReader::extractAndSaveCoverArt(const QString &audioFilePath)
{
    QString ext = QFileInfo(audioFilePath).suffix().toLower();
    QByteArray imageData;

#ifdef _WIN32
    std::wstring wpath = audioFilePath.toStdWString();
    const wchar_t *pathPtr = wpath.c_str();
#else
    QByteArray localPath = audioFilePath.toLocal8Bit();
    const char *pathPtr = localPath.constData();
#endif

    if (ext == "mp3") {
        TagLib::MPEG::File mpegFile(pathPtr);
        if (mpegFile.isValid() && mpegFile.ID3v2Tag()) {
            TagLib::ID3v2::Tag *id3v2 = mpegFile.ID3v2Tag();
            const auto &frames = id3v2->frameListMap()["APIC"];
            if (!frames.isEmpty()) {
                auto *picFrame = dynamic_cast<TagLib::ID3v2::AttachedPictureFrame *>(frames.front());
                if (picFrame && picFrame->picture().size() > 0) {
                    imageData = QByteArray(picFrame->picture().data(), picFrame->picture().size());
                }
            }
        }
    } else if (ext == "flac") {
        TagLib::FLAC::File flacFile(pathPtr);
        if (flacFile.isValid()) {
            const auto &pictures = flacFile.pictureList();
            if (!pictures.isEmpty() && pictures.front()->data().size() > 0) {
                imageData = QByteArray(pictures.front()->data().data(), pictures.front()->data().size());
            }
        }
    } else if (ext == "m4a" || ext == "mp4" || ext == "aac") {
        TagLib::MP4::File mp4File(pathPtr);
        if (mp4File.isValid() && mp4File.tag()) {
            TagLib::MP4::ItemMap items = mp4File.tag()->itemMap();
            if (items.contains("covr")) {
                TagLib::MP4::CoverArtList artList = items["covr"].toCoverArtList();
                if (!artList.isEmpty() && artList.front().data().size() > 0) {
                    imageData = QByteArray(artList.front().data().data(), artList.front().data().size());
                }
            }
        }
    }

    if (imageData.isEmpty()) {
        return QString();
    }

    QString coversDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/covers";
    QDir().mkpath(coversDir);

    QByteArray hash = QCryptographicHash::hash(audioFilePath.toUtf8(), QCryptographicHash::Md5).toHex();
    QString coverFilePath = coversDir + "/" + QString::fromLatin1(hash) + ".jpg";

    if (!QFile::exists(coverFilePath)) {
        QImage image;
        if (image.loadFromData(imageData)) {
            image.scaled(500, 500, Qt::KeepAspectRatio, Qt::SmoothTransformation).save(coverFilePath, "JPG", 90);
        } else {
            QFile file(coverFilePath);
            if (file.open(QIODevice::WriteOnly)) {
                file.write(imageData);
                file.close();
            }
        }
    }

    return coverFilePath;
}
