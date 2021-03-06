/* Phaethon - A FLOSS resource explorer for BioWare's Aurora engine games
 *
 * Phaethon is the legal property of its developers, whose names
 * can be found in the AUTHORS file distributed with this source
 * distribution.
 *
 * Phaethon is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * Phaethon is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Phaethon. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file
 *  ZIP file decompression.
 */

#include <cassert>

#include "src/common/zipfile.h"
#include "src/common/error.h"
#include "src/common/util.h"
#include "src/common/strutil.h"
#include "src/common/encoding.h"
#include "src/common/memreadstream.h"
#include "src/common/deflate.h"

namespace Common {

ZipFile::ZipFile(SeekableReadStream *zip) : _zip(zip) {
	assert(_zip);

	load(*_zip);
}

ZipFile::~ZipFile() {
}

void ZipFile::load(SeekableReadStream &zip) {
	static const byte kEndRecord[4] = { 0x50, 0x4B, 0x05, 0x06 };

	const size_t endPos = searchBackwards(zip, kEndRecord, sizeof(kEndRecord), 0xFFFF);
	if (endPos == SIZE_MAX)
		throw Exception("End of central directory record not found");

	zip.seek(endPos);

	zip.skip(4); // Header, already checked

	uint16 curDisk        = zip.readUint16LE();
	uint16 centralDirDisk = zip.readUint16LE();

	uint16 curDiskDirs = zip.readUint16LE();
	uint16 totalDirs   = zip.readUint16LE();

	if ((curDisk != 0) || (curDisk != centralDirDisk) || (curDiskDirs != totalDirs))
		throw Exception("Unsupported multi-disk ZIP file");

	zip.skip(4); // Size of central directory

	uint32 centralDirPos = zip.readUint32LE();
	zip.seek(centralDirPos);

	uint32 tag = zip.readUint32LE();
	if (tag != 0x02014B50)
		throw Exception("Unknown ZIP record %08X", tag);

	_iFiles.reserve(totalDirs);
	while (tag == 0x02014B50) {
		 File  file;
		IFile iFile;

		zip.skip(20);

		iFile.size = zip.readUint32LE();

		uint16 nameLength    = zip.readUint16LE();
		uint16 extraLength   = zip.readUint16LE();
		uint16 commentLength = zip.readUint16LE();
		uint16 diskNum       = zip.readUint16LE();

		if (diskNum != 0)
			throw Exception("Unsupported multi-disk ZIP file");

		zip.skip(6); // File attributes

		iFile.offset = zip.readUint32LE();

		file.name = readStringFixed(zip, kEncodingASCII, nameLength).toLower();

		zip.skip(extraLength);
		zip.skip(commentLength);

		tag = zip.readUint32LE();
		if ((tag != 0x02014B50) && (tag != 0x06054B50))
			throw Exception("Unknown ZIP record %08X", tag);

		// Ignore empty file names
		if (!file.name.empty()) {
			// HACK: Skip any filename with a trailing slash because it's
			// a directory. The proper solution would be to interpret the
			// file attributes.

			if (*(--file.name.end()) != '/') {
				file.index = _iFiles.size();

				_files.push_back(file);
				_iFiles.push_back(iFile);
			}
		}
	}
}

const ZipFile::FileList &ZipFile::getFiles() const {
	return _files;
}

const ZipFile::IFile &ZipFile::getIFile(uint32 index) const {
	if (index >= _iFiles.size())
		throw Exception("File index out of range (%u/%u)", index, (uint)_iFiles.size());

	return _iFiles[index];
}

void ZipFile::getFileProperties(SeekableReadStream &zip, const IFile &file,
		uint16 &compMethod, uint32 &compSize, uint32 &realSize) const {

	zip.seek(file.offset);

	uint32 tag = zip.readUint32LE();
	if (tag != 0x04034B50)
		throw Exception("Unknown ZIP record %08X", tag);

	zip.skip(4);

	compMethod = zip.readUint16LE();

	zip.skip(8);

	compSize = zip.readUint32LE();
	realSize = zip.readUint32LE();

	uint16 nameLength  = zip.readUint16LE();
	uint16 extraLength = zip.readUint16LE();

	zip.skip(nameLength);
	zip.skip(extraLength);
}

size_t ZipFile::getFileSize(uint32 index) const {
	return getIFile(index).size;
}

SeekableReadStream *ZipFile::getFile(uint32 index, bool tryNoCopy) const {
	const IFile &file = getIFile(index);

	uint16 compMethod;
	uint32 compSize;
	uint32 realSize;

	getFileProperties(*_zip, file, compMethod, compSize, realSize);

	if (tryNoCopy && (compMethod == 0))
		return new SeekableSubReadStream(_zip.get(), _zip->pos(), _zip->pos() + compSize);

	return decompressFile(*_zip, compMethod, compSize, realSize);
}

SeekableReadStream *ZipFile::decompressFile(SeekableReadStream &zip, uint32 method,
		uint32 compSize, uint32 realSize) {

	if (method == 0) {
		// Uncompressed

		return zip.readStream(compSize);
	}

	if (method != 8)
		throw Exception("Unhandled Zip compression %d", method);

	return decompressDeflate(zip, compSize, realSize, kWindowBitsMaxRaw);
}

} // End of namespace Common
