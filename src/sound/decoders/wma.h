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
 *  Decoding Microsoft's Windows Media Audio.
 */

/* Based on the WMA implementation in FFmpeg (<https://ffmpeg.org/)>,
 * which is released under the terms of version 2 or later of the GNU
 * Lesser General Public License.
 *
 * The original copyright note in libavcodec/wma.c reads as follows:
 *
 * WMA compatible codec
 * Copyright (c) 2002-2007 The FFmpeg Project
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef SOUND_DECODERS_WMA_H
#define SOUND_DECODERS_WMA_H

#include <vector>

#include "src/common/types.h"
#include "src/common/scopedptr.h"
#include "src/common/ptrvector.h"

#include "src/sound/audiostream.h"
#include "src/sound/decoders/codec.h"

namespace Common {
	class BitStream;
	class Huffman;
	class MDCT;
}

namespace Sound {

struct WMACoefHuffmanParam;

class WMACodec : public Codec, public PacketizedAudioStream {
public:
	WMACodec(int version, uint32 sampleRate, uint8 channels,
	         uint32 bitRate, uint32 blockAlign, Common::SeekableReadStream *extraData = 0);
	~WMACodec();

	// Codec API
	AudioStream *decodeFrame(Common::SeekableReadStream &data);

	// AudioStream API
	int getChannels() const { return _channels; }
	int getRate() const { return _sampleRate; }
	bool endOfData() const { return _audStream->endOfData(); }
	bool endOfStream() const { return _audStream->endOfStream(); }
	size_t readBuffer(int16 *buffer, const size_t numSamples) { return _audStream->readBuffer(buffer, numSamples); }

	// PacketizedAudioStream API
	void finish() { _audStream->finish(); }
	bool isFinished() const { return _audStream->isFinished(); }
	void queuePacket(Common::SeekableReadStream *data);

private:
	static const int kChannelsMax = 2; ///< Max number of channels we support.

	static const int kBlockBitsMin =  7; ///< Min number of bits in a block.
	static const int kBlockBitsMax = 11; ///< Max number of bits in a block.

	/** Max number of bytes in a block. */
	static const int kBlockSizeMax = (1 << kBlockBitsMax);

	static const int kBlockNBSizes = (kBlockBitsMax - kBlockBitsMin + 1);

	/** Max size of a superframe. */
	static const int kSuperframeSizeMax = 16384;

	/** Max size of a high band. */
	static const int kHighBandSizeMax = 16;

	/** Size of the noise table. */
	static const int kNoiseTabSize = 8192;

	/** Number of bits for the LSP power value. */
	static const int kLSPPowBits = 7;

	int _version; ///< WMA version.

	uint32 _sampleRate; ///< Output sample rate.
	uint8  _channels;   ///< Output channel count.
	uint32 _bitRate;    ///< Input bit rate.
	uint32 _blockAlign; ///< Input block align.
	byte   _audioFlags; ///< Output flags.

	bool _useExpHuffman;       ///< Exponents in Huffman code? Otherwise, in LSP.
	bool _useBitReservoir;     ///< Is each frame packet a "superframe"?
	bool _useVariableBlockLen; ///< Are the block lengths variable?
	bool _useNoiseCoding;      ///< Should perceptual noise be added?

	bool _resetBlockLengths; ///< Do we need new block lengths?

	int _curFrame;       ///< The number of the frame we're currently in.
	int _frameLen;       ///< The frame length.
	int _frameLenBits;   ///< log2 of the frame length.
	int _blockSizeCount; ///< Number of block sizes.
	int _framePos;       ///< The position within the frame we're currently in.

	int _curBlock;         ///< The number of the block we're currently in.
	int _blockLen;         ///< Current block length.
	int _blockLenBits;     ///< log2 of current block length.
	int _nextBlockLenBits; ///< log2 of next block length.
	int _prevBlockLenBits; ///< log2 of previous block length.

	int _byteOffsetBits;

	// Coefficients
	int    _coefsStart;                       ///< First coded coef.
	int    _coefsEnd[kBlockNBSizes];          ///< Max number of coded coefficients.
	int    _exponentSizes[kBlockNBSizes];
	uint16 _exponentBands[kBlockNBSizes][25];
	int    _highBandStart[kBlockNBSizes];     ///< Index of first coef in high band.
	int    _exponentHighSizes[kBlockNBSizes];
	int    _exponentHighBands[kBlockNBSizes][kHighBandSizeMax];

	Common::ScopedPtr<Common::Huffman> _coefHuffman[2]; ///< Coefficients Huffman codes.

	const WMACoefHuffmanParam *_coefHuffmanParam[2]; ///< Params for coef Huffman codes.

	Common::ScopedArray<uint16> _coefHuffmanRunTable[2];   ///< Run table for the coef Huffman.
	Common::ScopedArray<float>  _coefHuffmanLevelTable[2]; ///< Level table for the coef Huffman.
	Common::ScopedArray<uint16> _coefHuffmanIntTable[2];   ///< Int table for the coef Huffman.

	// Noise
	float _noiseMult;                 ///< Noise multiplier.
	float _noiseTable[kNoiseTabSize]; ///< Noise table.
	int   _noiseIndex;

	Common::ScopedPtr<Common::Huffman> _hgainHuffman; ///< Perceptual noise Huffman code.

	// Exponents
	int   _exponentsBSize[kChannelsMax];
	float _exponents[kChannelsMax][kBlockSizeMax];
	float _maxExponent[kChannelsMax];

	Common::ScopedPtr<Common::Huffman> _expHuffman; ///< Exponents Huffman code.

	// Coded values in high bands
	bool _highBandCoded [kChannelsMax][kHighBandSizeMax];
	int  _highBandValues[kChannelsMax][kHighBandSizeMax];

	// Coefficients
	float _coefs1[kChannelsMax][kBlockSizeMax];
	float _coefs [kChannelsMax][kBlockSizeMax];

	// Line spectral pairs
	float _lspCosTable[kBlockSizeMax];
	float _lspPowETable[256];
	float _lspPowMTable1[(1 << kLSPPowBits)];
	float _lspPowMTable2[(1 << kLSPPowBits)];

	// MDCT
	Common::PtrVector<Common::MDCT> _mdct;       ///< MDCT contexts.
	std::vector<const float *>  _mdctWindow; ///< MDCT window functions.

	/** Overhang from the last superframe. */
	byte _lastSuperframe[kSuperframeSizeMax + 4];
	int  _lastSuperframeLen; ///< Size of the overhang data.
	int  _lastBitoffset;     ///< Bit position within the overhang.

	// Output
	float _output[kBlockSizeMax * 2];
	float _frameOut[kChannelsMax][kBlockSizeMax * 2];

	// Backing stream for PacketizedAudioStream
	Common::ScopedPtr<QueuingAudioStream> _audStream;

	// Init helpers

	void init(Common::SeekableReadStream *extraData);

	uint16 getFlags(Common::SeekableReadStream *extraData);
	void evalFlags(uint16 flags, Common::SeekableReadStream *extraData);
	int getFrameBitLength();
	int getBlockSizeCount(uint16 flags);
	uint32 getNormalizedSampleRate();
	bool useNoiseCoding(float &highFreq, float &bps);
	void evalMDCTScales(float highFreq);
	void initNoise();
	void initCoefHuffman(float bps);
	void initMDCT();
	void initExponents();

	Common::Huffman *initCoefHuffman(Common::ScopedArray<uint16> &runTable,
	                                 Common::ScopedArray<float>  &levelTable,
	                                 Common::ScopedArray<uint16> &intTable,
	                                 const WMACoefHuffmanParam &params);
	void initLSPToCurve();

	// Decoding

	Common::SeekableReadStream *decodeSuperFrame(Common::SeekableReadStream &data);
	bool decodeFrame(Common::BitStream &bits, int16 *outputData);
	int decodeBlock(Common::BitStream &bits);

	// Decoding helpers

	bool evalBlockLength(Common::BitStream &bits);
	bool decodeChannels(Common::BitStream &bits, int bSize, bool msStereo, bool *hasChannel);
	bool calculateIMDCT(int bSize, bool msStereo, bool *hasChannel);

	void calculateCoefCount(int *coefCount, int bSize) const;
	bool decodeNoise(Common::BitStream &bits, int bSize, bool *hasChannel, int *coefCount);
	bool decodeExponents(Common::BitStream &bits, int bSize, bool *hasChannel);
	bool decodeSpectralCoef(Common::BitStream &bits, bool msStereo, bool *hasChannel,
	                        int *coefCount, int coefBitCount);
	float getNormalizedMDCTLength() const;
	void calculateMDCTCoefficients(int bSize, bool *hasChannel,
	                               int *coefCount, int totalGain, float mdctNorm);

	bool decodeExpHuffman(Common::BitStream &bits, int ch);
	bool decodeExpLSP(Common::BitStream &bits, int ch);
	bool decodeRunLevel(Common::BitStream &bits, const Common::Huffman &huffman,
		const float *levelTable, const uint16 *runTable, int version, float *ptr,
		int offset, int numCoefs, int blockLen, int frameLenBits, int coefNbBits);

	void lspToCurve(float *out, float *val_max_ptr, int n, float *lsp);

	void window(float *out) const;

	float pow_m1_4(float x) const;

	static int readTotalGain(Common::BitStream &bits);
	static int totalGainToBits(int totalGain);
	static uint32 getLargeVal(Common::BitStream &bits);
};

} // End of namespace Sound

#endif // SOUND_DECODERS_WMA_H
