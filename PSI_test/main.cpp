#include "PSI/include/Defines.h"
#include "PSI/include/utils.h"
#include "PSI/include/PsiSender.h"
#include "PSI/include/PsiReceiver.h"

#include <cryptoTools/Network/IOService.h>
#include <cryptoTools/Network/Endpoint.h>
#include <cryptoTools/Network/Channel.h>
#include <cryptoTools/Common/CLP.h>
#include <cryptoTools/Common/Log.h>

#include <vector>

using namespace std;
using namespace PSI;

const block commonSeed = oc::toBlock(123456);

u64 senderSize;
u64 senderSize1;
u64 senderSize2;
u64 senderSize3;
u64 senderSize4;
u64 senderSize5;
u64 senderSize6;
u64 senderSize7;
u64 senderSize8;
u64 senderSize9;
u64 senderSize10;
u64 All_senderSize;
u64 PrepareSize;
u64 receiverSize1;
u64 receiverSize2;
u64 receiverSize3;
u64 receiverSize4;
u64 receiverSize5;
u64 receiverSize6;
u64 receiverSize7;
u64 receiverSize8;
u64 receiverSize9;
u64 receiverSize10;
u64 All_receiverSize;
u64 width;
u64 height;
u64 logHeight;
u64 hashLengthInBytes;
u64 width1;
u64 height1;
u64 logHeight1;
u64 hashLengthInBytes1;
u64 bucket, bucket1, bucket2;
u64 Float;
string ip;


void runSenderOffline() {
	IOService ios;
	Endpoint ep(ios, ip, EpMode::Server, "test-psi");
	Channel ch = ep.addChannel();

	// All_senderSize=senderSize1+senderSize2;
	// senderSize2=All_senderSize;
	vector<block> senderSet(All_senderSize); 
	//-------------delete----------delete----------delete------------
	PRNG prng(oc::toBlock(123));//means a random feed in "123".
	for (auto i = 0; i < All_senderSize; ++i) {
		senderSet[i] = prng.get<block>();
	}
	//-------------delete----------delete----------delete------------
	PsiSender psiSender;
	BitVector choices(width);
	prng.get(choices.data(), choices.sizeBytes());

	psiSender.runOffline(prng, ch, commonSeed, All_senderSize, All_receiverSize, height, logHeight, width, senderSet, hashLengthInBytes, 32, bucket1, bucket2, senderSize1, receiverSize1, choices);
	psiSender.runNext(prng, ch, commonSeed, All_senderSize, All_receiverSize, height, logHeight, width, senderSet, hashLengthInBytes, 32, bucket1, bucket2, senderSize1, receiverSize1, choices, height1, logHeight1, width1, hashLengthInBytes1);

	
	ch.close();
	ep.stop();
	ios.stop();
}


void runReceiverOffline() {
	IOService ios;
	Endpoint ep(ios, ip, EpMode::Client, "test-psi");
	Channel ch = ep.addChannel();

	// All_receiverSize=receiverSize1+receiverSize2;

	vector<block> receiverSet(All_receiverSize); 
	PRNG prng0(oc::toBlock(123));//means a random feed in "123".
	for (auto i = 0; i < 100; ++i) {
		receiverSet[i] = prng0.get<block>();
	}
	PRNG prng(oc::toBlock(314));//means a random feed in "123".
	for (auto i = 100; i < All_receiverSize; ++i) {
		receiverSet[i] = prng.get<block>();
	}

	PsiReceiver psiReceiver;
	psiReceiver.runOffline(prng, ch, commonSeed, All_senderSize, All_receiverSize, height, logHeight, width, receiverSet, hashLengthInBytes, 32, bucket1, bucket2, senderSize1, receiverSize1);
	psiReceiver.runNext(prng, ch, commonSeed, All_senderSize, All_receiverSize, height, logHeight, width, receiverSet, hashLengthInBytes, 32, bucket1, bucket2, senderSize1, receiverSize1, height1, logHeight1, width1, hashLengthInBytes1);
	
	
	ch.close();
	ep.stop();
	ios.stop();
}


int main(int argc, char** argv) {
	
	oc::CLP cmd;
	cmd.parse(argc, argv);
	
	cmd.setDefault("ps", 20);
	PrepareSize = 1 << cmd.get<u64>("ss");

	cmd.setDefault("ss", 20);
	All_senderSize = 1 << cmd.get<u64>("ss");

	cmd.setDefault("rs", 20);
	All_receiverSize = 1 << cmd.get<u64>("rs");

	cmd.setDefault("ss1", 16);
	senderSize1 = 1 << cmd.get<u64>("ss1");
	
	cmd.setDefault("rs1", 16);
	receiverSize1 = 1 << cmd.get<u64>("rs1");

	cmd.setDefault("ss2", 16);
	senderSize2 = 1 << cmd.get<u64>("ss2");
	
	cmd.setDefault("rs2", 16);
	receiverSize2 = 1 << cmd.get<u64>("rs2");

	cmd.setDefault("ss3", 16);
	senderSize3 = 1 << cmd.get<u64>("ss3");
	
	cmd.setDefault("rs3", 16);
	receiverSize3 = 1 << cmd.get<u64>("rs3");

	cmd.setDefault("float", 1);
	Float = cmd.get<u64>("float");

	cmd.setDefault("w", 632);
	width = cmd.get<u64>("w");
	
	cmd.setDefault("h", 20);
	logHeight = cmd.get<u64>("h");
	height = 1 << cmd.get<u64>("h");
	
	cmd.setDefault("hash", 10);
	hashLengthInBytes = cmd.get<u64>("hash");

	cmd.setDefault("w1", 609);
	width1 = cmd.get<u64>("w1");
	
	cmd.setDefault("h1", 16);
	logHeight1 = cmd.get<u64>("h1");
	height1 = 1 << cmd.get<u64>("h1");
	
	cmd.setDefault("hash1", 9);
	hashLengthInBytes1 = cmd.get<u64>("hash1");
	
	cmd.setDefault("ip", "localhost");
	ip = cmd.get<string>("ip");
	
	bucket1 = bucket2 = 1 << 8;
	
	bool noneSet = !cmd.isSet("r");
	if (noneSet) {
		std::cout
		<< "=================================\n"
		<< "||  Private Set Intersection   ||\n"
		<< "=================================\n"
		<< "\n"
		<< "This program reports the performance of the private set intersection protocol.\n"
		<< "\n"
		<< "Experimenet flag:\n"
		<< " -r 0    to run a sender.\n"
		<< " -r 1    to run a receiver.\n"
		<< "\n"
		<< "Parameters:\n"
		<< " -ss     log(#elements) on sender side.\n"
		<< " -rs     log(#elements) on receiver side.\n"
		<< " -w      width of the matrix.\n"
		<< " -h      log(height) of the matrix.\n"
		<< " -hash   hash output length in bytes.\n"
		<< " -ip     ip address (and port).\n"
		;
	} else {
		// All_senderSize=senderSize1+senderSize2;
		// All_receiverSize=receiverSize1+receiverSize2;
		// height=Float*height;
		if (cmd.get<u64>("r") == 0) {
			runSenderOffline();
		} else if (cmd.get<u64>("r") == 1) {
			runReceiverOffline();
		}
	}

	return 0;
}