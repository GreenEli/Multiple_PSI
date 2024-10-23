#include "PsiReceiver.h"
#include<sys/mman.h>
#include <array>
#include <vector>
#include <iostream>
#include <cstdlib>
#include <unordered_map>
#include <iomanip>
#include <bitset>
#include <thread>

using namespace std;

namespace PSI {

	void PsiReceiver::runOffline(PRNG& prng, Channel& ch, block commonSeed, const u64& senderSize, const u64& receiverSize, const u64& height, const u64& logHeight, const u64& width, std::vector<block>& receiverSet, const u64& hashLengthInBytes, const u64& h1LengthInBytes, const u64& bucket1, const u64& bucket2, const u64& senderSize1, const u64& receiverSize1) {
	
		for(int w=0;w<2;w++){
			if(w==0){
				Timer timer;
				timer.setTimePoint("Receiver start");
				TimeUnit start, end;
				char *savePath = "/tmp/output_D.txt";
				char *savePath_a = "/tmp/output_A.txt";
				// char *savePath_Sender = "/home/qiqiang6/Desktop/EX_PSIOT_Ex/OPRF-PSI/PSI/Output/output_S.txt";
				char *savePath_Receiver = "/tmp/output_R.txt";
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
				// if(locationInBytes==4){
				// 	locationInBytes=3;
				// }
				// auto receiverSizeInBytes = (receiverSize + 7) / 8;
				auto receiverSizeInBytes = (receiverSize + 7) / 8;
				auto shift = (1 << logHeight) - 1;
				auto widthBucket1 = sizeof(block) / locationInBytes;
				int widthdivide=width/widthBucket1+1;

				u64 sentData = ch.getTotalDataSent();
				u64 recvData = ch.getTotalDataRecv();
				u64 totalData = sentData + recvData;
				
				// std::cout << "Receiver sent communication: " << sentData / std::pow(2.0, 20) << " MB\n";
				// std::cout << "Receiver received communication: " << recvData / std::pow(2.0, 20) << " MB\n";
				// std::cout << "BaseOT_Before_Receiver total communication: " << totalData / std::pow(2.0, 20) << " MB\n";		
				
				///////////////////// Base OTs ///////////////////////////
				
				IknpOtExtSender otExtSender;
				otExtSender.genBaseOts(prng, ch);
				
				std::vector<std::array<block, 2> > otMessages(width);

				otExtSender.send(otMessages, prng, ch);

				// std::cout << "Receiver base OT finished\n";
				timer.setTimePoint("Receiver base OT finished");

		// TEST ------------------------------------------------------------------------------
				
				// sentData = ch.getTotalDataSent();
				// recvData = ch.getTotalDataRecv();
				// totalData = sentData + recvData;
				
				// // std::cout << "Receiver sent communication: " << sentData / std::pow(2.0, 20) << " MB\n";
				// // std::cout << "Receiver received communication: " << recvData / std::pow(2.0, 20) << " MB\n";
				// std::cout << "BaseOT_Later_Receiver total communication: " << totalData / std::pow(2.0, 20) << " MB\n";
		// TEST ------------------------------------------------------------------------------- 
				
				//////////// Initialization ///////////////////
				
				PRNG commonPrng(commonSeed);
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
					transLocations[i] = new u8[receiverSize * locationInBytes + sizeof(u32)];
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
				
				block* recvSet = new block[receiverSize];
				block* aesInput = new block[receiverSize];
				block* aesOutput = new block[receiverSize];
				
				RandomOracle H1(h1LengthInBytes);
				u8 h1Output[h1LengthInBytes];

				for (auto i = 0; i < receiverSize; ++i) {
					H1.Reset();
					H1.Update((u8*)(receiverSet.data() + i), sizeof(block));
					H1.Final(h1Output);
					
					aesInput[i] = *(block*)h1Output;
					recvSet[i] = *(block*)(h1Output + sizeof(block));
				}
				
				commonAes.ecbEncBlocks(aesInput, receiverSize, aesOutput);
				for (auto i = 0; i < receiverSize; ++i) {
					recvSet[i] ^= aesOutput[i];
				}
				
				// std::cout << "Receiver set transformed\n";
				timer.setTimePoint("Receiver set transformed");


				// // 写入Matrix D into文件 output_D.txt.
				int Selflen2=SelfheightInBytes;
				int len2 = heightInBytes;
				int len1 = len2*widthBucket1;
				int len = len1*widthdivide;
				int lenr = receiverSize*hashLengthInBytes;
				int lenhashBatch=hashLengthInBytes*bucket2;
				// int lens = senderSize*hashLengthInBytes;
				// int len2 = sizeof(matrixDelta[0]);
				// 打开文件
				int fd=open("/tmp/output_D.txt", O_RDWR|O_CREAT, 00777);
				int fd_a=open("/tmp/output_A.txt", O_RDWR|O_CREAT, 00777);
				int fd_r=open("/tmp/output_R.txt", O_RDWR|O_CREAT, 00777);
				// int fd_s=open("/home/qiqiang6/Desktop/EX_PSIOT_Ex/OPRF-PSI/PSI/Output/output_S.txt", O_RDWR|O_CREAT, 00777);
				// int fd=open("/home/qiqiang6/Desktop/EX_PSIOT_Ex/OPRF-PSI/PSI/Output/output.txt", O_RDWR, 0644);
				u8* addr;
				u8* addr_a;
				u8* addr_r;
				// u8* addr_s;
				// u8* addr1; 
				// u8* addr2;
				// lseek将文件指针往后移动file_size-1位
				// lseek(fd,len-1,SEEK_END);
				// lseek(fd_a,len-1,SEEK_END);   
				lseek(fd,len-1,SEEK_END);
				lseek(fd_a,len-1,SEEK_END);  
				lseek(fd_r,lenr-1,SEEK_END); 
				// lseek(fd_s,lens-1,SEEK_END); 
				// 从指针处写入一个空字符；mmap不能扩展文件长度，这里相当于预先给文件长度，准备一个空架子
				write(fd, "", 1);
				write(fd_a, "", 1);
				write(fd_r, "", 1);
				// write(fd_s, "", 1);
				// 使用mmap函数建立内存映射
				addr = (u8*)mmap(NULL, len, PROT_READ|PROT_WRITE,MAP_SHARED, fd, 0);
				// addr1 = (u8*)mmap(NULL, len1, PROT_READ|PROT_WRITE,MAP_SHARED, fd, len1);
				addr_a = (u8*)mmap(NULL, len, PROT_READ|PROT_WRITE,MAP_SHARED, fd_a, 0);
				// addr2 = (u8*)mmap(NULL, len1, PROT_READ|PROT_WRITE,MAP_SHARED, fd_a, len1);
				addr_r = (u8*)mmap(NULL, lenr, PROT_READ|PROT_WRITE,MAP_SHARED, fd_r, 0);
				// addr_s = (u8*)mmap(NULL, lens, PROT_READ|PROT_WRITE,MAP_SHARED, fd_s, 0);
				int t=0;
				for (auto wLeft = 0; wLeft < width; wLeft += widthBucket1) {
					auto wRight = wLeft + widthBucket1 < width ? wLeft + widthBucket1 : width;
					auto w = wRight - wLeft;
					t++;
					// cout<<"The number of widthbuckets is "<<t<<"\n";
					//////////// Compute random locations (transposed) ////////////////
					// std::cout <<t<< " Receiver is \n";
					commonPrng.get((u8*)&commonKey, sizeof(block));
					commonAes.setKey(commonKey);

					for (auto low = 0; low < receiverSize; low += bucket1) {
					
						auto up = low + bucket1 < receiverSize ? low + bucket1 : receiverSize;

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
						for (auto j = 0; j < receiverSize; ++j) {
							auto location = (*(u32*)(transLocations[i] + j * locationInBytes)) & shift;//location means the truly location in present of 2-bits.
							
							matrixDelta[i][location >> 3] &= ~(1 << (location & 7));//matrix[i][j]={0-255},location & 7 means the truly changed location between{0-255} 
						}
					}
					// std::cout <<t<< " Receiver is 2\n";
					for(int k=1;k<widthBucket1+1;k++){
						memcpy(addr+(k-1)*len2+(t-1)*len1, matrixDelta[k-1], len2);
						// std::cout <<k<< " OK\n";
					}
					// std::cout <<t<< " Receiver is 3\n";
					// memcpy(addr+(t-1)*len1, matrixDelta, len1);
					// std::cout <<"The Compare matrixDelta[0][0] is "<< int(matrixDelta[0][0])<<"\n";
					// std::cout <<"The Compare addr[("<<len1_STR*(t-1)<<")] is "<< int(addr[0+len1_STR*(t-1)])<<"\n";
					// memcpy(addr, matrixDelta, len1);
					// memcpy(addr, matrixDelta, len1);
					// if(t<4){
					// 	std::cout <<"The Write matrixD[0+("<<(t-1)<<")][0] is "<< int(matrixDelta[0][1])<<"\n";
					// }
					// memcpy(matrixDelta,addr+(t-1)*len1,len1);
					// if(t<4){
					// 	std::cout <<"The Write_copy matrixD[0+("<<(t-1)<<")][0] is "<< int(matrixDelta[0][0])<<"\n";
					// 	// cout<<"True len is "<<len<<" and len1 is"<<len1<<" and len2 is "<<len2<<" and widthbucket is "<<widthBucket1<<" \n";
					// }
					
					
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
					// std::cout <<t<< " Receiver is 4\n";
					for(int k=1;k<widthBucket1+1;k++){
						memcpy(addr_a+(k-1)*len2+(t-1)*len1, matrixA[k-1], len2);
					}

					///////////////// Compute hash inputs (transposed) /////////////////////
			
					for (auto i = 0; i < w; ++i) {
						for (auto j = 0; j < receiverSize; ++j) {
							auto location = (*(u32*)(transLocations[i] + j * locationInBytes)) & shift;
							
							transHashInputs[i + wLeft][j >> 3] |= (u8)((bool)(matrixA[i][location >> 3] & (1 << (location & 7)))) << (j & 7);
							// if(j%1000000==0){
							// cout<<s<<" counter c2 is"<<++c2<<" \n";
							// }
						}		
					}
				}
				
				// for(int j=1;j<6;j++){
				// 	// j++;
				// 	memcpy(matrixA,addr2,len1);
				// 	std::cout <<"The Write_copy2 matrixA[0+("<<(j-1)<<")][0] is "<< int(matrixA[0][j-1])<<"\n";
				// 	// cout<<"True len is "<<len<<" and len1 is"<<len1<<" and len2 is "<<len2<<" and widthbucket is "<<widthBucket1<<" \n";
				// }

				// 内存映射建立好了，此时可以关闭文件了, Put This into Final
				close(fd);
				close(fd_a);
				// // 把data复制到addr里
				// // memcpy(addr, data, len);
				// 解除映射
				// munmap(addr, len);
				// munmap(addr_a, len);
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
				for (auto low = 0; low < receiverSize; low += bucket2) {
					auto up = low + bucket2 < receiverSize ? low + bucket2 : receiverSize;
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
				
				for (auto low = 0; low < senderSize; low += bucket2) {
					auto up = low + bucket2 < senderSize ? low + bucket2 : senderSize;
					
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
				
				// if (psi == 100) {
				// 	std::cout << "Receiver intersection computed - correct!\n";
				// }
				// else{
				std::cout << "Receiver intersection computed"<< psi <<"- correct!\n";
				// }
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
				// ch.asyncSend(sentMatrix[i], heightInBytes);
				// ch.recv(recvBuff, (up - low) * hashLengthInBytes);
				// ch.asyncSend(recvBuff, bucket2 * hashLengthInBytes);//yipiyipi send to receiver.
			}
			else{
				const block commonSeed1 = oc::toBlock(234567);
				u64 senderSize_To;
				u64 receiverSize_To;
				senderSize_To=receiverSize;
				receiverSize_To=senderSize;
				vector<block> senderSet(senderSize_To); 
				//-------------delete----------delete----------delete------------
				PRNG prng0(oc::toBlock(123));//means a random feed in "123".
				for (auto i = 0; i < 100; ++i) {
					senderSet[i] = prng0.get<block>();
				}
				PRNG prng1(oc::toBlock(314));//means a random feed in "123".
				for (auto i = 100; i < senderSize_To; ++i) {
					senderSet[i] = prng1.get<block>();
				}
				BitVector choices(width);
				prng1.get(choices.data(), choices.sizeBytes());	
				////////////Syn the Time--------------/////////////
				// u8* transHashInputsSyn[2];//receiver trans.
				// for (auto i = 0; i < 2; ++i) {
				// 	transHashInputsSyn[i] = new u8[hashLengthInBytes];
				// 	memset(transHashInputsSyn[i], 0, hashLengthInBytes);
				// }			
				u8* SynTime = new u8[hashLengthInBytes];		
				memset( SynTime, 0, hashLengthInBytes);
				ch.asyncSend(SynTime, hashLengthInBytes);//yipiyipi send to receiver.
				ch.recv(SynTime, hashLengthInBytes);
				///////////Syn the TIME ///////////////////////////
				Timer timer;
				char *savePath_c = "/tmp/output_C1.txt";
				if(remove(savePath_c)==0){
				cout<<"The path is delete successfully\n";
				}
				timer.setTimePoint("Sender start");
				
				auto heightInBytes = (height + 7) / 8;
				auto widthInBytes = (width + 7) / 8;
				auto locationInBytes = (logHeight + 7) / 8;//0001|1110|0101|1010|...
				// if(locationInBytes==4){
				// 	locationInBytes=3;
				// }
				auto senderSizeInBytes = (senderSize_To + 7) / 8;
				auto shift = (1 << logHeight) - 1;
				auto widthBucket1 = sizeof(block) / locationInBytes;//which is familier with this as 0001|1100|0101|1000|...|A Batchnorm
				int widthdivide=width/widthBucket1+1;

				//////////////////// Base OTs /////////////////////////////////
				
				IknpOtExtReceiver otExtReceiver;
				otExtReceiver.genBaseOts(prng1, ch);
				// BitVector choices(width);
				std::vector<block> otMessages(width);
				// prng.get(choices.data(), choices.sizeBytes());
				otExtReceiver.receive(choices, otMessages, prng1, ch);
				
				// cout<<"SEND choices[0] is "<<int(choices[0])<<" \n";
				// cout<<"SEND choices[1] is "<<int(choices[1])<<" \n";
				// std::cout << "Sender base OT finished\n";
				timer.setTimePoint("Sender base OT finished");


				////////////// Initialization //////////////////////
				
				PRNG commonPrng(commonSeed1);
				block commonKey;
				AES commonAes;
				
				u8* transLocations[widthBucket1];
				for (auto i = 0; i < widthBucket1; ++i) {
					transLocations[i] = new u8[senderSize_To * locationInBytes + sizeof(u32)];//everysender have locationInBytes
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
				
				block* sendSet = new block[senderSize_To];
				block* aesInput = new block[senderSize_To];
				block* aesOutput = new block[senderSize_To];
				
				RandomOracle H1(h1LengthInBytes);
				u8 h1Output[h1LengthInBytes];
				
				for (auto i = 0; i < senderSize_To; ++i) {
					H1.Reset();
					H1.Update((u8*)(senderSet.data() + i), sizeof(block));
					H1.Final(h1Output);
					
					aesInput[i] = *(block*)h1Output;
					sendSet[i] = *(block*)(h1Output + sizeof(block));
				}
				//Aes(h1())is OVER.
				commonAes.ecbEncBlocks(aesInput, senderSize_To, aesOutput);
				for (auto i = 0; i < senderSize_To; ++i) {
					sendSet[i] ^= aesOutput[i];
				}
				
				// std::cout << "Sender set transformed\n";
				timer.setTimePoint("Sender set transformed");


				// // 写入Matrix C into文件 output_C.txt.
				// int len = sizeof(matrixC)*150;
				// int len1 = sizeof(matrixC);
				// int len2 = sizeof(matrixC[0]);
				int len2 = heightInBytes;
				int len1 = len2*widthBucket1;
				int len = len1*widthdivide;
				// 打开文件
				// int fd=open("/home/qiqiang6/Desktop/EX_PSIOT_Ex/OPRF-PSI/PSI/Output/output_D.txt", O_RDWR|O_CREAT, 00777);
				int fd_c=open("/tmp/output_C1.txt", O_RDWR|O_CREAT, 00777);
				// int fd=open("/home/qiqiang6/Desktop/EX_PSIOT_Ex/OPRF-PSI/PSI/Output/output.txt", O_RDWR, 0644);
				// u8* addr;
				u8* addr_c;
				// lseek将文件指针往后移动file_size-1位
				// lseek(fd,len-1,SEEK_END);
				lseek(fd_c,len-1,SEEK_END);   
				// 从指针处写入一个空字符；mmap不能扩展文件长度，这里相当于预先给文件长度，准备一个空架子
				// write(fd, "", 1);
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
					
					for (auto low = 0; low < senderSize_To; low += bucket1) {
					
						auto up = low + bucket1 < senderSize_To ? low + bucket1 : senderSize_To;
					
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
					// std::cout <<t<< " Sender is 0\n";
					//Relate yuansu to copy into another way
					for(int k=1;k<widthBucket1+1;k++){
						memcpy(addr_c+(k-1)*len2+(t-1)*len1, matrixC[k-1], len2);
					}

					///////////////// Compute hash inputs (transposed) /////////////////////
					
					for (auto i = 0; i < w; ++i) {
						for (auto j = 0; j < senderSize_To; ++j) {
							auto location = (*(u32*)(transLocations[i] + j * locationInBytes)) & shift;
							//it is really excellent transHashInputs ments the senderSize*w outputs.
							transHashInputs[i + wLeft][j >> 3] |= (u8)((bool)(matrixC[i][location >> 3] & (1 << (location & 7)))) << (j & 7);
						}		
					}
					
				}

				// 内存映射建立好了，此时可以关闭文件了
				// close(fd);
				close(fd_c);
				// // 把data复制到addr里
				// // memcpy(addr, data, len);
				// 解除映射
				// munmap(addr, len);
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
				
				for (auto low = 0; low < senderSize_To; low += bucket2) {
					auto up = low + bucket2 < senderSize_To ? low + bucket2 : senderSize_To;
					
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
		}
		///////////Syn the TIME ///////////////////////////
		u8* SynTime = new u8[hashLengthInBytes];
		ch.recv(SynTime, hashLengthInBytes);
		ch.asyncSend(SynTime, hashLengthInBytes);//yipiyipi send to receive
		///////////Syn the TIME ///////////////////////////

	}

	void PsiReceiver::runNext(PRNG& prng, Channel& ch, block commonSeed, const u64& senderSize, const u64& receiverSize, const u64& height, const u64& logHeight, const u64& width, std::vector<block>& receiverSet, const u64& hashLengthInBytes, const u64& h1LengthInBytes, const u64& bucket1, const u64& bucket2, const u64& senderSize1, const u64& receiverSize1, const u64& height1, const u64& logHeight1, const u64& width1, const u64& hashLengthInBytes1) {
	
		for(int w=0;w<3;w++){
			if(w==0){
				///////////Syn the TIME ///////////////////////////
				u8* SynTime = new u8[hashLengthInBytes];
				ch.recv(SynTime, hashLengthInBytes);
				ch.asyncSend(SynTime, hashLengthInBytes);//yipiyipi send to receive
				///////////Syn the TIME ///////////////////////////
				Timer timer;
				timer.setTimePoint("Receiver start");
				TimeUnit start, end;
				// char *savePath = "/tmp/output_D.txt";
				// char *savePath_a = "/tmp/output_A.txt";
				// // char *savePath_Sender = "/home/qiqiang6/Desktop/EX_PSIOT_Ex/OPRF-PSI/PSI/Output/output_S.txt";
				// char *savePath_Receiver = "/tmp/output_R.txt";
				// if(remove(savePath)==0 && remove(savePath_a)==0 && remove(savePath_Receiver)==0){
				// cout<<"The path is delete successfully\n";
				// }
				cout<<"\n\n\n---------------------------------------The prepare process---------------------------------------\n";
				cout<<"---------------------------------------The prepare process---------------------------------------\n";
				cout<<"---------------------------------------The prepare process---------------------------------------\n";
				cout<<"---------------------------------------The prepare process---------------------------------------\n";
				cout<<"---------------------------------------The prepare process---------------------------------------\n\n\n";
				auto heightInBytes = (height + 7) / 8;
				auto SelfheightInBytes = (height + 7) / 16;
				auto widthInBytes = (width + 7) / 8;
				auto locationInBytes = (logHeight + 7) / 8;
				// if(locationInBytes==4){
				// 	locationInBytes=3;
				// }
				// auto receiverSizeInBytes = (receiverSize + 7) / 8;
				// auto receiverSizeInBytes = (receiverSize + 7) / 8;
				auto shift = (1 << logHeight) - 1;
				auto widthBucket1 = sizeof(block) / locationInBytes;
				int widthdivide=width/widthBucket1+1;

				u64 sentData = ch.getTotalDataSent();
				u64 recvData = ch.getTotalDataRecv();
				u64 totalData = sentData + recvData;
				
				timer.setTimePoint("Receiver initialized");
	
				/////////////////// Compute hash outputs ///////////////////////////

				RandomOracle H(hashLengthInBytes);
				u8 hashOutput[sizeof(block)];
				std::unordered_map<u64, std::vector<std::pair<block, u32>>> allHashes;
				// u8* hashInputs[bucket2];
				// for (auto i = 0; i < bucket2; ++i) {
				// 	hashInputs[i] = new u8[widthInBytes];
				// }
				// // 写入Matrix D into文件 output_D.txt.
				int lenr = receiverSize*hashLengthInBytes;
				int lenhashBatch=hashLengthInBytes*bucket2;
				// int len2 = sizeof(matrixDelta[0]);
				// 打开文件
				int fd_r=open("/tmp/output_R.txt", O_RDWR|O_CREAT, 00777);
				// int fd=open("/home/qiqiang6/Desktop/EX_PSIOT_Ex/OPRF-PSI/PSI/Output/output.txt", O_RDWR, 0644);
				u8* addr_r;
				lseek(fd_r,lenr-1,SEEK_END); 
				// 从指针处写入一个空字符；mmap不能扩展文件长度，这里相当于预先给文件长度，准备一个空架子
				write(fd_r, "", 1);
				// 使用mmap函数建立内存映射
				addr_r = (u8*)mmap(NULL, lenr, PROT_READ|PROT_WRITE,MAP_SHARED, fd_r, 0);

				// int s1=0;
				u8* recvBuff = new u8[bucket2 * hashLengthInBytes];//output0001|1111|1010|... to 01010101010101010
				for (auto low = 0; low < senderSize1; low += bucket2) {
					auto up = low + bucket2 < senderSize1 ? low + bucket2 : senderSize1;
					// s1++;
					ch.recv(recvBuff, (up - low) * hashLengthInBytes);
					for (auto j = low; j < up; ++j) {
						memcpy(hashOutput, recvBuff+(j-low)*hashLengthInBytes, hashLengthInBytes);
						// memcpy(hashOutput, addr_r+(j-low)*hashLengthInBytes+(s1-1)*lenhashBatch, hashLengthInBytes);
						allHashes[*(u64*)hashOutput].push_back(std::make_pair(*(block*)hashOutput, j));
					}
				}


				
				// std::cout << "Receiver hash outputs computed\n";
				timer.setTimePoint("Receiver hash outputs computed");

				///////////////// Receive hash outputs from sender and compute PSI ///////////////////


				
				auto psi = 0;
				int s1=0;
				for (auto low = 0; low < receiverSize; low += bucket2) {
					auto up = low + bucket2 < receiverSize ? low + bucket2 : receiverSize;
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
				// if (psi == 100) {
				// 	std::cout << "Receiver intersection computed - correct!\n";
				// }
				// else{
				std::cout << "Receiver intersection computed"<< psi <<"- correct!\n";
				// }
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

			}else if(w==1){
			////////////Syn the Time--------------/////////////
			u8* SynTime = new u8[hashLengthInBytes];		
			memset( SynTime, 0, hashLengthInBytes);
			ch.asyncSend(SynTime, hashLengthInBytes);//yipiyipi send to receiver.
			ch.recv(SynTime, hashLengthInBytes);
			///////////Syn the TIME ///////////////////////////
			Timer timer;
			// char *savePath_c = "/tmp/output_C.txt";
			// if(remove(savePath_c)==0){
			// cout<<"The path is delete successfully\n";
			// }
			timer.setTimePoint("Sender start");
			const block commonSeed1 = oc::toBlock(234567);
			u64 senderSize1_To;
			u64 receiverSize1_To;
			senderSize1_To=receiverSize1;
			receiverSize1_To=senderSize1;
			vector<block> senderSet(senderSize1_To); 
			//-------------delete----------delete----------delete------------
			PRNG prng0(oc::toBlock(123));//means a random feed in "123".
			for (auto i = 0; i < 150; ++i) {
				senderSet[i] = prng0.get<block>();
			}
			PRNG prng1(oc::toBlock(413));//means a random feed in "123".
			for (auto i = 0; i < 100; ++i) {
				senderSet[i] = prng1.get<block>();
			}	
			for (auto i = 150; i < senderSize1_To; ++i) {
				senderSet[i] = prng1.get<block>();
			}			

			auto heightInBytes = (height + 7) / 8;
			auto widthInBytes = (width + 7) / 8;
			auto locationInBytes = (logHeight + 7) / 8;//0001|1110|0101|1010|...
			// if(locationInBytes==4){
			// 	locationInBytes=3;
			// }
			auto senderSizeInBytes = (senderSize1_To + 7) / 8;
			auto shift = (1 << logHeight) - 1;
			auto widthBucket1 = sizeof(block) / locationInBytes;//which is familier with this as 0001|1100|0101|1000|...|A Batchnorm
			int widthdivide=width/widthBucket1+1;

			// //------It is different from last time databases.-----------
			// PRNG prng2(oc::toBlock(314));//means a random feed in "123".
			// for (auto i = 0; i < 100; ++i) {
			// 	senderSet[i] = prng2.get<block>();
			// }
			// PRNG prng1(oc::toBlock(561));//means a random feed in "123".
			// for (auto i = 100; i < senderSize1_To; ++i) {
			// 	senderSet[i] = prng1.get<block>();
			// }

			// //////////////////// Base OTs /////////////////////////////////
			
			// IknpOtExtReceiver otExtReceiver;
			// otExtReceiver.genBaseOts(prng, ch);
			// // BitVector choices(width);
			// std::vector<block> otMessages(width);
			// // prng.get(choices.data(), choices.sizeBytes());
			// otExtReceiver.receive(choices, otMessages, prng, ch);
			
			// // cout<<"SEND choices[0] is "<<int(choices[0])<<" \n";
			// // cout<<"SEND choices[1] is "<<int(choices[1])<<" \n";
			// // std::cout << "Sender base OT finished\n";
			// timer.setTimePoint("Sender base OT finished");


			////////////// Initialization //////////////////////
			
			PRNG commonPrng(commonSeed1);
			block commonKey;
			AES commonAes;
			
			u8* transLocations[widthBucket1];
			for (auto i = 0; i < widthBucket1; ++i) {
				transLocations[i] = new u8[senderSize1_To * locationInBytes + sizeof(u32)];//everysender have locationInBytes
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
			
			block* sendSet = new block[senderSize1_To];
			block* aesInput = new block[senderSize1_To];
			block* aesOutput = new block[senderSize1_To];
			
			RandomOracle H1(h1LengthInBytes);
			u8 h1Output[h1LengthInBytes];
			
			for (auto i = 0; i < senderSize1_To; ++i) {
				H1.Reset();
				H1.Update((u8*)(senderSet.data() + i), sizeof(block));
				H1.Final(h1Output);
				
				aesInput[i] = *(block*)h1Output;
				sendSet[i] = *(block*)(h1Output + sizeof(block));
			}
			//Aes(h1())is OVER.
			commonAes.ecbEncBlocks(aesInput, senderSize1_To, aesOutput);
			for (auto i = 0; i < senderSize1_To; ++i) {
				sendSet[i] ^= aesOutput[i];
			}
			
			// std::cout << "Sender set transformed\n";
			timer.setTimePoint("Sender set transformed");


			// // 写入Matrix C into文件 output_C.txt.
			// int len = sizeof(matrixC)*150;
			// int len1 = sizeof(matrixC);
			// int len2 = sizeof(matrixC[0]);
			int len2 = heightInBytes;
			int len1 = len2*widthBucket1;
			int len = len1*widthdivide;
			// 打开文件
			// int fd=open("/home/qiqiang6/Desktop/EX_PSIOT_Ex/OPRF-PSI/PSI/Output/output_D.txt", O_RDWR|O_CREAT, 00777);
			int fd_c=open("/tmp/output_C1.txt", O_RDWR|O_CREAT, 00777);
			// int fd=open("/home/qiqiang6/Desktop/EX_PSIOT_Ex/OPRF-PSI/PSI/Output/output.txt", O_RDWR, 0644);
			// u8* addr;
			u8* addr_c;
			// lseek将文件指针往后移动file_size-1位
			// // lseek(fd,len-1,SEEK_END);
			// lseek(fd_c,len-1,SEEK_END);   
			// // 从指针处写入一个空字符；mmap不能扩展文件长度，这里相当于预先给文件长度，准备一个空架子
			// // write(fd, "", 1);
			// write(fd_c, "", 1);
			// 使用mmap函数建立内存映射
			// addr = (u8*)mmap(NULL, len, PROT_READ|PROT_WRITE,MAP_SHARED, fd, 0);
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
				
				for (auto low = 0; low < senderSize1_To; low += bucket1) {
				
					auto up = low + bucket1 < senderSize1_To ? low + bucket1 : senderSize1_To;
				
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
					for (auto j = 0; j < senderSize1_To; ++j) {
						auto location = (*(u32*)(transLocations[i] + j * locationInBytes)) & shift;
						//it is really excellent transHashInputs ments the senderSize*w outputs.
						transHashInputs[i + wLeft][j >> 3] |= (u8)((bool)(matrixC[i][location >> 3] & (1 << (location & 7)))) << (j & 7);
					}		
				}
				
			}

			// 内存映射建立好了，此时可以关闭文件了
			// close(fd);
			close(fd_c);
			// // 把data复制到addr里
			// // memcpy(addr, data, len);
			// 解除映射
			// munmap(addr, len);
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
			
			for (auto low = 0; low < senderSize1_To; low += bucket2) {
				auto up = low + bucket2 < senderSize1_To ? low + bucket2 : senderSize1_To;
				
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
				const block commonSeed1 = oc::toBlock(345678);
				u64 senderSize1_To;
				u64 receiverSize1_To;
				// u64 senderSize_To;
				senderSize1_To=receiverSize1;
				receiverSize1_To=senderSize1;
				vector<block> senderSet(senderSize1_To); 
				//-------------delete----------delete----------delete------------
				// PRNG prng0(oc::toBlock(123));//means a random feed in "123".
				PRNG prng0(oc::toBlock(561));//means a random feed in "123".
				PRNG prng1(oc::toBlock(413));//means a random feed in "123".
				PRNG prng2(oc::toBlock(314));//means a random feed in "123".
				for (auto i = 0; i < senderSize1_To; ++i) {
					senderSet[i] = prng2.get<block>();
				}
				// for (auto i = 0; i < 150; ++i) {
				// 	senderSet[i] = prng0.get<block>();
				// }
				// for (auto i = 0; i < 100; ++i) {
				// 	senderSet[i] = prng1.get<block>();
				// }	
				// for (auto i = 150; i < senderSize1_To; ++i) {
				// 	senderSet[i] = prng1.get<block>();
				// }	
				////////////Syn the Time--------------/////////////
				// u8* transHashInputsSyn[2];//receiver trans.
				// for (auto i = 0; i < 2; ++i) {
				// 	transHashInputsSyn[i] = new u8[hashLengthInBytes];
				// 	memset(transHashInputsSyn[i], 0, hashLengthInBytes);
				// }			
				u8* SynTime = new u8[hashLengthInBytes];		
				memset( SynTime, 0, hashLengthInBytes);
				ch.asyncSend(SynTime, hashLengthInBytes);//yipiyipi send to receiver.
				ch.recv(SynTime, hashLengthInBytes);
				///////////Syn the TIME ///////////////////////////
				Timer timer;
				timer.setTimePoint("Sender start");
				
				auto heightInBytes = (height1 + 7) / 8;
				auto widthInBytes = (width1 + 7) / 8;
				auto locationInBytes = (logHeight1 + 7) / 8;//0001|1110|0101|1010|...
				// if(locationInBytes==4){
				// 	locationInBytes=3;
				// }
				auto senderSizeInBytes = (senderSize1_To + 7) / 8;
				auto shift = (1 << logHeight1) - 1;
				auto widthBucket1 = sizeof(block) / locationInBytes;//which is familier with this as 0001|1100|0101|1000|...|A Batchnorm
				int widthdivide=width1/widthBucket1+1;

				//////////////////// Base OTs /////////////////////////////////
				BitVector choices(width1);
				prng1.get(choices.data(), choices.sizeBytes());	
				IknpOtExtReceiver otExtReceiver;
				otExtReceiver.genBaseOts(prng1, ch);
				// BitVector choices(width);
				std::vector<block> otMessages(width1);
				// prng.get(choices.data(), choices.sizeBytes());
				otExtReceiver.receive(choices, otMessages, prng1, ch);
				
				// cout<<"SEND choices[0] is "<<int(choices[0])<<" \n";
				// cout<<"SEND choices[1] is "<<int(choices[1])<<" \n";
				// std::cout << "Sender base OT finished\n";
				timer.setTimePoint("Sender base OT finished");


				////////////// Initialization //////////////////////
				
				PRNG commonPrng(commonSeed1);
				block commonKey;
				AES commonAes;
				
				u8* transLocations[widthBucket1];
				for (auto i = 0; i < widthBucket1; ++i) {
					transLocations[i] = new u8[senderSize1_To * locationInBytes + sizeof(u32)];//everysender have locationInBytes
				}
				
				block randomLocations[bucket1];//ok

				u8* matrixC[widthBucket1];//also yipiyipi kuangdu.
				for (auto i = 0; i < widthBucket1; ++i) {
					matrixC[i] = new u8[heightInBytes];
				}

				//transHashInputs[sender][width]=0|1|0|0|1|1|0|...
				u8* transHashInputs[width1];
				for (auto i = 0; i < width1; ++i) {
					transHashInputs[i] = new u8[senderSizeInBytes];
					memset(transHashInputs[i], 0, senderSizeInBytes);
				}

				// std::cout << "Sender initialed\n";
				timer.setTimePoint("Sender initialed");

				/////////// Transform input /////////////////////

				commonPrng.get((u8*)&commonKey, sizeof(block));
				commonAes.setKey(commonKey);
				
				block* sendSet = new block[senderSize1_To];
				block* aesInput = new block[senderSize1_To];
				block* aesOutput = new block[senderSize1_To];
				
				RandomOracle H1(h1LengthInBytes);
				u8 h1Output[h1LengthInBytes];
				
				for (auto i = 0; i < senderSize1_To; ++i) {
					H1.Reset();
					H1.Update((u8*)(senderSet.data() + i), sizeof(block));
					H1.Final(h1Output);
					
					aesInput[i] = *(block*)h1Output;
					sendSet[i] = *(block*)(h1Output + sizeof(block));
				}
				//Aes(h1())is OVER.
				commonAes.ecbEncBlocks(aesInput, senderSize1_To, aesOutput);
				for (auto i = 0; i < senderSize1_To; ++i) {
					sendSet[i] ^= aesOutput[i];
				}
				
				// // std::cout << "Sender set transformed\n";
				// std::cout<<"S senderSet[0][0] is "<<int(senderSet[0][0])<<"\n";
				// std::cout<<"S senderSet[1][1] is "<<int(senderSet[1][1])<<"\n";
				// std::cout<<"S senderSet[2][2] is "<<int(senderSet[2][2])<<"\n";
				// std::cout<<"S h1Output[0] is "<<int(h1Output[0])<<"\n";
				// std::cout<<"S h1Output[1] is "<<int(h1Output[1])<<"\n";
				// std::cout<<"S h1Output[2] is "<<int(h1Output[2])<<"\n";				
				// // std::cout<<"S sendSet[0][2] is "<<int(sendSet[0][2])<<"\n";
				// std::cout<<"S sendSet[0][2] is "<<int(sendSet[0][2])<<"\n";
				// std::cout<<"S sendSet[1][2] is "<<int(sendSet[1][2])<<"\n";
				// std::cout<<"S sendSet[2][2] is "<<int(sendSet[2][2])<<"\n";
				timer.setTimePoint("Sender set transformed");


				// // // 写入Matrix C into文件 output_C.txt.
				// // int len = sizeof(matrixC)*150;
				// // int len1 = sizeof(matrixC);
				// // int len2 = sizeof(matrixC[0]);
				// int len2 = heightInBytes;
				// int len1 = len2*widthBucket1;
				// int len = len1*widthdivide;
				// // 打开文件
				// // int fd=open("/home/qiqiang6/Desktop/EX_PSIOT_Ex/OPRF-PSI/PSI/Output/output_D.txt", O_RDWR|O_CREAT, 00777);
				// int fd_c=open("/tmp/output_C1.txt", O_RDWR|O_CREAT, 00777);
				// // int fd=open("/home/qiqiang6/Desktop/EX_PSIOT_Ex/OPRF-PSI/PSI/Output/output.txt", O_RDWR, 0644);
				// // u8* addr;
				// u8* addr_c;
				// // lseek将文件指针往后移动file_size-1位
				// // lseek(fd,len-1,SEEK_END);
				// lseek(fd_c,len-1,SEEK_END);   
				// // 从指针处写入一个空字符；mmap不能扩展文件长度，这里相当于预先给文件长度，准备一个空架子
				// // write(fd, "", 1);
				// write(fd_c, "", 1);
				// // 使用mmap函数建立内存映射
				// // addr = (u8*)mmap(NULL, len, PROT_READ|PROT_WRITE,MAP_SHARED, fd, 0);
				// addr_c = (u8*)mmap(NULL, len, PROT_READ|PROT_WRITE,MAP_SHARED, fd_c, 0);
				int t=0;
				for (auto wLeft = 0; wLeft < width1; wLeft += widthBucket1) {
					auto wRight = wLeft + widthBucket1 < width1 ? wLeft + widthBucket1 : width1;
					auto w = wRight - wLeft;
					t++;

					//////////// Compute random locations (transposed) ////////////////

					commonPrng.get((u8*)&commonKey, sizeof(block));
					commonAes.setKey(commonKey);
					
					for (auto low = 0; low < senderSize1_To; low += bucket1) {
					
						auto up = low + bucket1 < senderSize1_To ? low + bucket1 : senderSize1_To;
					
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
					// std::cout <<t<< " Sender is 0\n";
					//Relate yuansu to copy into another way
					// for(int k=1;k<widthBucket1+1;k++){
					// 	memcpy(addr_c+(k-1)*len2+(t-1)*len1, matrixC[k-1], len2);
					// }

					///////////////// Compute hash inputs (transposed) /////////////////////
					
					for (auto i = 0; i < w; ++i) {
						for (auto j = 0; j < senderSize1_To; ++j) {
							auto location = (*(u32*)(transLocations[i] + j * locationInBytes)) & shift;
							//it is really excellent transHashInputs ments the senderSize*w outputs.
							transHashInputs[i + wLeft][j >> 3] |= (u8)((bool)(matrixC[i][location >> 3] & (1 << (location & 7)))) << (j & 7);
						}		
					}
					
				}
				// std::cout<<"S transHashInputs[0][2] is "<<int(transHashInputs[0][2])<<"\n";
				// std::cout<<"S transHashInputs[1][2] is "<<int(transHashInputs[1][2])<<"\n";
				// std::cout<<"S transHashInputs[2][2] is "<<int(transHashInputs[2][2])<<"\n";				

				// // 内存映射建立好了，此时可以关闭文件了
				// // close(fd);
				// close(fd_c);
				// // // 把data复制到addr里
				// // // memcpy(addr, data, len);
				// // 解除映射
				// // munmap(addr, len);
				// munmap(addr_c, len);
				// // std::cout << "Sender transposed hash input computed\n";
				timer.setTimePoint("Sender transposed hash input computed");

				/////////////////// Compute hash outputs ///////////////////////////
				
				RandomOracle H(hashLengthInBytes1);
				u8 hashOutput[sizeof(block)];

				u8* hashInputs[bucket2];
				
				for (auto i = 0; i < bucket2; ++i) {
					hashInputs[i] = new u8[widthInBytes];
				}
				
				for (auto low = 0; low < senderSize1_To; low += bucket2) {
					auto up = low + bucket2 < senderSize1_To ? low + bucket2 : senderSize1_To;
					
					for (auto j = low; j < up; ++j) {
						memset(hashInputs[j - low], 0, widthInBytes);
					}
					
					for (auto i = 0; i < width1; ++i) {
						for (auto j = low; j < up; ++j) {
							hashInputs[j - low][i >> 3] |= (u8)((bool)(transHashInputs[i][j >> 3] & (1 << (j & 7)))) << (i & 7);//means * 2^(i & 7),that is OK!
						}
					}
					
					u8* sentBuff = new u8[(up - low) * hashLengthInBytes1];
					
					for (auto j = low; j < up; ++j) {
						H.Reset();
						H.Update(hashInputs[j - low], widthInBytes);
						H.Final(hashOutput);
						
						memcpy(sentBuff + (j - low) * hashLengthInBytes1, hashOutput, hashLengthInBytes1);
					}
					
					ch.asyncSend(sentBuff, (up - low) * hashLengthInBytes1);//yipiyipi send to receiver.
				}
				
				// std::cout << "Sender hash outputs computed and sent\n";
				timer.setTimePoint("Sender hash outputs computed and sent");

				std::cout << timer;
			}
		}
		///////////Syn the TIME ///////////////////////////
		u8* SynTime = new u8[hashLengthInBytes];
		ch.recv(SynTime, hashLengthInBytes);
		ch.asyncSend(SynTime, hashLengthInBytes);//yipiyipi send to receive
		///////////Syn the TIME ///////////////////////////

	}


}