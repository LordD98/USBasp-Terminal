
#include <stdint.h>
#include <math.h>
#include <stdio.h>


uint8_t uart_write_int(uint16_t);
void uart_putc(char c);

int main()
{
	for (uint16_t i = 0; i < 65999; i++)
	{
		uart_write_int(i);
	}
	getchar();
	return 0;
}

uint8_t uart_write_int(uint16_t i)
{
	uint8_t num_of_chars = (uint8_t)ceil(log10((double)i));
	uint8_t digit;
	uint8_t prevNum = 0;
	double d;
	for (int j = num_of_chars - 1; j >= 0; j--)
	{
		/*d = fmod(floor((double)i / pow(10.0, (double)j)), 10.0);
		digit = (uint8_t)d;*/
		digit = (uint8_t)fmod(floor((double)i / pow(10.0, (double)j)), 10.0);
		//printf("%u", digit);
		uart_putc(48 + digit);
		//digit = (int)floor(((double)i/pow(10.0, (double)j))) - 10*prevNum;		//ie. 987 where j=2, prev = 0: digit = 
	}
	uart_putc('\0');
	uart_putc('\n');
	return 1;
}

void uart_putc(char c)
{
	printf("%c", c);
}