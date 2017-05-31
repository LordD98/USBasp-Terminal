#include "usbasp_uart.h"

#include <chrono>
#include <fstream>
#include <thread>
#include <vector>
#include <string>
#include <iostream>

int verbose=0;

void writeTest(USBasp_UART* usbasp, int size){
	std::string s;
	char c='a';
	for(int i=0; i<size; i++){
		s+=c;
		c++;
		if(c>'z'){ c='a'; }
	}
	auto start=std::chrono::high_resolution_clock::now();
	int rv;
	if((rv=usbasp_uart_write_all(usbasp, (uint8_t*)s.c_str(), s.size()))<0){
		std::cerr << "Error " << rv << " while writing..." << std::endl;
		return;
	}
	auto finish=std::chrono::high_resolution_clock::now();
	auto us=std::chrono::duration_cast<std::chrono::microseconds>(finish-start).count();
	std::cout << s.size() << " bytes sent in " << us / 1000 << "ms" << std::endl;
	std::cout << "Average speed: " << s.size() / 1000.0 / (us / 1000000.0) << "kB/s" << std::endl;
}

void readTest(USBasp_UART* usbasp, size_t size){
	auto start=std::chrono::high_resolution_clock::now();
	int us;
	std::string s;
	while(1){
		if(s.size()==0){
			start=std::chrono::high_resolution_clock::now();
		}
		if(s.size()>=size){
			auto finish=std::chrono::high_resolution_clock::now();
			us=std::chrono::duration_cast<std::chrono::microseconds>(finish-start).count();
			break;
		}
		uint8_t buff[300];
		int rv=usbasp_uart_read(usbasp, buff, sizeof(buff));
		if(rv==0)
			continue;	// Nothing is available for now.
		else if(rv<0){
			std::cerr << "Error while reading, rv=" << rv << std::endl;
			return;
		}
		else
		{
			s+=std::string((char*)buff, rv);
			std::cerr << s.size() << "/" << size << std::endl;
		}
	}
	std::cout << "Whole received text:" << std::endl;
	std::cout << s.c_str() << std::endl;
	std::cout << s.size() << " bytes received in " << us / 1000 << "ms" << std::endl;
	std::cout << "Average speed: " << s.size() / 1000.0 / (us / 1000000.0) << "kB/s" << std::endl;
}

void read_forever(USBasp_UART* usbasp){
	while(1){
		uint8_t buff[300];
		int rv=usbasp_uart_read(usbasp, buff, sizeof(buff));
		if(rv<0){
			std::cerr << "read: rv=" << rv << std::endl;
			return;
		}
		else if (rv != 0)
		{
			for (int i = 0; i < rv; i++) {
				std::cout << buff[i];
			}
			std::cout << std::endl;
		}
	}
}

void write_forever(USBasp_UART* usbasp){
	std::string line;
	char buff[1024];
	while(1){
		std::cin.getline(buff, 1024);
		int rv = strlen(buff) + 1;		//count chars, including '\0'
		if(rv==0){ return; }
		else if(rv<0){
			std::cerr << "write: read from stdin returned %d\n" << rv;
			return;
		}
		else{
			usbasp_uart_write_all(usbasp, reinterpret_cast<uint8_t*>(&buff[0]), rv);
		}
	}
}

void usage(const char* name)
{
	std::cerr << "Usage: " << name <<" [OPTIONS]" << std::endl;
	std::cerr << "Allows UART communication through modified USBasp." << std::endl;
	std::cerr << "Options:" << std::endl;
	std::cerr << "  -r        copy UART to stdout" << std::endl;
	std::cerr << "  -w        copy stdin to UART" << std::endl;
	std::cerr << "  -R        perform read test (read 10kB from UART and output average speed)" << std::endl;
	std::cerr << "  -W        perform write test (write 10kB to UART and output average speed)" << std::endl;
	std::cerr << "  -S SIZE   set different r/w test size (in bytes)" << std::endl;
	std::cerr << "  -b BAUD   set baud, default 9600" << std::endl;
	std::cerr << "  -p PARITY set parity (default 0=none, 1=even, 2=odd)" << std::endl;
	std::cerr << "  -B BITS   set byte size in bits, default 8" << std::endl;
	std::cerr << "  -s BITS   set stop bit count, default 1" << std::endl;
	std::cerr << "  -v        increase verbosity" << std::endl;
	std::cerr << std::endl;
	std::cerr << "If you want to use it as interactive terminal, use " << name << " -rw -b 9600" << std::endl;
	exit(0);
}

int main(int argc, char** argv){
	if(argc==1){
		usage(argv[0]);
	}
	int baud=9600;
	int parity=USBASP_UART_PARITY_NONE;
	int bits=USBASP_UART_BYTES_8B;
	int stop=USBASP_UART_STOP_1BIT;
	 
	bool should_test_read=false;
	bool should_test_write=false;
	bool should_read=false;
	bool should_write=false;
	int test_size=(10*1024);

	//opterr=0;
	
	int c;

	int i = 0;
	while (i < argc)
	{
		if (strcmp(argv[i], "-rw") == 0)
		{
			should_write = true;
			//should_read = true;
			i++;
			continue;
		}
		else if (strcmp(argv[i], "-b")==0)
		{
			char *buf = argv[i + 1];
			baud = std::stoi(buf);
			i += 2;
			continue;
		} 
		else if(strcmp(argv[i], "") == 0)
		{
		}
		else i++;
	}

	/*
	while( (c=getopt(argc, argv, "rwRWS:b:p:B:s:v"))!=-1){
		switch(c){
		case 'r':
			should_read=true;
			break;
		case 'w':
			should_write=true;
			break;
		case 'R':
			should_test_read=true;
			break;
		case 'W':
			should_test_write=true;
			break;
		case 'S':
			sscanf(optarg, "%d", &test_size);
			break;
		case 'b':
			sscanf(optarg, "%d", &baud);
			break;
		case 'p':
			sscanf(optarg, "%d", &parity);
			switch(parity){
			default: fprintf(stderr, "Bad parity, falling back to default.\n");
			case 0:	parity=USBASP_UART_PARITY_NONE;	break;
			case 1:	parity=USBASP_UART_PARITY_EVEN;	break;
			case 2:	parity=USBASP_UART_PARITY_ODD ;	break;
			}
			break;
		case 'B':
			sscanf(optarg, "%d", &bits);
			switch(bits){
			case 5:	bits=USBASP_UART_BYTES_5B;	break;
			case 6:	bits=USBASP_UART_BYTES_6B;	break;
			case 7:	bits=USBASP_UART_BYTES_7B;	break;
			default: fprintf(stderr, "Bad byte size, falling back to default.\n");
			case 8:	bits=USBASP_UART_BYTES_8B;	break;
			case 9:	bits=USBASP_UART_BYTES_9B;	break;
			}
			break;
		case 's':
			sscanf(optarg, "%d", &stop);
			switch(stop){
			default: fprintf(stderr, "Bad stop bit count, falling back to default.\n");
			case 1: stop=USBASP_UART_STOP_1BIT;	break;
			case 2: stop=USBASP_UART_STOP_2BIT;	break;
			}
			break;
		case 'v':
			verbose++;
			break;
		default:
			usage(argv[0]);
			break;
		}
	}
	*/

	USBasp_UART usbasp;
	int rv;
	if((rv=usbasp_uart_config(&usbasp, baud, parity | bits | stop)) < 0){
		std::cerr << "Error " << rv << " while initializing USBasp" << std::endl;
		if(rv==USBASP_NO_CAPS){
			std::cerr << "USBasp has no UART capabilities." << std::endl;
		}
		return -1;
	}
	if(should_test_write){
		std::cerr << "Writing..." << std::endl;
		writeTest(&usbasp, test_size);
	}
	if(should_test_read){
		std::cerr << "Reading..." << std::endl;
		readTest(&usbasp, test_size);
	}
	std::vector<std::thread> threads;
	if(should_read){
		threads.push_back(std::thread([&]{read_forever(&usbasp);}));
	}
	if(should_write){
		threads.push_back(std::thread([&]{write_forever(&usbasp);}));
	}
	for(auto& thr : threads){
		thr.join();
	}
}
