// Visual Studio Console Test.cpp : Definiert den Einstiegspunkt für die Konsolenanwendung.
//

#include "stdafx.h"
#include <iostream>

using namespace std;

int main()
{
	char buf[1024];
	while (1)
	{
		cin >> buf;
		if (!buf[0] == '\0')
		{
			cout << buf << endl;
		}
	}
    return 0;
}

