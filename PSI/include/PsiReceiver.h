#pragma once

#include "Defines.h"

#include <vector>

namespace PSI {

	class PsiReceiver {
	public:
		PsiReceiver() {}
		void runOffline(PRNG& prng, Channel& ch, block commonSeed, const u64& senderSize, const u64& receiverSize, const u64& height, const u64& logHeight, const u64& width, std::vector<block>& receiverSet, const u64& hashLengthInBytes, const u64& h1LengthInBytes, const u64& bucket1, const u64& bucket2, const u64& senderSize1, const u64& receiverSize1);
		void runNext(PRNG& prng, Channel& ch, block commonSeed, const u64& senderSize, const u64& receiverSize, const u64& height, const u64& logHeight, const u64& width, std::vector<block>& receiverSet, const u64& hashLengthInBytes, const u64& h1LengthInBytes, const u64& bucket1, const u64& bucket2, const u64& senderSize1, const u64& receiverSize1, const u64& height1, const u64& logHeight1, const u64& width1, const u64& hashLengthInBytes1);
	};

}
