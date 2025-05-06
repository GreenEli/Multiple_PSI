#include "PsiSender.h"
#include<sys/mman.h>  
#include <iostream>
#include <cstdlib>
#include <bitset>

using namespace std;
namespace PSI {

	void PsiSender::runOffline(PRNG& prng, Channel& ch, block commonSeed, const u64& senderSize, const u64& receiverSize, const u64& height, const u64& logHeight, const u64& width, std::vector<block>& senderSet, const u64& hashLengthInBytes, const u64& h1LengthInBytes, const u64& bucket1, const u64& bucket2, const u64& senderSize1, const u64& receiverSize1, BitVector choices) {
	for(int w=0;w<2;w++){
		if(w==0){
			Timer timer;
			char *savePath_c = "/tmp/output_C.txt";
			if(remove(savePath_c)==0){
			cout<<"The path is delete successfully\n";
			}
			timer.setTimePoint("Sender start");
			
			auto heightInBytes = (height + 7) / 8;
			auto widthInBytes = (width + 7) / 8;
			auto locationInBytes = (logHeight + 7) / 8;//0001|1110|0101|1010|...
			auto senderSizeInBytes = (senderSize + 7) / 8;
			auto shift = (1 << logHeight) - 1;
			auto widthBucket1 = sizeof(block) / locationInBytes;//which is familier with this as 0001|1100|0101|1000|...|A Batchnorm
			int widthdivide=width/widthBucket1+1;

			//////////////////// Base OTs /////////////////////////////////
			
			IknpOtExtReceiver otExtReceiver;
			otExtReceiver.genBaseOts(prng, ch);
			// BitVector choices(width);
			std::vector<block> otMessages(width);
			// prng.get(choices.data(), choices.sizeBytes());
			otExtReceiver.receive(choices, otMessages, prng, ch);
			
			timer.setTimePoint("Sender base OT finished");


			////////////// Initialization //////////////////////
			
			PRNG commonPrng(commonSeed);
			block commonKey;
			AES commonAes;
			
			u8* transLocations[widthBucket1];
			for (auto i = 0; i < widthBucket1; ++i) {
				transLocations[i] = new u8[senderSize * locationInBytes + sizeof(u32)];//everysender have locationInBytes
			}
			
			block randomLocations[bucket1];//ok

			u8* matrixC[widthBucket1];//also yipiyipi kuangdu.
			for (auto i = 0; i < widthBucket1; ++i) {
				matrixC[i] = new u8[heightInBytes];
			}

			//transHashInputs[sender][width]=0|1|0|0|1|1|0|...
			u8* transHashInputs[width];
			for (auto i = 0; i < width; ++i) {
				transHashInputs[i] = new u8[senderSizeInBytes];
				memset(transHashInputs[i], 0, senderSizeInBytes);
			}

			// std::cout << "Sender initialed\n";
			timer.setTimePoint("Sender initialed");

			/////////// Transform input /////////////////////

			commonPrng.get((u8*)&commonKey, sizeof(block));
			commonAes.setKey(commonKey);
			
			block* sendSet = new block[senderSize];
			block* aesInput = new block[senderSize];
			block* aesOutput = new block[senderSize];
			
			RandomOracle H1(h1LengthInBytes);
			u8 h1Output[h1LengthInBytes];
			
			for (auto i = 0; i < senderSize; ++i) {
				H1.Reset();
				H1.Update((u8*)(senderSet.data() + i), sizeof(block));
				H1.Final(h1Output);
				
				aesInput[i] = *(block*)h1Output;
				sendSet[i] = *(block*)(h1Output + sizeof(block));
			}
			//Aes(h1())is OVER.
			commonAes.ecbEncBlocks(aesInput, senderSize, aesOutput);
			for (auto i = 0; i < senderSize; ++i) {
				sendSet[i] ^= aesOutput[i];
			}
			
			// std::cout << "Sender set transformed\n";
			timer.setTimePoint("Sender set transformed");

			int len2 = heightInBytes;
			int len1 = len2*widthBucket1;
			int len = len1*widthdivide;
			int fd_c=open("/tmp/output_C.txt", O_RDWR|O_CREAT, 00777);
			u8* addr_c;
			// lseek将文件指针往后移动file_size-1位
			lseek(fd_c,len-1,SEEK_END);   
			write(fd_c, "", 1);
			// 使用mmap函数建立内存映射
			// addr = (u8*)mmap(NULL, len, PROT_READ|PROT_WRITE,MAP_SHARED, fd, 0);
			addr_c = (u8*)mmap(NULL, len, PROT_READ|PROT_WRITE,MAP_SHARED, fd_c, 0);
			int t=0;
			for (auto wLeft = 0; wLeft < width; wLeft += widthBucket1) {
				auto wRight = wLeft + widthBucket1 < width ? wLeft + widthBucket1 : width;
				auto w = wRight - wLeft;
				t++;

				//////////// Compute random locations (transposed) ////////////////

				commonPrng.get((u8*)&commonKey, sizeof(block));
				commonAes.setKey(commonKey);
				
				for (auto low = 0; low < senderSize; low += bucket1) {
				
					auto up = low + bucket1 < senderSize ? low + bucket1 : senderSize;
				
					commonAes.ecbEncBlocks(sendSet + low, up - low, randomLocations);//Fkey(h1) truly.

					for (auto i = 0; i < w; ++i) {
						for (auto j = low; j < up; ++j) {
							// Final i know what's the mean it is.randomLocations + (j - low) means 0001|1110|...|
							//every row have j * locationInBytes,then can be used easyly.
							//(u8*)(randomLocations + (j - low)) + i * locationInBytes means randomLocations[i][jfrom 0 to locationInBytes] copy to transLocations[i][jfrom 0 to locationInBytes]
							memcpy(transLocations[i] + j * locationInBytes, (u8*)(randomLocations + (j - low)) + i * locationInBytes, locationInBytes);//dazhigaodongleyitu.
						}
					}
				}
				
				//////////////// Extend OTs and compute matrix C ///////////////////
				//R-OT process.
				u8* recvMatrix;
				recvMatrix = new u8[heightInBytes];
				// std::cout <<t<< " Sender is \n";
				for (auto i = 0; i < w; ++i) {
					PRNG prng(otMessages[i + wLeft]);
					prng.get(matrixC[i], heightInBytes);
					
					ch.recv(recvMatrix, heightInBytes);
					
					if (choices[i + wLeft]) {
						for (auto j = 0; j < heightInBytes; ++j) {
							matrixC[i][j] ^= recvMatrix[j];
						}
					}
				}
				//Relate yuansu to copy into another way
				for(int k=1;k<widthBucket1+1;k++){
					memcpy(addr_c+(k-1)*len2+(t-1)*len1, matrixC[k-1], len2);
				}

				///////////////// Compute hash inputs (transposed) /////////////////////
				
				for (auto i = 0; i < w; ++i) {
					for (auto j = 0; j < senderSize; ++j) {
						auto location = (*(u32*)(transLocations[i] + j * locationInBytes)) & shift;
						//it is really excellent transHashInputs ments the senderSize*w outputs.
						transHashInputs[i + wLeft][j >> 3] |= (u8)((bool)(matrixC[i][location >> 3] & (1 << (location & 7)))) << (j & 7);
					}		
				}
				
			}

			// 内存映射建立好了，此时可以关闭文件了
			close(fd_c);
			munmap(addr_c, len);
			// std::cout << "Sender transposed hash input computed\n";
			timer.setTimePoint("Sender transposed hash input computed");

			/////////////////// Compute hash outputs ///////////////////////////
			
			RandomOracle H(hashLengthInBytes);
			u8 hashOutput[sizeof(block)];

			u8* hashInputs[bucket2];
			
			for (auto i = 0; i < bucket2; ++i) {
				hashInputs[i] = new u8[widthInBytes];
			}
			
			for (auto low = 0; low < senderSize; low += bucket2) {
				auto up = low + bucket2 < senderSize ? low + bucket2 : senderSize;
				
				for (auto j = low; j < up; ++j) {
					memset(hashInputs[j - low], 0, widthInBytes);
				}
				
				for (auto i = 0; i < width; ++i) {
					for (auto j = low; j < up; ++j) {
						hashInputs[j - low][i >> 3] |= (u8)((bool)(transHashInputs[i][j >> 3] & (1 << (j & 7)))) << (i & 7);//means * 2^(i & 7),that is OK!
					}
				}
				
				u8* sentBuff = new u8[(up - low) * hashLengthInBytes];
				
				for (auto j = low; j < up; ++j) {
					H.Reset();
					H.Update(hashInputs[j - low], widthInBytes);
					H.Final(hashOutput);
					
					memcpy(sentBuff + (j - low) * hashLengthInBytes, hashOutput, hashLengthInBytes);
				}
				
				ch.asyncSend(sentBuff, (up - low) * hashLengthInBytes);//yipiyipi send to receiver.
			}
			
			// std::cout << "Sender hash outputs computed and sent\n";
			timer.setTimePoint("Sender hash outputs computed and sent");

			std::cout << timer;
		}
		else{
			const block commonSeed1 = oc::toBlock(234567);
			u64 senderSize_To;
			u64 receiverSize_To;
			senderSize_To=receiverSize;
			receiverSize_To=senderSize;

			vector<block> receiverSet(receiverSize_To); 
			PRNG prng1(oc::toBlock(123));//means a random feed in "123".
			for (auto i = 0; i < receiverSize_To; ++i) {
				receiverSet[i] = prng1.get<block>();
			}
		//////////Syn The Time ///////////////////////////

			u8* SynTime = new u8[hashLengthInBytes];
			ch.recv(SynTime, hashLengthInBytes);
			ch.asyncSend(SynTime, hashLengthInBytes);//yipiyipi send to receiver.
		////////Syn The Time /////////////////////////////
			Timer timer;
			timer.setTimePoint("Receiver start");
			TimeUnit start, end;
			char *savePath = "/tmp/output_D1.txt";
			char *savePath_a = "/tmp/output_A1.txt";
			// char *savePath_Sender = "/home/qiqiang6/Desktop/EX_PSIOT_Ex/OPRF-PSI/PSI/Output/output_S.txt";
			char *savePath_Receiver = "/tmp/output_R1.txt";
			if(remove(savePath)==0 && remove(savePath_a)==0 && remove(savePath_Receiver)==0){
				cout<<"The path is delete successfully\n";
			}
			cout<<"\n\n\n---------------------------------------The prepare process---------------------------------------\n";
			cout<<"---------------------------------------The prepare process---------------------------------------\n";
			cout<<"---------------------------------------The prepare process---------------------------------------\n";
			cout<<"---------------------------------------The prepare process---------------------------------------\n";
			cout<<"---------------------------------------The prepare process---------------------------------------\n\n\n";
			auto heightInBytes = (height + 7) / 8;
			auto SelfheightInBytes = (height + 7) / 16;
			auto widthInBytes = (width + 7) / 8;
			auto locationInBytes = (logHeight + 7) / 8;
			auto receiverSizeInBytes = (receiverSize_To + 7) / 8;
			auto shift = (1 << logHeight) - 1;
			auto widthBucket1 = sizeof(block) / locationInBytes;
			int widthdivide=width/widthBucket1+1;

			u64 sentData = ch.getTotalDataSent();
			u64 recvData = ch.getTotalDataRecv();
			u64 totalData = sentData + recvData;	
			
			///////////////////// Base OTs ///////////////////////////
			
			IknpOtExtSender otExtSender;
			otExtSender.genBaseOts(prng1, ch);
			
			std::vector<std::array<block, 2> > otMessages(width);

			otExtSender.send(otMessages, prng1, ch);

			// std::cout << "Receiver base OT finished\n";
			timer.setTimePoint("Receiver base OT finished");
			
			//////////// Initialization ///////////////////
			
			PRNG commonPrng(commonSeed1);
			block commonKey;
			AES commonAes;
			
			u8* matrixA[widthBucket1];// a batchnorm
			u8* matrixDelta[widthBucket1];
			u8* matrixFinal[widthBucket1];
			// matrixDelta[0];
			for (auto i = 0; i < widthBucket1; ++i) {
				matrixA[i] = new u8[heightInBytes];//every have the number of heightbyte figures.
				matrixDelta[i] = new u8[heightInBytes];
				matrixFinal[i] = new u8[heightInBytes];
			}

			u8* transLocations[widthBucket1];// OK
			for (auto i = 0; i < widthBucket1; ++i) {
				transLocations[i] = new u8[receiverSize_To * locationInBytes + sizeof(u32)];
			}
		
			block randomLocations[bucket1];//The same as sender.

			u8* transHashInputs[width];//receiver trans.
			for (auto i = 0; i < width; ++i) {
				transHashInputs[i] = new u8[receiverSizeInBytes];
				memset(transHashInputs[i], 0, receiverSizeInBytes);
			}

			// std::cout << "Receiver initialized\n";
			timer.setTimePoint("Receiver initialized");

			


			/////////// Transform input /////////////////////

			commonPrng.get((u8*)&commonKey, sizeof(block));
			commonAes.setKey(commonKey);
			
			block* recvSet = new block[receiverSize_To];
			block* aesInput = new block[receiverSize_To];
			block* aesOutput = new block[receiverSize_To];
			
			RandomOracle H1(h1LengthInBytes);
			u8 h1Output[h1LengthInBytes];

			for (auto i = 0; i < receiverSize_To; ++i) {
				H1.Reset();
				H1.Update((u8*)(receiverSet.data() + i), sizeof(block));
				H1.Final(h1Output);
				
				aesInput[i] = *(block*)h1Output;
				recvSet[i] = *(block*)(h1Output + sizeof(block));
			}
			
			commonAes.ecbEncBlocks(aesInput, receiverSize_To, aesOutput);
			for (auto i = 0; i < receiverSize_To; ++i) {
				recvSet[i] ^= aesOutput[i];
			}
			
			// std::cout << "Receiver set transformed\n";
			timer.setTimePoint("Receiver set transformed");


			// // 写入Matrix D into文件 output_D.txt.
			int Selflen2=SelfheightInBytes;
			int len2 = heightInBytes;
			int len1 = len2*widthBucket1;
			int len = len1*widthdivide;
			int lenr = receiverSize_To*hashLengthInBytes;
			int lenhashBatch=hashLengthInBytes*bucket2;
			// 打开文件
			int fd=open("/tmp/output_D1.txt", O_RDWR|O_CREAT, 00777);
			int fd_a=open("/tmp/output_A1.txt", O_RDWR|O_CREAT, 00777);
			int fd_r=open("/tmp/output_R1.txt", O_RDWR|O_CREAT, 00777);
			u8* addr;
			u8* addr_a;
			u8* addr_r; 
			lseek(fd,len-1,SEEK_END);
			lseek(fd_a,len-1,SEEK_END);  
			lseek(fd_r,lenr-1,SEEK_END); 
			// 从指针处写入一个空字符；mmap不能扩展文件长度，这里相当于预先给文件长度，准备一个空架子
			write(fd, "", 1);
			write(fd_a, "", 1);
			write(fd_r, "", 1);
			// write(fd_s, "", 1);
			// 使用mmap函数建立内存映射
			addr = (u8*)mmap(NULL, len, PROT_READ|PROT_WRITE,MAP_SHARED, fd, 0);
			addr_a = (u8*)mmap(NULL, len, PROT_READ|PROT_WRITE,MAP_SHARED, fd_a, 0);
			addr_r = (u8*)mmap(NULL, lenr, PROT_READ|PROT_WRITE,MAP_SHARED, fd_r, 0);
			int t=0;
			for (auto wLeft = 0; wLeft < width; wLeft += widthBucket1) {
				auto wRight = wLeft + widthBucket1 < width ? wLeft + widthBucket1 : width;
				auto w = wRight - wLeft;
				t++;
				//////////// Compute random locations (transposed) ////////////////
				commonPrng.get((u8*)&commonKey, sizeof(block));
				commonAes.setKey(commonKey);

				for (auto low = 0; low < receiverSize_To; low += bucket1) {
				
					auto up = low + bucket1 < receiverSize_To ? low + bucket1 : receiverSize_To;

					commonAes.ecbEncBlocks(recvSet + low, up - low, randomLocations); 
						
					for (auto i = 0; i < w; ++i) {
						for (auto j = low; j < up; ++j) {
							memcpy(transLocations[i] + j * locationInBytes, (u8*)(randomLocations + (j - low)) + i * locationInBytes, locationInBytes);
						}
					}
				}
				// std::cout <<t<< " Receiver is 0\n";
			

				//////////// Compute matrix Delta /////////////////////////////////
				
				for (auto i = 0; i < widthBucket1; ++i) {
					memset(matrixDelta[i], 255, heightInBytes);
				}
				// std::cout <<t<< " Receiver is 1\n";
				for (auto i = 0; i < w; ++i) {
					for (auto j = 0; j < receiverSize_To; ++j) {
						auto location = (*(u32*)(transLocations[i] + j * locationInBytes)) & shift;//location means the truly location in present of 2-bits.
						
						matrixDelta[i][location >> 3] &= ~(1 << (location & 7));//matrix[i][j]={0-255},location & 7 means the truly changed location between{0-255} 
					}
				}
				// std::cout <<t<< " Receiver is 2\n";
				for(int k=1;k<widthBucket1+1;k++){
					memcpy(addr+(k-1)*len2+(t-1)*len1, matrixDelta[k-1], len2);
					// std::cout <<k<< " OK\n";
				}
				
				//////////////// Compute matrix A & sent matrix ///////////////////////

				// memcpy(matrixDelta,addr+(t-1)*len1,len1);

				u8* sentMatrix[w];
				
				for (auto i = 0; i < w; ++i) {
					PRNG prng(otMessages[i + wLeft][0]);
					prng.get(matrixA[i], heightInBytes);
					
					sentMatrix[i] = new u8[heightInBytes];
					prng.SetSeed(otMessages[i + wLeft][1]);
					prng.get(sentMatrix[i], heightInBytes);
					
					for (auto j = 0; j < heightInBytes; ++j) {
						sentMatrix[i][j] ^= matrixA[i][j] ^ matrixDelta[i][j];//A^D^r1
					}
					
					ch.asyncSend(sentMatrix[i], heightInBytes);
					
				}
				for(int k=1;k<widthBucket1+1;k++){
					memcpy(addr_a+(k-1)*len2+(t-1)*len1, matrixA[k-1], len2);
				}

				///////////////// Compute hash inputs (transposed) /////////////////////
		
				for (auto i = 0; i < w; ++i) {
					for (auto j = 0; j < receiverSize_To; ++j) {
						auto location = (*(u32*)(transLocations[i] + j * locationInBytes)) & shift;
						
						transHashInputs[i + wLeft][j >> 3] |= (u8)((bool)(matrixA[i][location >> 3] & (1 << (location & 7)))) << (j & 7);
					}		
				}
			}
			

			// 内存映射建立好了，此时可以关闭文件了, Put This into Final
			close(fd);
			close(fd_a);
			munmap(addr, len);
			munmap(addr_a, len);

			// std::cout << "Receiver matrix sent and transposed hash input computed\n";
			timer.setTimePoint("Receiver matrix sent and transposed hash input computed");
			
			/////////////////// Compute hash outputs ///////////////////////////
			
			RandomOracle H(hashLengthInBytes);
			u8 hashOutput[sizeof(block)];
			std::unordered_map<u64, std::vector<std::pair<block, u32>>> allHashes;
			u8* hashInputs[bucket2];
			for (auto i = 0; i < bucket2; ++i) {
				hashInputs[i] = new u8[widthInBytes];
			}

			int s1=0;
			for (auto low = 0; low < receiverSize_To; low += bucket2) {
				auto up = low + bucket2 < receiverSize_To ? low + bucket2 : receiverSize_To;
				s1++;
				for (auto j = low; j < up; ++j) {
					memset(hashInputs[j - low], 0, widthInBytes);
				}
				
				for (auto i = 0; i < width; ++i) {
					for (auto j = low; j < up; ++j) {
						hashInputs[j - low][i >> 3] |= (u8)((bool)(transHashInputs[i][j >> 3] & (1 << (j & 7)))) << (i & 7);//transform to output of 0|1|0|0|1|...
					}
				}
				
				for (auto j = low; j < up; ++j) {
					H.Reset();
					H.Update(hashInputs[j - low], widthInBytes);
					H.Final(hashOutput);
					memcpy(addr_r+(j-low)*hashLengthInBytes+(s1-1)*lenhashBatch, hashOutput, hashLengthInBytes);
					allHashes[*(u64*)hashOutput].push_back(std::make_pair(*(block*)hashOutput, j));
				}
			}

			// 内存映射建立好了，此时可以关闭文件了, Put This into Final
			close(fd_r);
			// 解除映射
			munmap(addr_r, lenr);
			
			// std::cout << "Receiver hash outputs computed\n";
			timer.setTimePoint("Receiver hash outputs computed");

			///////////////// Receive hash outputs from sender and compute PSI ///////////////////

			u8* recvBuff = new u8[bucket2 * hashLengthInBytes];//output0001|1111|1010|... to 01010101010101010
			
			auto psi = 0;
			
			for (auto low = 0; low < senderSize_To; low += bucket2) {
				auto up = low + bucket2 < senderSize_To ? low + bucket2 : senderSize_To;
				
				ch.recv(recvBuff, (up - low) * hashLengthInBytes);
				
				for (auto idx = 0; idx < up - low; ++idx) {
					u64 mapIdx = *(u64*)(recvBuff + idx * hashLengthInBytes);
					
					auto found = allHashes.find(mapIdx);
					if (found == allHashes.end()) continue;
					
					for (auto i = 0; i < found->second.size(); ++i) {
						if (memcmp(&(found->second[i].first), recvBuff + idx * hashLengthInBytes, hashLengthInBytes) == 0) {
							++psi;
							break;
						}
					}
				}
			}
			
			std::cout << "Receiver intersection computed"<< psi <<"- correct!\n";
			timer.setTimePoint("Receiver intersection computed");
			
			
			std::cout << timer;
				
			//////////////// Output communication /////////////////
		
			sentData = ch.getTotalDataSent();
			recvData = ch.getTotalDataRecv();
			totalData = sentData + recvData;
			
			std::cout << "Receiver sent communication: " << sentData / std::pow(2.0, 20) << " MB\n";
			std::cout << "Receiver received communication: " << recvData / std::pow(2.0, 20) << " MB\n";
			std::cout << "Receiver total communication: " << totalData / std::pow(2.0, 20) << " MB\n";
				
			}
		}
		////////////Syn the Time--------------/////////////		
		u8* SynTime = new u8[hashLengthInBytes];		
		memset( SynTime, 0, hashLengthInBytes);
		ch.asyncSend(SynTime, hashLengthInBytes);//yipiyipi send to receiver.
		ch.recv(SynTime, hashLengthInBytes);
		///////////Syn the TIME ///////////////////////////

	}

	void PsiSender::runNext(PRNG& prng, Channel& ch, block commonSeed, const u64& senderSize, const u64& receiverSize, const u64& height, const u64& logHeight, const u64& width, std::vector<block>& senderSet, const u64& hashLengthInBytes, const u64& h1LengthInBytes, const u64& bucket1, const u64& bucket2, const u64& senderSize1, const u64& receiverSize1, BitVector choices, const u64& height1, const u64& logHeight1, const u64& width1, const u64& hashLengthInBytes1) {
	for(int w=0;w<3;w++){
		if(w==0){
			////////////Syn the Time--------------/////////////
			u8* SynTime = new u8[hashLengthInBytes];		
			memset( SynTime, 0, hashLengthInBytes);
			ch.asyncSend(SynTime, hashLengthInBytes);//yipiyipi send to receiver.
			ch.recv(SynTime, hashLengthInBytes);
			///////////Syn the TIME ///////////////////////////
			Timer timer;
			timer.setTimePoint("Sender start");
			
			auto heightInBytes = (height + 7) / 8;
			auto widthInBytes = (width + 7) / 8;
			auto locationInBytes = (logHeight + 7) / 8;//0001|1110|0101|1010|...
			auto senderSizeInBytes = (senderSize1 + 7) / 8;
			auto shift = (1 << logHeight) - 1;
			auto widthBucket1 = sizeof(block) / locationInBytes;//which is familier with this as 0001|1100|0101|1000|...|A Batchnorm
			int widthdivide=width/widthBucket1+1;

			//------It is different from last time databases.-----------
			PRNG prng2(oc::toBlock(314));//means a random feed in "123".
			for (auto i = 0; i < 100; ++i) {
				senderSet[i] = prng2.get<block>();
			}
			PRNG prng1(oc::toBlock(561));//means a random feed in "123".
			for (auto i = 100; i < senderSize1; ++i) {
				senderSet[i] = prng1.get<block>();
			}
			////////////// Initialization //////////////////////
			
			PRNG commonPrng(commonSeed);
			block commonKey;
			AES commonAes;
			
			u8* transLocations[widthBucket1];
			for (auto i = 0; i < widthBucket1; ++i) {
				transLocations[i] = new u8[senderSize1 * locationInBytes + sizeof(u32)];//everysender have locationInBytes
			}
			
			block randomLocations[bucket1];//ok

			u8* matrixC[widthBucket1];//also yipiyipi kuangdu.
			for (auto i = 0; i < widthBucket1; ++i) {
				matrixC[i] = new u8[heightInBytes];
			}

			//transHashInputs[sender][width]=0|1|0|0|1|1|0|...
			u8* transHashInputs[width];
			for (auto i = 0; i < width; ++i) {
				transHashInputs[i] = new u8[senderSizeInBytes];
				memset(transHashInputs[i], 0, senderSizeInBytes);
			}

			// std::cout << "Sender initialed\n";
			timer.setTimePoint("Sender initialed");

			/////////// Transform input /////////////////////

			commonPrng.get((u8*)&commonKey, sizeof(block));
			commonAes.setKey(commonKey);
			
			block* sendSet = new block[senderSize1];
			block* aesInput = new block[senderSize1];
			block* aesOutput = new block[senderSize1];
			
			RandomOracle H1(h1LengthInBytes);
			u8 h1Output[h1LengthInBytes];
			
			for (auto i = 0; i < senderSize1; ++i) {
				H1.Reset();
				H1.Update((u8*)(senderSet.data() + i), sizeof(block));
				H1.Final(h1Output);
				
				aesInput[i] = *(block*)h1Output;
				sendSet[i] = *(block*)(h1Output + sizeof(block));
			}
			commonAes.ecbEncBlocks(aesInput, senderSize1, aesOutput);
			for (auto i = 0; i < senderSize1; ++i) {
				sendSet[i] ^= aesOutput[i];
			}
			
			// std::cout << "Sender set transformed\n";
			timer.setTimePoint("Sender set transformed");


			// // 写入Matrix C into文件 output_C.txt.
			int len2 = heightInBytes;
			int len1 = len2*widthBucket1;
			int len = len1*widthdivide;
			// 打开文件
			int fd_c=open("/tmp/output_C.txt", O_RDWR|O_CREAT, 00777);
			u8* addr_c;
			// 使用mmap函数建立内存映射
			addr_c = (u8*)mmap(NULL, len, PROT_READ|PROT_WRITE,MAP_SHARED, fd_c, 0);
			int t=0;
			for (auto wLeft = 0; wLeft < width; wLeft += widthBucket1) {
				auto wRight = wLeft + widthBucket1 < width ? wLeft + widthBucket1 : width;
				auto w = wRight - wLeft;
				t++;

				for(int k=1;k<widthBucket1+1;k++){
					memcpy(matrixC[k-1], addr_c+(k-1)*len2+(t-1)*len1, len2);
				}

				//////////// Compute random locations (transposed) ////////////////

				commonPrng.get((u8*)&commonKey, sizeof(block));
				commonAes.setKey(commonKey);
				
				for (auto low = 0; low < senderSize1; low += bucket1) {
				
					auto up = low + bucket1 < senderSize1 ? low + bucket1 : senderSize1;
				
					commonAes.ecbEncBlocks(sendSet + low, up - low, randomLocations);//Fkey(h1) truly.

					for (auto i = 0; i < w; ++i) {
						for (auto j = low; j < up; ++j) {
							// Final i know what's the mean it is.randomLocations + (j - low) means 0001|1110|...|
							//every row have j * locationInBytes,then can be used easyly.
							//(u8*)(randomLocations + (j - low)) + i * locationInBytes means randomLocations[i][jfrom 0 to locationInBytes] copy to transLocations[i][jfrom 0 to locationInBytes]
							memcpy(transLocations[i] + j * locationInBytes, (u8*)(randomLocations + (j - low)) + i * locationInBytes, locationInBytes);//dazhigaodongleyitu.
						}
					}
				}

				///////////////// Compute hash inputs (transposed) /////////////////////
				
				for (auto i = 0; i < w; ++i) {
					for (auto j = 0; j < senderSize1; ++j) {
						auto location = (*(u32*)(transLocations[i] + j * locationInBytes)) & shift;
						//it is really excellent transHashInputs ments the senderSize*w outputs.
						transHashInputs[i + wLeft][j >> 3] |= (u8)((bool)(matrixC[i][location >> 3] & (1 << (location & 7)))) << (j & 7);
					}		
				}
				
			}

			// 内存映射建立好了，此时可以关闭文件了
			close(fd_c);
			munmap(addr_c, len);
			timer.setTimePoint("Sender transposed hash input computed");

			/////////////////// Compute hash outputs ///////////////////////////
			
			RandomOracle H(hashLengthInBytes);
			u8 hashOutput[sizeof(block)];

			u8* hashInputs[bucket2];
			
			for (auto i = 0; i < bucket2; ++i) {
				hashInputs[i] = new u8[widthInBytes];
			}
			
			for (auto low = 0; low < senderSize1; low += bucket2) {
				auto up = low + bucket2 < senderSize1 ? low + bucket2 : senderSize1;
				
				for (auto j = low; j < up; ++j) {
					memset(hashInputs[j - low], 0, widthInBytes);
				}
				
				for (auto i = 0; i < width; ++i) {
					for (auto j = low; j < up; ++j) {
						hashInputs[j - low][i >> 3] |= (u8)((bool)(transHashInputs[i][j >> 3] & (1 << (j & 7)))) << (i & 7);//means * 2^(i & 7),that is OK!
					}
				}
				
				u8* sentBuff = new u8[(up - low) * hashLengthInBytes];
				
				for (auto j = low; j < up; ++j) {
					H.Reset();
					H.Update(hashInputs[j - low], widthInBytes);
					H.Final(hashOutput);
					
					memcpy(sentBuff + (j - low) * hashLengthInBytes, hashOutput, hashLengthInBytes);
				}
				
				ch.asyncSend(sentBuff, (up - low) * hashLengthInBytes);//yipiyipi send to receiver.
			}
			
			// std::cout << "Sender hash outputs computed and sent\n";
			timer.setTimePoint("Sender hash outputs computed and sent");

			std::cout << timer;

		}else if(w==1){
				///////////Syn the TIME ///////////////////////////
				u8* SynTime = new u8[hashLengthInBytes];
				ch.recv(SynTime, hashLengthInBytes);
				ch.asyncSend(SynTime, hashLengthInBytes);//yipiyipi send to receive
				///////////Syn the TIME ///////////////////////////
				Timer timer;
				timer.setTimePoint("Receiver start");
				TimeUnit start, end;
				u64 senderSize1_To;
				u64 receiverSize1_To;
				u64 receiverSize_To;
				senderSize1_To=receiverSize1;
				receiverSize1_To=senderSize1;
				receiverSize_To=senderSize;

				cout<<"\n\n\n---------------------------------------The prepare process---------------------------------------\n";
				cout<<"---------------------------------------The prepare process---------------------------------------\n";
				cout<<"---------------------------------------The prepare process---------------------------------------\n";
				cout<<"---------------------------------------The prepare process---------------------------------------\n";
				cout<<"---------------------------------------The prepare process---------------------------------------\n\n\n";

				u64 sentData = ch.getTotalDataSent();
				u64 recvData = ch.getTotalDataRecv();
				u64 totalData = sentData + recvData;
				
				timer.setTimePoint("Receiver initialized");
	
				/////////////////// Compute hash outputs ///////////////////////////

				RandomOracle H(hashLengthInBytes);
				u8 hashOutput[sizeof(block)];
				std::unordered_map<u64, std::vector<std::pair<block, u32>>> allHashes;

				// // 写入Matrix D into文件 output_D.txt.
				int lenr = receiverSize_To*hashLengthInBytes;
				int lenhashBatch=hashLengthInBytes*bucket2;
				// int len2 = sizeof(matrixDelta[0]);
				// 打开文件
				int fd_r=open("/tmp/output_R1.txt", O_RDWR|O_CREAT, 00777);
				// int fd=open("/home/qiqiang6/Desktop/EX_PSIOT_Ex/OPRF-PSI/PSI/Output/output.txt", O_RDWR, 0644);
				u8* addr_r;
				lseek(fd_r,lenr-1,SEEK_END); 
				// 从指针处写入一个空字符；mmap不能扩展文件长度，这里相当于预先给文件长度，准备一个空架子
				write(fd_r, "", 1);
				// 使用mmap函数建立内存映射
				addr_r = (u8*)mmap(NULL, lenr, PROT_READ|PROT_WRITE,MAP_SHARED, fd_r, 0);

				// int s1=0;
				u8* recvBuff = new u8[bucket2 * hashLengthInBytes];//output0001|1111|1010|... to 01010101010101010
				for (auto low = 0; low < senderSize1_To; low += bucket2) {
					auto up = low + bucket2 < senderSize1_To ? low + bucket2 : senderSize1_To;
					// s1++;
					ch.recv(recvBuff, (up - low) * hashLengthInBytes);
					for (auto j = low; j < up; ++j) {
						memcpy(hashOutput, recvBuff+(j-low)*hashLengthInBytes, hashLengthInBytes);
						allHashes[*(u64*)hashOutput].push_back(std::make_pair(*(block*)hashOutput, j));
					}
				}


				
				// std::cout << "Receiver hash outputs computed\n";
				timer.setTimePoint("Receiver hash outputs computed");

				///////////////// Receive hash outputs from sender and compute PSI ///////////////////


				
				auto psi = 0;
				int s1=0;
				for (auto low = 0; low < receiverSize_To; low += bucket2) {
					auto up = low + bucket2 < receiverSize_To ? low + bucket2 : receiverSize_To;
					s1++;
					// memcpy(hashOutput, addr_r+(j-low)*hashLengthInBytes+(s1-1)*lenhashBatch, hashLengthInBytes);
					memcpy(recvBuff, addr_r+(s1-1)*(up - low) * hashLengthInBytes, (up - low) * hashLengthInBytes);
					for (auto idx = 0; idx < up - low; ++idx) {
						u64 mapIdx = *(u64*)(recvBuff + idx * hashLengthInBytes);
						
						auto found = allHashes.find(mapIdx);
						if (found == allHashes.end()) continue;
						
						for (auto i = 0; i < found->second.size(); ++i) {
							if (memcmp(&(found->second[i].first), recvBuff + idx * hashLengthInBytes, hashLengthInBytes) == 0) {
								++psi;
								break;
							}
						}
					}
				}
				// 内存映射建立好了，此时可以关闭文件了, Put This into Final
				close(fd_r);
				// 解除映射
				munmap(addr_r, lenr);
				std::cout << "Receiver intersection computed"<< psi <<"- correct!\n";
				timer.setTimePoint("Receiver intersection computed");
				
				
				std::cout << timer;
					
				//////////////// Output communication /////////////////
			
				sentData = ch.getTotalDataSent();
				recvData = ch.getTotalDataRecv();
				totalData = sentData + recvData;
				
				std::cout << "Receiver sent communication: " << sentData / std::pow(2.0, 20) << " MB\n";
				std::cout << "Receiver received communication: " << recvData / std::pow(2.0, 20) << " MB\n";
				std::cout << "Receiver total communication: " << totalData / std::pow(2.0, 20) << " MB\n";

				//////////////// Next TongBu /////////////////
		}
		else{
			const block commonSeed1 = oc::toBlock(345678);
			u64 senderSize1_To;
			u64 receiverSize1_To;
			// u64 receiverSize_To;
			senderSize1_To=receiverSize1;
			receiverSize1_To=senderSize1;

			vector<block> receiverSet(receiverSize1_To); 

			//------It is different from last time databases.-----------
			PRNG prng2(oc::toBlock(314));//means a random feed in "123".
			for (auto i = 0; i < 100; ++i) {
				receiverSet[i] = prng2.get<block>();
			}
			PRNG prng1(oc::toBlock(561));//means a random feed in "123".
			for (auto i = 100; i < receiverSize1_To; ++i) {
				receiverSet[i] = prng1.get<block>();
			}
			
			// u8* sentBuff = new u8[bucket2 * hashLengthInBytes];
		//////////Syn The Time ///////////////////////////

			u8* SynTime = new u8[hashLengthInBytes];
			ch.recv(SynTime, hashLengthInBytes);
			ch.asyncSend(SynTime, hashLengthInBytes);//yipiyipi send to receiver.
		////////Syn The Time /////////////////////////////
			Timer timer;
			timer.setTimePoint("Receiver start");
			TimeUnit start, end;
			cout<<"\n\n\n---------------------------------------The prepare process---------------------------------------\n";
			cout<<"---------------------------------------The prepare process---------------------------------------\n";
			cout<<"---------------------------------------The prepare process---------------------------------------\n";
			cout<<"---------------------------------------The prepare process---------------------------------------\n";
			cout<<"---------------------------------------The prepare process---------------------------------------\n\n\n";
			auto heightInBytes = (height1 + 7) / 8;
			auto SelfheightInBytes = (height1 + 7) / 16;
			auto widthInBytes = (width1 + 7) / 8;
			auto locationInBytes = (logHeight1 + 7) / 8;
			auto receiverSizeInBytes = (receiverSize1_To + 7) / 8;
			auto shift = (1 << logHeight1) - 1;
			auto widthBucket1 = sizeof(block) / locationInBytes;
			int widthdivide=width1/widthBucket1+1;

			u64 sentData = ch.getTotalDataSent();
			u64 recvData = ch.getTotalDataRecv();
			u64 totalData = sentData + recvData;
					
			
			///////////////////// Base OTs ///////////////////////////
			
			IknpOtExtSender otExtSender;
			otExtSender.genBaseOts(prng1, ch);
			
			std::vector<std::array<block, 2> > otMessages(width1);

			otExtSender.send(otMessages, prng1, ch);

			// std::cout << "Receiver base OT finished\n";
			timer.setTimePoint("Receiver base OT finished");
			
			//////////// Initialization ///////////////////
			
			PRNG commonPrng(commonSeed1);
			block commonKey;
			AES commonAes;
			
			u8* matrixA[widthBucket1];// a batchnorm
			u8* matrixDelta[widthBucket1];
			u8* matrixFinal[widthBucket1];
			// matrixDelta[0];
			for (auto i = 0; i < widthBucket1; ++i) {
				matrixA[i] = new u8[heightInBytes];//every have the number of heightbyte figures.
				matrixDelta[i] = new u8[heightInBytes];
				matrixFinal[i] = new u8[heightInBytes];
			}

			u8* transLocations[widthBucket1];// OK
			for (auto i = 0; i < widthBucket1; ++i) {
				transLocations[i] = new u8[receiverSize1_To * locationInBytes + sizeof(u32)];
			}
		
			block randomLocations[bucket1];//The same as sender.

			u8* transHashInputs[width1];//receiver trans.
			for (auto i = 0; i < width1; ++i) {
				transHashInputs[i] = new u8[receiverSizeInBytes];
				memset(transHashInputs[i], 0, receiverSizeInBytes);
			}

			// std::cout << "Receiver initialized\n";
			timer.setTimePoint("Receiver initialized");

			


			/////////// Transform input /////////////////////

			commonPrng.get((u8*)&commonKey, sizeof(block));
			commonAes.setKey(commonKey);
			
			block* recvSet = new block[receiverSize1_To];
			block* aesInput = new block[receiverSize1_To];
			block* aesOutput = new block[receiverSize1_To];
			
			RandomOracle H1(h1LengthInBytes);
			u8 h1Output[h1LengthInBytes];

			for (auto i = 0; i < receiverSize1_To; ++i) {
				H1.Reset();
				H1.Update((u8*)(receiverSet.data() + i), sizeof(block));
				H1.Final(h1Output);
				
				aesInput[i] = *(block*)h1Output;
				recvSet[i] = *(block*)(h1Output + sizeof(block));
			}
			
			commonAes.ecbEncBlocks(aesInput, receiverSize1_To, aesOutput);
			for (auto i = 0; i < receiverSize1_To; ++i) {
				recvSet[i] ^= aesOutput[i];
			}
			
			timer.setTimePoint("Receiver set transformed");


			int t=0;
			for (auto wLeft = 0; wLeft < width1; wLeft += widthBucket1) {
				auto wRight = wLeft + widthBucket1 < width1 ? wLeft + widthBucket1 : width1;
				auto w = wRight - wLeft;
				t++;
				// cout<<"The number of widthbuckets is "<<t<<"\n";
				//////////// Compute random locations (transposed) ////////////////
				commonPrng.get((u8*)&commonKey, sizeof(block));
				commonAes.setKey(commonKey);

				for (auto low = 0; low < receiverSize1_To; low += bucket1) {
				
					auto up = low + bucket1 < receiverSize1_To ? low + bucket1 : receiverSize1_To;

					commonAes.ecbEncBlocks(recvSet + low, up - low, randomLocations); 
						
					for (auto i = 0; i < w; ++i) {
						for (auto j = low; j < up; ++j) {
							memcpy(transLocations[i] + j * locationInBytes, (u8*)(randomLocations + (j - low)) + i * locationInBytes, locationInBytes);
						}
					}
				}


				//////////// Compute matrix Delta /////////////////////////////////
				
				for (auto i = 0; i < widthBucket1; ++i) {
					memset(matrixDelta[i], 255, heightInBytes);
				}
				// std::cout <<t<< " Receiver is 1\n";
				for (auto i = 0; i < w; ++i) {
					for (auto j = 0; j < receiverSize1_To; ++j) {
						auto location = (*(u32*)(transLocations[i] + j * locationInBytes)) & shift;//location means the truly location in present of 2-bits.
						
						matrixDelta[i][location >> 3] &= ~(1 << (location & 7));//matrix[i][j]={0-255},location & 7 means the truly changed location between{0-255} 
					}
				}
				
				
				//////////////// Compute matrix A & sent matrix ///////////////////////

				u8* sentMatrix[w];
				for (auto i = 0; i < w; ++i) {
					PRNG prng(otMessages[i + wLeft][0]);
					prng.get(matrixA[i], heightInBytes);
					
					sentMatrix[i] = new u8[heightInBytes];
					prng.SetSeed(otMessages[i + wLeft][1]);
					prng.get(sentMatrix[i], heightInBytes);
					
					for (auto j = 0; j < heightInBytes; ++j) {
						sentMatrix[i][j] ^= matrixA[i][j] ^ matrixDelta[i][j];//A^D^r1
					}
					
					ch.asyncSend(sentMatrix[i], heightInBytes);
					
				}


				///////////////// Compute hash inputs (transposed) /////////////////////
		
				for (auto i = 0; i < w; ++i) {
					for (auto j = 0; j < receiverSize1_To; ++j) {
						auto location = (*(u32*)(transLocations[i] + j * locationInBytes)) & shift;
						
						transHashInputs[i + wLeft][j >> 3] |= (u8)((bool)(matrixA[i][location >> 3] & (1 << (location & 7)))) << (j & 7);
					}		
				}
			}

			timer.setTimePoint("Receiver matrix sent and transposed hash input computed");
			
			/////////////////// Compute hash outputs ///////////////////////////
			
			RandomOracle H(hashLengthInBytes1);
			u8 hashOutput[sizeof(block)];
			std::unordered_map<u64, std::vector<std::pair<block, u32>>> allHashes;
			u8* hashInputs[bucket2];
			for (auto i = 0; i < bucket2; ++i) {
				hashInputs[i] = new u8[widthInBytes];
			}

			int s1=0;
			for (auto low = 0; low < receiverSize1_To; low += bucket2) {
				auto up = low + bucket2 < receiverSize1_To ? low + bucket2 : receiverSize1_To;
				s1++;
				for (auto j = low; j < up; ++j) {
					memset(hashInputs[j - low], 0, widthInBytes);
				}
				
				for (auto i = 0; i < width1; ++i) {
					for (auto j = low; j < up; ++j) {
						hashInputs[j - low][i >> 3] |= (u8)((bool)(transHashInputs[i][j >> 3] & (1 << (j & 7)))) << (i & 7);//transform to output of 0|1|0|0|1|...
					}
				}
				
				for (auto j = low; j < up; ++j) {
					H.Reset();
					H.Update(hashInputs[j - low], widthInBytes);
					H.Final(hashOutput);
					// memcpy(addr_r+(j-low)*hashLengthInBytes1+(s1-1)*lenhashBatch, hashOutput, hashLengthInBytes1);
					allHashes[*(u64*)hashOutput].push_back(std::make_pair(*(block*)hashOutput, j));
				}
			}

			timer.setTimePoint("Receiver hash outputs computed");

			///////////////// Receive hash outputs from sender and compute PSI ///////////////////

			u8* recvBuff = new u8[bucket2 * hashLengthInBytes1];//output0001|1111|1010|... to 01010101010101010
			
			auto psi = 0;
			
			for (auto low = 0; low < senderSize1_To; low += bucket2) {
				auto up = low + bucket2 < senderSize1_To ? low + bucket2 : senderSize1_To;
				
				ch.recv(recvBuff, (up - low) * hashLengthInBytes1);
				
				for (auto idx = 0; idx < up - low; ++idx) {
					u64 mapIdx = *(u64*)(recvBuff + idx * hashLengthInBytes1);
					
					auto found = allHashes.find(mapIdx);
					if (found == allHashes.end()) continue;
					
					for (auto i = 0; i < found->second.size(); ++i) {
						if (memcmp(&(found->second[i].first), recvBuff + idx * hashLengthInBytes1, hashLengthInBytes1) == 0) {
							++psi;
							break;
						}
					}
				}
			}
			std::cout << "Receiver intersection computed"<< psi <<"- correct!\n";
			timer.setTimePoint("Receiver intersection computed");
			
			
			std::cout << timer;
				
			//////////////// Output communication /////////////////
		
			sentData = ch.getTotalDataSent();
			recvData = ch.getTotalDataRecv();
			totalData = sentData + recvData;
			
			std::cout << "Receiver sent communication: " << sentData / std::pow(2.0, 20) << " MB\n";
			std::cout << "Receiver received communication: " << recvData / std::pow(2.0, 20) << " MB\n";
			std::cout << "Receiver total communication: " << totalData / std::pow(2.0, 20) << " MB\n";
				
			}
		}
		////////////Syn the Time--------------/////////////
		u8* SynTime = new u8[hashLengthInBytes];		
		memset( SynTime, 0, hashLengthInBytes);
		ch.asyncSend(SynTime, hashLengthInBytes);//yipiyipi send to receiver.
		ch.recv(SynTime, hashLengthInBytes);
		///////////Syn the TIME ///////////////////////////

	}



}
