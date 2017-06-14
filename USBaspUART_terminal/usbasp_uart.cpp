
#include "usbasp_uart.h"
#include <iostream>
#include <cstdint>

#define USB_ERROR_NOTFOUND 1
#define USB_ERROR_ACCESS   2
#define USB_ERROR_IO       3

#define	USBASP_SHARED_VID  0x16C0
#define	USBASP_SHARED_PID  0x05DC

//#define dprintf(...) if(verbose>0){fprintf(stderr,__VA_ARGS__);}

static int usbasp_uart_open(USBasp_UART* usbasp);
static uint32_t usbasp_uart_capabilities(USBasp_UART* usbasp);
static int usbasp_uart_transmit(USBasp_UART* usbasp, uint8_t receive, 
		uint8_t functionid, const uint8_t* send, uint8_t* buffer, 
		uint16_t buffersize);

static uint8_t dummy[4];

int usbasp_uart_config(USBasp_UART* usbasp, int baud, int flags)
{
	if(int errorcode = usbasp_uart_open(usbasp) != 0)
	{
		return -errorcode;
	}
	uint32_t caps=usbasp_uart_capabilities(usbasp);
	printf("Capabilities: %x\n", caps);							//modified
	if(!(caps & USBASP_CAP_6_UART)){
		return USBASP_NO_CAPS;
	}
	uint8_t send[4];

	const int FOSC=F_CPU;
	int presc=FOSC/8/baud - 1;
	printf("Baud prescaler: %d\n", presc);							//modified
	int real_baud=FOSC/8/(presc+1);
	if(real_baud!=baud){
		fprintf(stderr, "Note: cannot select baud=%d, selected %d instead.\n", baud, real_baud);
	}
	send[1]=presc>>8;
	send[0]=presc&0xFF;
	send[2]=flags&0xFF;
	usbasp_uart_transmit(usbasp, 1, USBASP_FUNC_UART_CONFIG, send, dummy, 0);
	return 0;
}

void usbasp_uart_flushrx(USBasp_UART* usbasp){
	usbasp_uart_transmit(usbasp, 1, USBASP_FUNC_UART_FLUSHRX, dummy, dummy, 0);
}

void usbasp_uart_flushtx(USBasp_UART* usbasp){
	usbasp_uart_transmit(usbasp, 1, USBASP_FUNC_UART_FLUSHTX, dummy, dummy, 0);
}

void usbasp_uart_disable(USBasp_UART* usbasp){
	usbasp_uart_transmit(usbasp, 1, USBASP_FUNC_UART_DISABLE, dummy, dummy, 0);
	libusb_close(usbasp->usbhandle);
}

int usbasp_uart_read(USBasp_UART* usbasp, uint8_t* buff, size_t len){
	if(len>254){ len=254; } // Limitation of V-USB library.
	return usbasp_uart_transmit(usbasp, 1, USBASP_FUNC_UART_RX, dummy, buff, len);
}

int usbasp_uart_write(USBasp_UART* usbasp, uint8_t* buff, size_t len){
	uint8_t tmp[2];
	usbasp_uart_transmit(usbasp, 1,  USBASP_FUNC_UART_TX_FREE, dummy, tmp, 2);
	size_t avail=(tmp[0]<<8)|tmp[1];
	if(len>avail){
		len=avail;
	}
	std::cout << "Received free= " << avail << ", transmitting " << len << " bytes" << std::endl;
	if(len==0){
		return 0;
	}
	//std::cout << std::endl << (int)buff[0] << std::endl;
	return usbasp_uart_transmit(usbasp, 0, USBASP_FUNC_UART_TX, dummy, buff, len);
}

int usbasp_uart_write_all(USBasp_UART* usbasp, uint8_t* buff, int len){
	int i=0;
	while(i<len){
		int rv=usbasp_uart_write(usbasp, buff+i, len-i);
		if(rv<0)
		{
			std::cout << "write_all: rv=" << rv << std::endl;
			return rv; 
		}
		i+=rv;
		std::cout << "write_all: "<< i << "/" << len << " sent" << std::endl;
	}
	return len;
}

int usbasp_uart_open(USBasp_UART* usbasp){
	
	int errorCode = USB_ERROR_NOTFOUND;

	libusb_context* ctx;
	libusb_init(&ctx);

	libusb_device** dev_list;
	int dev_list_len = libusb_get_device_list(ctx, &dev_list);

	for (int j=0; j<dev_list_len; ++j) {
		libusb_device* dev = dev_list[j];
		struct libusb_device_descriptor descriptor;
		libusb_get_device_descriptor(dev, &descriptor);
		if (descriptor.idVendor == USBASP_SHARED_VID 
				&& descriptor.idProduct == USBASP_SHARED_PID) {
			uint8_t str[256];
			libusb_open(dev, &usbasp->usbhandle);
			if (!usbasp->usbhandle) {
				errorCode = USB_ERROR_ACCESS;
				continue;
			}
			libusb_get_string_descriptor_ascii(usbasp->usbhandle, 
					descriptor.iManufacturer & 0xff, str, sizeof(str));
			if(strcmp("www.fischl.de", (const char*)str)){
				libusb_close(usbasp->usbhandle);
				usbasp->usbhandle=NULL;
				continue;
			}
			printf("Vendor: %s\n", str);							//modified
			libusb_get_string_descriptor_ascii(usbasp->usbhandle, 
					descriptor.iProduct & 0xff, str, sizeof(str));
			if(strcmp("USBasp", (const char*)str)){
				libusb_close(usbasp->usbhandle);
				usbasp->usbhandle=NULL;
				continue;
			}
			printf("Product: %s\n", str);							//modified
			break;
		}
	}
	libusb_free_device_list(dev_list,1);
	if (usbasp->usbhandle != NULL){
		errorCode = 0;
	}
	return errorCode;
}

uint32_t usbasp_uart_capabilities(USBasp_UART* usbasp){
	uint8_t res[4];
	uint8_t tmp[4];
	uint32_t ret=0;
	if(usbasp_uart_transmit(usbasp, 1, USBASP_FUNC_GETCAPABILITIES, 
				tmp, res, sizeof(res)) == 4){
		 ret = res[0] | ((uint32_t)res[1] << 8) | ((uint32_t)res[2] << 16) |
			 ((uint32_t)res[3] << 24);
	}
	return ret;
}

int usbasp_uart_transmit(USBasp_UART* usbasp, uint8_t receive, 
		uint8_t functionid, const uint8_t* send, uint8_t* buffer, 
		uint16_t buffersize){
	return libusb_control_transfer(usbasp->usbhandle,
			(LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | (receive << 7)) & 0xff,
			functionid, 
			((send[1] << 8) | send[0]), 
			((send[3] << 8) | send[2]), 
			buffer, 
			buffersize,
			5000); // 5s timeout.
}
