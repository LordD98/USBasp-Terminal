#include "usbasp_uart.h"

#include <chrono>
#include <fstream>
#include <thread>
#include <vector>
#include <string>
#include <iostream>
#include <limits>

bool verbose = false;
bool sendNewline = false;
bool single_byte_mode;
bool single_char;
bool closeConnectionOnError = true;

#define USB_ERROR_NOTFOUND 1

using namespace std;

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
	if (single_byte_mode)
	{
		while (1)
		{
			uint8_t buff[300];
			int rv = usbasp_uart_read(usbasp, buff, sizeof(buff));
			if (rv < 0) {
				std::cerr << "read: rv=" << rv << std::endl;
				return;
			}
			else if (rv != 0)
			{
				for (int i = 0; i < rv; i++)
				{
					std::cout << (int)buff[i];
					if (buff[i] == '\0')
						std::cout << ": '\\0'" << std::endl;
					else if (buff[i] == '\n')
						std::cout << ": '\\n'" << std::endl;
					else if (buff[i] < 32 || buff[i] > 126)
						std::cout << std::endl;
					else
						std::cout << ": '" << buff[i] << "'" << std::endl;
				}
				//std::cout << std::endl;
			}
		}
	} 
	else
	{
		while(1)
		{
			uint8_t buff[300];
			int rv=usbasp_uart_read(usbasp, buff, sizeof(buff));
			if(rv<0)
			{
				std::cerr << "read: rv=" << rv << std::endl;
				if(closeConnectionOnError)
					return;
			}
			else if (rv != 0)
			{
				for (int i = 0; i < rv; i++)
				{
					if (buff[i] != '\0')
						std::cout << buff[i];
					//else
						//std::cout << std::endl;
				}
				//std::cout << std::endl;
			}
		}
	}
}

void write_forever(USBasp_UART* usbasp){
	std::string line;
	char buff[1024];
	if (single_byte_mode)
	{
		while (1)
		{
			if (single_char)
			{
				char c;
				std::cin >> c;
				std::cin.getline(buff, 1024);
				usbasp_uart_write_all(usbasp, reinterpret_cast<uint8_t*>(&c), 1);
			}
			else
			{
				int i;
				std::cin >> i;
				std::cin.getline(buff, 1024);
				usbasp_uart_write_all(usbasp, reinterpret_cast<uint8_t*>(&i), 1);
			}
		}
	} 
	else
	{
		while(1)
		{
			std::cin.getline(buff, 1024);
			int rv = strlen(buff) + 1;		//count chars, including '\0'
	
			if(rv==0){ return; }
			else if(rv<0)
			{
				std::cerr << "write: read from stdin returned %d\n" << rv;
				if (closeConnectionOnError)
					return;
			}
			else
			{
				if (sendNewline)
				{
					buff[rv] = '\r';			//add CR
					buff[rv + 1] = '\n';		//and NL to end of string
					rv += 2;
				}
				usbasp_uart_write_all(usbasp, reinterpret_cast<uint8_t*>(&buff[0]), rv);
			}
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
}

int main(int argc, char** argv)
{
	int baud = 9600;
	int parity = USBASP_UART_PARITY_NONE;
	int bits = USBASP_UART_BYTES_8B;
	int stop = USBASP_UART_STOP_1BIT;

	volatile bool should_test_read = false;
	volatile bool should_test_write = false;
	bool should_read = false;
	bool should_write = false;
	bool skipOptions = false;
	single_byte_mode = false;
	single_char = false;
	int test_size = (10 * 1024);

	if (argc == 1)
	{
		//usage(argv[0]);
	}
	else
	{
		/*
		Select Baud:
		------------------
		0		1		2		3		4
		2400	4800	9600	14400	115200
		input: x

		Select Parity:
		------------------
		e		o		n
		Even	Odd		None
		input: x
		------------------
		to end press ctrl-c
		*/

		for (int i = 0; i < argc; i++)
		{
			if (strcmp(argv[i], "-skipOptions") == 0)
			{
				skipOptions = true;
			}
			if (strcmp(argv[i], "-rw") == 0)
			{
				should_write = true;
				should_read = true;
				i++;
				continue;
			}
			else if (strcmp(argv[i], "-b") == 0)
			{
				char *buf = argv[i + 1];
				baud = std::stoi(buf);
				i += 2;
				continue;
			}
			else if (strcmp(argv[i], "-nl") == 0)
			{
				sendNewline = true;
				i++;
			}
			else if (strcmp(argv[i], "-sbm") == 0)
			{
				single_byte_mode = true;
				i++;
				continue;
			}
			else if (strcmp(argv[i], "-sc") == 0)
			{
				single_char = true;
				i++;
				continue;
			}
			else if (strcmp(argv[i], "-p") == 0)
			{
				//sscanf(argv[i+1], "%d", &parity);
				switch (parity)
				{
				case 0:	parity = USBASP_UART_PARITY_NONE;	break;
				case 1:	parity = USBASP_UART_PARITY_EVEN;	break;
				case 2:	parity = USBASP_UART_PARITY_ODD;	break;
				default: std::cerr << "Bad parity, falling back to default." << std::endl;
				}
				i += 2;
			}
			else if (strcmp(argv[i], "-B") == 0)
			{
				//sscanf(argv[i + 1], "%d", &bits);
				switch (bits)
				{
				case 5:	bits = USBASP_UART_BYTES_5B;	break;
				case 6:	bits = USBASP_UART_BYTES_6B;	break;
				case 7:	bits = USBASP_UART_BYTES_7B;	break;
				case 8:	bits = USBASP_UART_BYTES_8B;	break;
				case 9:	bits = USBASP_UART_BYTES_9B;	break;
				default: std::cerr << "Bad byte size, falling back to default." << std::endl;
				}
				i += 2;
			}
			else if (strcmp(argv[i], "-s") == 0)
			{
				stop = stoi(argv[i + 1]);
				switch (stop)
				{
				case 1: stop = USBASP_UART_STOP_1BIT;	break;
				case 2: stop = USBASP_UART_STOP_2BIT;	break;
				default: std::cerr << "Bad stop bit count, falling back to default." << std::endl;
				}
			}
			else if (strcmp(argv[i], "") == 0)
			{
				i++;
			}
			else
			{
				if (strstr(argv[i], "r") != NULL)
				{
					should_read = true;
				}
				if (strstr(argv[i], "w") != NULL)
				{
					should_write = true;
				}
				if (strstr(argv[i], "R") != NULL)
				{
					should_test_read = true;
				}
				if (strstr(argv[i], "W") != NULL)
				{
					should_test_write = true;
				}
				if (strstr(argv[i], "v") != NULL)
				{
					verbose = true;
				}
			}
		}
	}

	auto stob = [](string s) -> bool {
		if (s == "0" || s == "false")
		{
			return false;
		}
		else
		{
			return true;
		}
	};

	fstream settings;
	settings.open("settings.txt", ios::in | ios::out);
	string line;
	int num;

	if (settings.is_open())
	{
		getline(settings, line);	//Baud
		baud = stoi(line);
		getline(settings, line);	//Parity
		parity = stoi(line);
		getline(settings, line);	//Stop bits
		stop = stoi(line);
		getline(settings, line);	//Data bits
		bits = stoi(line);
		getline(settings, line);	//Single byte
		single_byte_mode = stob(line);
		getline(settings, line);	//Read cont.
		should_read = stob(line);
		getline(settings, line);	//Write cont.
		should_write = stob(line);
		getline(settings, line);	//Send '\n'
		sendNewline = stob(line);
		getline(settings, line);	//Single char
		single_char = stob(line);
		getline(settings, line);	//Verbose
		verbose = stob(line);
		getline(settings, line);	//Test read
		should_test_read = stob(line);
		getline(settings, line);	//Test write
		should_test_write = stob(line);
		getline(settings, line);	//Close connection when an error occurs
		closeConnectionOnError = stob(line);
		settings.close();
	}

	if (!skipOptions)
	{
		//dialog to specify more options:
		char option;
		char c;
	OPTIONS_START:
		option = 0;
		cout << "Do you want to have extended options? Y/N/ENTER(to skip completely) ";
		option = cin.get();
		if (option == '\n')		//no options, use default settings
			cout << endl;
		else					//show options
		{
			cout << endl;
			cin.get();
			if (option == 'n' || option == 'N')
			{
				//string input = "";

				//normal options:
				//Baud rate selection:
				cout << "Select Baud:" << endl
					<< "--------------------------------------" << endl
					<< "0\t1\t2\t3\t4" << endl
					<< "2400\t4800\t9600\t14400\t115200" << endl
					<< "input: ";
				cin >> baud;
				switch (baud)
				{
				case 0:
					baud = 2400;
					break;
				case 1:
					baud = 4800;
					break;
				case 2:
					baud = 9600;
					break;
				case 3:
					baud = 14400;
					break;
				case 4:
					baud = 115200;
					break;
				default:
					//baud = input
					break;
				}
				cout << endl;

				//Parity mode selection:
				cout << "Select Parity:" << endl
					<< "--------------------------------------" << endl
					<< "e\to\tn" << endl
					<< "Even\tOdd\tNone" << endl
					<< "input: ";
				cin >> c;
				switch (c)
				{
				case 'e':
					parity = USBASP_UART_PARITY_EVEN;
					break;
				case 'o':
					parity = USBASP_UART_PARITY_ODD;
					break;
				case 'n':
					parity = USBASP_UART_PARITY_NONE;
					break;
				default:
					break;
				}

				cout << endl;
				cout << "--------------------------------------" << endl;
				cout << endl;

				cin.get();
			}
			else if (option == 'y' || option == 'Y')
			{
				//extended options:
				int input;
				//bool to string
				auto btos = [](bool b)
				{
					return b ? "true" : "false";
				};
				//parity to string
				auto paritytos = [](int parity) -> string
				{
					switch (parity)
					{
					case USBASP_UART_PARITY_NONE:
						return "None";
						break;
					case USBASP_UART_PARITY_EVEN:
						return "Even";
						break;
					case USBASP_UART_PARITY_ODD:
						return "Odd";
						break;
					default:
						return "";
						break;
					}
				};

			EXTENDED_MENU_START:
				input = 0;
				//print all options:
				cout << "All options:" << endl
					<< "Pick a listed number, Bools and Ints with only two options get toggled directly!" << endl
					<< endl
					<< "Number\t| Type |\t|\t    Description \t    |\t   | Current value |" << endl
					<< "(0):\t(Action)\tExit this menu and start the terminal" << endl
					<< "(1):\t(Int)\t\tBaud rate" << "\t\t\t\t\t(" << baud << ")" << endl
					<< "(2):\t(Int)\t\tParity" << "\t\t\t\t\t\t(" << paritytos(parity) << ")" << endl
					<< "(3):\t(Int)\t\tNumber of stop bits" << "\t\t\t\t(" << stop / 4 + 1 << ")" << endl
					<< "(4):\t(Int)\t\tNumber of data bits" << "\t\t\t\t(" << 5 + ((USBASP_UART_BYTES_MASK&bits) >> 3) << ")" << endl
					<< "(5):\t(Bool)\t\tSingle byte mode" << "\t\t\t\t(" << btos(single_byte_mode) << ")" << endl
					<< "(6):\t(Bool)\t\tRead continuously" << "\t\t\t\t(" << btos(should_read) << ")" << endl
					<< "(7):\t(Bool)\t\tWrite continuously" << "\t\t\t\t(" << btos(should_write) << ")" << endl
					<< "(8):\t(Bool)\t\tSend new line" << "\t\t\t\t\t(" << btos(sendNewline) << ")" << endl
					<< "(9):\t(Bool)\t\tSingle char output" << "\t\t\t\t(" << btos(single_char) << ")" << endl
					<< "(10):\t(Bool)\t\tVerbose logging" << "\t\t\t\t\t(" << btos(verbose) << ")" << endl
					<< "(11):\t(Bool)\t\tTest read" << "\t\t\t\t\t(" << btos(should_test_read) << ")" << endl
					<< "(12):\t(Bool)\t\tTest write" << "\t\t\t\t\t(" << btos(should_test_write) << ")" << endl
					<< "(13):\t(Bool)\t\tClose connection on error" << "\t\t\t(" << btos(closeConnectionOnError) << ")" << endl
					<< endl;
					cin >> input;
					cout << endl;
					switch (input)
					{
					case 0:
						goto MENU_EXIT;
						break;
					case 1:
						cout << "Baud rate: ";
						cin >> baud;
						goto EXTENDED_MENU_START;
						break;
					case 2:
						goto EXTENDED_MENU_START;
						break;
					case 3:
						//cout << "Set number of stop bits (1/2): ";
						stop = (stop == USBASP_UART_STOP_1BIT ? USBASP_UART_STOP_2BIT : USBASP_UART_STOP_1BIT);
						goto EXTENDED_MENU_START;
						break;
					case 4:
						cout << "Set number of data bits (5-9): ";
						cin >> input;
						switch (input)
						{
						case 5:
							bits = USBASP_UART_BYTES_5B;
							break;
						case 6:
							bits = USBASP_UART_BYTES_6B;
							break;
						case 7:
							bits = USBASP_UART_BYTES_7B;
							break;
						case 8:
							bits = USBASP_UART_BYTES_8B;
							break;
						case 9:
							bits = USBASP_UART_BYTES_9B;
							break;
						default:
							cout << "Invalid input, leaving everything as is" << endl;
							break;
						}
						goto EXTENDED_MENU_START;
						break;
					case 5:
						single_byte_mode = !single_byte_mode;
						goto EXTENDED_MENU_START;
						break;
					case 6:
						should_read = !should_read;
						goto EXTENDED_MENU_START;
						break;
					case 7:
						should_write = !should_write;
						goto EXTENDED_MENU_START;
						break;
					case 8:
						sendNewline = !sendNewline;
						goto EXTENDED_MENU_START;
						break;
					case 9:
						single_char = !single_char;
						goto EXTENDED_MENU_START;
						break;
					case 10:
						verbose = !verbose;
						goto EXTENDED_MENU_START;
						break;
					case 11:
						should_test_read = !should_test_read;
						goto EXTENDED_MENU_START;
						break;
					case 12:
						should_test_write = !should_test_write;
						goto EXTENDED_MENU_START;
						break;
					case 13:
						closeConnectionOnError = !closeConnectionOnError;
						goto EXTENDED_MENU_START;
						break;
					default:
						cout << "Invalid input!" << endl;
						goto EXTENDED_MENU_START;
						break;
					}
				MENU_EXIT:
					cin.get();
			}
			else
				goto OPTIONS_START;
		}
	}
	
	//save settings:
	settings.open("settings.txt", ios::out | ios::trunc);

	if (settings.is_open())
	{
		settings.clear();
		settings << baud << '\n';	//Baud
		settings << parity << '\n';	//Parity
		settings << stop << '\n';	//Stop bits
		settings << bits << '\n';	//Data bits
		settings << single_byte_mode << '\n';	//Single byte
		settings << should_read << '\n';	//Read cont.
		settings << should_write << '\n';	//Write cont.
		settings << sendNewline << '\n';	//Send '\n'
		settings << single_char << '\n';	//Single char
		settings << verbose << '\n';	//Verbose
		settings << should_test_read << '\n';	//Test read
		settings << should_test_write << '\n';	//Test write
		settings << closeConnectionOnError << '\n';	//Test write
		settings.close();
	}

	USBasp_UART usbasp;
	usbasp.usbhandle = NULL;
	int rv;
	if ((rv = usbasp_uart_config(&usbasp, baud, parity | bits | stop)) < 0)
	{
		std::cerr << "Error " << rv << " while initializing USBasp" << std::endl;
		switch (rv)
		{
		case USBASP_NO_CAPS:
			std::cerr << "USBasp has no UART capabilities." << std::endl;
			break;
		case -USB_ERROR_NOTFOUND:
			std::cerr << "Cannot find USBasp device." << std::endl;
			break;
		default:
			break;
		}
		std::cout << "Press enter to exit" << std::endl;
		std::cin.get();
		return -1;
	}
	if (should_test_write) {
		std::cerr << "Writing..." << std::endl;
		writeTest(&usbasp, test_size);
	}
	if (should_test_read) {
		std::cerr << "Reading..." << std::endl;
		readTest(&usbasp, test_size);
	}

	std::vector<std::thread> threads;
	if (should_read) {
		threads.push_back(std::thread([&] {read_forever(&usbasp); }));
	}
	if (should_write) {
		threads.push_back(std::thread([&] {write_forever(&usbasp); }));
	}
	for (auto& thr : threads) {
		thr.join();
	}
	cin.get();
}