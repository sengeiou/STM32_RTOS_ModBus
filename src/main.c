#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "main.h"
#include "timer.h"


#define F_CPU 8000000UL

#define adrr_devise 					81 // 2				// ����� ����� ���������� � ���� ������
#define leth_array						50					// ������ ������� ��� ��������/����������� ������. ������ ��� �������� ����������� ������ �� ������



// ���������
typedef struct
{
	unsigned int 	count_modbas_in;						// ������� ���-�� �������� ����
	unsigned int 	array_modbas_in[leth_array];			// ����� ��� ����� �� �����
} modbus_in_str;

modbus_in_str Modbus_in_str;

typedef struct
{
	unsigned int 	count_modbas_out;						// ������� ���-�� ���������� ����
	unsigned int 	array_modbas_out[leth_array];			// ����� ��� �������� �� �����
}modbus_out_str;

modbus_out_str Modbus_out_str;



// ������� ���������
QueueHandle_t vModBus_slave_queue_in;
QueueHandle_t vModBus_slave_queue_out;




//���������� ����������
unsigned int 	delay_packet 			= 0;				// �������� ����� ��������. ����� ����� 7, ��� ������ 1,75 ��
unsigned int	flag_UART_in 			= 0;				// ���� ������ ����� ������� �� UART

unsigned int	buffer_index = 0;							// ������� ��� ��������



// ����������� ����������

void TIM2_IRQHandler (void)									// ���������� ������� ��� ������ ��������
{
	unsigned short status;
	status = TIM2->SR;
	TIM2->SR = ~status;

	if(status & TIM_SR_UIF)
	{
		if(delay_packet != 0)								// ������ ������� ��� ������
		{
			delay_packet--;
		}

		 if (flag_UART_in == 1) 							// �������� ����� ������ �����. ������ ����� ������ ������ ����.
		 	{
		 		if (delay_packet == 0)						// �������� �� ������� �����. ���� ����� �� ����� ��� �����
		 		{
		 			flag_UART_in = 0;						// ���������� ���� ������ ������, ��������� � ������� ���������


		 			xQueueSendFromISR(vModBus_slave_queue_in,&(Modbus_in_str),0);

		 			USART1->CR1  &=  ~USART_CR1_RXNEIE;				    // ���������� �� ���������� �����. ��������.

		 		}
		 	}
	}
}	// ������ TIM2_IRQHandler





void USART1_IRQHandler(void)
{
	unsigned short temp =  USART1->SR;
	USART1->SR = 0;

	 if (temp & USART_SR_RXNE)								// RXNE - ���-�� ������, ����� ��������. ������������ ��� ������ DR
		{

			 Modbus_in_str.array_modbas_in[(Modbus_in_str.count_modbas_in & 0x0ff)] = USART1->DR;	// ���������� ���� � �������� ������ ��� �����.
			 Modbus_in_str.count_modbas_in++;				// ����������� ������� ���� � ������


			 delay_packet = 7;								// ��������� ������ 1,75 ��, ������ ����� ���� ����� ����������� ��� ����������. ����������� ����� - ������� �������.
			 flag_UART_in = 1;								// ���� ������ ����� �������

			 return;
		}





	 if(temp & USART_SR_TC)
	 {
		 USART1->CR1 	&= 	~(USART_CR1_TCIE); 				// ��������� ���������� �� ���������� ��������. ����� ������ �������� �������

		 //GPIOC->BSRR =  GPIO_BSRR_BR12;					// �������� 0 � GPIOA.12. �������, ���� ��������� ����������� ������������ ����
		 return;
	 }





	 if (temp & USART_SR_TXE)
		 {

		 	 xQueueReceiveFromISR(vModBus_slave_queue_out,&(Modbus_out_str),0);

			 buffer_index++;												// ����������� ������


			 USART1->DR = Modbus_out_str.array_modbas_out[buffer_index];   	// ����� ������ �� �������.

			 if(buffer_index == (Modbus_out_str.count_modbas_out-1))  		// ������ ���� ������?
				{
					USART1->CR1 	&= 	~(USART_CR1_TXEIE); 				// ��������� ���������� �� ����������� - �������� ���������
					USART1->CR1 	|= 	USART_CR1_TCIE; 					// �������� ���������� �� ���������� ��������

					buffer_index = 0;										// ���������� �������� ���������� �������

					USART1->CR1  |=	USART_CR1_RXNEIE;				    	// ���������� �� ���������� ����� ��������
					Modbus_in_str.count_modbas_in = 0;						// �������� ������� ����������
				}
		 }
} // ������ USART1_IRQHandler





//��� ��������� ������ ��� �������� �����
#define	ERROR_ACTION(CODE,POS)		do{}while(0)



//TODO blinker
// ������ ��������. ������ ���. ������ ��������
void vBlinker (void *pvParameters)
{


	//���������������� GPIOC.13	������� ������ �� �����
	GPIOC->CRH &= ~GPIO_CRH_MODE13;   			// �������� ������� MODE
	GPIOC->CRH &= ~GPIO_CRH_CNF13;    			// �������� ������� CNF
	GPIOC->CRH |=  GPIO_CRH_MODE13;   			// �����, 50MHz
	GPIOC->CRH &= ~GPIO_CRH_CNF13;    			// ������ ����������, ������������



	while(1)
	{

		GPIOC->BSRR = GPIO_BSRR_BR13;

		vTaskDelay(250);

		GPIOC->BSRR = GPIO_BSRR_BS13;

		vTaskDelay(250);
	}// ����������� ���� ������ vBlinker
} // �������� ����� blinker









//TODO slave
void vModBus_slave (void *pvParameters)
{

	modbus_in_str	Receive_Modbus_in_str;			// ��������� ���������� �������� �� ���������� �����
	modbus_out_str	Sender_Modbus_out_str;


	unsigned short	array_mb[leth_array]	= {};	// ������ ��� �������� ��������/���������� ����

	unsigned long 	crc_calc 				= 0;	// ��������� ����������� �����
	unsigned long 	adrr_var 				= 0;	// �������� ����� �� ������� ��������� �� ����. ��� ���������.
	unsigned long 	quantity_byte 			= 0;	// ���-�� ���� ������� ���������� �������� ��� ������� �� ���� �� ������

	unsigned int 	crc_read_low 			= 0;	// �������� ���������� ��� ��������� ����������� �����
	unsigned int 	crc_read_high 			= 0;	//
	unsigned int 	crc_calc_low 			= 0;	//
	unsigned int 	crc_calc_high 			= 0;	//





	// �������� � �������� �������. (72 000 000/19200)/16 = 234.375 �������� ������� ����� 0���, � ������� 0�6. ����� BRR = 0xEA6.
	// (72 000 000 / 115 200) / 16 = 39,0625
	// 39 = 27h
	// 0.0625 * 16 = 1 (����� ��������� �� ������ � �������. �� � ���� ������ �����)
	// 27h � 1h = 271h

	/*
	 * 115200 - 0x0271
	 * 76800 - 0x03AA
	 * 57600 - 0x04E2
	 * 38400 - 0x0753
	 * 28800 - 0x09C4
	 * 19200 - 0x0EA6
	 * 14400 - 0x1388
	 * 9600 - 0x1D4C
	 * 4800 - 0x3A98
	 * 2400 - 0x7530
	 */

	//�������� �����������
	RCC->APB2ENR |=   RCC_APB2ENR_AFIOEN;                // ������������ �������������� ������� GPIO
	RCC->APB2ENR |=   RCC_APB2ENR_USART1EN;              // ������������ USART1

	//���������������� PORTA.9 ��� TX
	GPIOA->CRH   &= ~(GPIO_CRH_MODE9 | GPIO_CRH_CNF9);   // ����������� MODE � CNF
	GPIOA->CRH   |=   GPIO_CRH_MODE9 | GPIO_CRH_CNF9_1;  // ����������� ����� � ���������� �-��, 50MHz

	//���������������� PORTA.10 ��� RX
	GPIOA->CRH   &= ~(GPIO_CRH_MODE10 | GPIO_CRH_CNF10);   // ����������� MODE � CNF
	GPIOA->CRH   |=   GPIO_CRH_CNF10_0;                   // ����, ������ ���������


	// ������� ������ ������
	USART1->BRR   =   0x0271;                           // 115200
	USART1->CR1  &=  ~USART_CR1_M;                      // 8 ��� ������
	USART1->CR2  &=  ~USART_CR2_STOP;                   // ���-�� ����-���: 1

	// ���������� �������
	USART1->CR1  |=   USART_CR1_TE;                     // ��������� �����������
	USART1->CR1  |=   USART_CR1_RE;                     // ��������� ��������

	USART1->CR1  |=   USART_CR1_UE;                     // ��������� ������ USART4

	// ��������� ����������
	NVIC_EnableIRQ (USART1_IRQn);
	USART1->CR1  |=	USART_CR1_RXNEIE;				    // ���������� �� ���������� �����
	//USART1->CR1  |= 	USART_CR1_TCIE;                 // ���������� �� ���������� ��������





	while(1)
	{

		if(array_mb[0] == 1)
		{
			GPIOC->BSRR = GPIO_BSRR_BR13;
		}
		if(array_mb[0] == 2)
		{
			GPIOC->BSRR = GPIO_BSRR_BS13;
		}


		array_mb[8]++;


		if(xQueueReceiveFromISR(vModBus_slave_queue_in,&(Receive_Modbus_in_str),0))				// ���, ���� ���������� �� ������ �������.//
		{

			if (Receive_Modbus_in_str.array_modbas_in[0] == adrr_devise) 							// �������� ������ ����������
			{																// ���� ����� ������, �� ������� ��� � ������ ������
				crc_calc = CRC16(Receive_Modbus_in_str.array_modbas_in, (Receive_Modbus_in_str.count_modbas_in-2));		// ������� CRC ��������� ������

				crc_read_high = Receive_Modbus_in_str.array_modbas_in[Receive_Modbus_in_str.count_modbas_in-2]; 		// �������
				crc_read_low = Receive_Modbus_in_str.array_modbas_in[Receive_Modbus_in_str.count_modbas_in-1];			// �������

				crc_calc_low = ((crc_calc >> 8) & 0x00FF);					// ������� (count_modbas_in+2)
				crc_calc_high = (crc_calc & 0x00FF);						// ������� (count_modbas_in+1)


				if((crc_read_low == crc_calc_low)&&(crc_read_high == crc_calc_high))// �������� ������������ ����������� �����.
				{


					switch (Receive_Modbus_in_str.array_modbas_in[1]) 	// ���� ���������� ����� ������� ������� � ������.
						{

							case 0x06:				// ������ �������� � ���� ������� �������� (Preset Single Register).
							{
								adrr_var = Receive_Modbus_in_str.array_modbas_in[2];								// �������� ����� �� ������� � ���� ����������
								adrr_var = ((adrr_var << 8) | Receive_Modbus_in_str.array_modbas_in[3]);

								if (adrr_var <= leth_array)															// �������� ����������� ������ �� ���������� ������
								{

									array_mb[adrr_var] = Receive_Modbus_in_str.array_modbas_in[4];
									array_mb[adrr_var] = ((array_mb[adrr_var] << 8) | Receive_Modbus_in_str.array_modbas_in[5]);



									for(int i = 0; i<= Receive_Modbus_in_str.count_modbas_in; i++)					// ��������� ������. ��� ��� ����� ������ ���� ����� �� ��� ������� �����.
									{
										Sender_Modbus_out_str.array_modbas_out[i] = Receive_Modbus_in_str.array_modbas_in[i];
									}

									Sender_Modbus_out_str.count_modbas_out = Receive_Modbus_in_str.count_modbas_in;
									Receive_Modbus_in_str.count_modbas_in = 0;


									// �������� ������ �������
									xQueueSend(vModBus_slave_queue_out,&Sender_Modbus_out_str,4);		// ��� � ���������� ���������� ���������

									//GPIOC->BSRR =  GPIO_BSRR_BS12;

									USART1->DR = Sender_Modbus_out_str.array_modbas_out[0];		// ���������� ������ ���� �� ������� ��� ��������
									USART1->CR1 	|= 	USART_CR1_TXEIE;		// �������� ���������� �� ����������� ����


								} // ������ �������� ������ ����������
								else 														// ���� ����� ������� �� �������� ���������, ����� �������� ��� ������
								{
									Receive_Modbus_in_str.count_modbas_in = 0;
									USART1->CR1  |=	USART_CR1_RXNEIE;				    // ���������� �� ���������� �����
								} // else ������

							} // ������ ������� 0�06
							break;



							case 0x10:				// ������ �������� � ��������� ��������� �������� (Preset Multiple Registers)
							{
								adrr_var = Receive_Modbus_in_str.array_modbas_in[2];								// �������� ����� �� ������� � ���� ����������
								adrr_var = ((adrr_var << 8) | Receive_Modbus_in_str.array_modbas_in[3]);


									if (adrr_var + (Receive_Modbus_in_str.array_modbas_in[6]/2) <= leth_array)	// �������� ����������� ������ �� ���������� ������
									{

										int adrr_var_buf = adrr_var;

										for (int i = 0; Receive_Modbus_in_str.array_modbas_in[6] > i ; i++)
										{
											array_mb[adrr_var_buf] = Receive_Modbus_in_str.array_modbas_in[(7+i)];	//
											adrr_var_buf++;
										} // ������ �����


										Sender_Modbus_out_str.array_modbas_out[0] = Receive_Modbus_in_str.array_modbas_in[0];
										Sender_Modbus_out_str.array_modbas_out[1] = Receive_Modbus_in_str.array_modbas_in[1];
										Sender_Modbus_out_str.array_modbas_out[2] = Receive_Modbus_in_str.array_modbas_in[2];
										Sender_Modbus_out_str.array_modbas_out[3] = Receive_Modbus_in_str.array_modbas_in[3];
										Sender_Modbus_out_str.array_modbas_out[4] = Receive_Modbus_in_str.array_modbas_in[4];
										Sender_Modbus_out_str.array_modbas_out[5] = Receive_Modbus_in_str.array_modbas_in[5];


										crc_calc = CRC16(Sender_Modbus_out_str.array_modbas_out, 6);				// ������� CRC ������������� ������, 3 ����� ��������� ����� � ����� ��������� , ������� �� ���-�� � ����������

										crc_calc_low = ((crc_calc >> 8) & 0x00FF);			// ������� (count_modbas_in+2)
										crc_calc_high = (crc_calc & 0x00FF);				// ������� (count_modbas_in+1)

										Sender_Modbus_out_str.array_modbas_out[6] = crc_calc_high;				// ���������� ����������, ����� � ������ ��� ��������
										Sender_Modbus_out_str.array_modbas_out[7] = crc_calc_low;


										Sender_Modbus_out_str.count_modbas_out = 8;								// ����� ������ ����� 8 ����
										Receive_Modbus_in_str.count_modbas_in = 0;


										// �������� ������ �������
										xQueueSend(vModBus_slave_queue_out,&Sender_Modbus_out_str,4);		// ��� � ���������� ���������� ���������

										//GPIOC->BSRR =  GPIO_BSRR_BS12;


										USART1->DR = Sender_Modbus_out_str.array_modbas_out[0];		// ���������� ������ ���� �� ������� ��� ��������
										USART1->CR1 	|= 	USART_CR1_TXEIE;		// �������� ���������� �� ����������� ����


									} // ������ �������� ������ ����������
									else
									{
										Receive_Modbus_in_str.count_modbas_in = 0;
										USART1->CR1  |=	USART_CR1_RXNEIE;				    // ���������� �� ���������� �����
									} // ������ �������� ������ ����������

							} // ������ ������� 0�10
							break;



							case 0x03:				// ������ �������� �� ���������� ��������� �������� (Read Holding Registers).
							{

								adrr_var = Receive_Modbus_in_str.array_modbas_in[2];										// �������� ����� �� ������� � ���� ����������
								adrr_var = ((adrr_var << 8) | Receive_Modbus_in_str.array_modbas_in[3]);

								quantity_byte = Receive_Modbus_in_str.array_modbas_in[4];
								quantity_byte = ((quantity_byte << 8) | Receive_Modbus_in_str.array_modbas_in[5]);		// ������ ���-�� ����, ������� ����� ���������


								if ((adrr_var <= leth_array) || ((adrr_var + quantity_byte) <= leth_array))	// �������� ����������� ������ ������ �� ���������� ������.
								{
									Sender_Modbus_out_str.array_modbas_out[0] = Receive_Modbus_in_str.array_modbas_in[0];						// ����� ������, ������� �������� �� ������
									Sender_Modbus_out_str.array_modbas_out[1] = Receive_Modbus_in_str.array_modbas_in[1];						// ��� �������, �� ������� �������� ����� (0�3 ��������� � ���� ������)
									Sender_Modbus_out_str.array_modbas_out[2] = quantity_byte * 2;						// ����������� ����, ������� �������. ���������� 2 �����, ���� ��������, ������� �� ����� ����� � ���� ����� �������� ����� 255-�� ����



									for (int i = 0; i <= quantity_byte; i++, adrr_var++)
									{
										Sender_Modbus_out_str.array_modbas_out[i*2+3] = ((array_mb[adrr_var] >> 8) & 0x00FF);			// ������ �� ������� �������� � ���� �� ���������� ������
										Sender_Modbus_out_str.array_modbas_out[i*2+4] = (array_mb[adrr_var] & 0x00FF);
									}

									quantity_byte *= 2;
																									// � ����� �� ��� �����
									crc_calc = CRC16(Sender_Modbus_out_str.array_modbas_out, (quantity_byte + 3));		// ������� CRC ������������� ������, 3 ����� ��������� ����� � ����� ��������� , ������� �� ����� � ����������

									crc_calc_low = ((crc_calc >> 8) & 0x00FF);						// ������� (count_modbas_in+2)
									crc_calc_high = (crc_calc & 0x00FF);							// ������� (count_modbas_in+1)

									Sender_Modbus_out_str.array_modbas_out[(quantity_byte + 3)] = crc_calc_high;			// ���������� ����������, ����� � ������ ��� ��������
									Sender_Modbus_out_str.array_modbas_out[(quantity_byte + 3)+1] = crc_calc_low;			//

									Sender_Modbus_out_str.count_modbas_out = (quantity_byte + 3)+2;
									Receive_Modbus_in_str.count_modbas_in = 0;


									// �������� ������ �������
									xQueueSend(vModBus_slave_queue_out,&Sender_Modbus_out_str,4);		// ��� � ���������� ���������� ���������

									//GPIOC->BSRR =  GPIO_BSRR_BS12;


									USART1->DR = Sender_Modbus_out_str.array_modbas_out[0];		// ���������� ������ ���� �� ������� ��� ��������
									USART1->CR1 	|= 	USART_CR1_TXEIE;		// �������� ���������� �� ����������� ����


								} // ������ �������� ������ ����������
								else // ���� ����� ������� �� �������� ���������, ����� �������� ��� ������
								{
									Receive_Modbus_in_str.count_modbas_in = 0;
									USART1->CR1  |=	USART_CR1_RXNEIE;				    // ���������� �� ���������� �����
								} // else ������

							} // ������ ������� 0�03
							break;



							case 0x04:				// ������ �������� �� ���������� ��������� ����� (Read Input Registers).
							{
								USART1->CR1  |=	USART_CR1_RXNEIE;				    // ���������� �� ���������� �����
							} // ������ ������� 0�04
							break;

						} // ������ ����� ������� ������ �� �������.


				} // ������ �������� ���������� ��������� crc

				else
				{
					// ���� ��������� �����, �� crc �� ������. ����� ������������ ���������� ��������� ��������
					Receive_Modbus_in_str.count_modbas_in = 0;
					USART1->CR1  |=	USART_CR1_RXNEIE;				    // ���������� �� ���������� �����
				} // else ������ �������� ���������� ��������� crc

			} // ������ �������� ������


			else
			{
				// ���� ��������� �����, �� ������� �� ����, ������� �� �������� � ��� ���������
				Receive_Modbus_in_str.count_modbas_in = 0;
				USART1->CR1  |=	USART_CR1_RXNEIE;				    // ���������� �� ���������� �����
			} // else ������ �������� ������

	}


	}// ����������� ���� ������ vModBus_slave

} // ��������� ������ vModBus_slave









//TODO main
int main(void)
{

	vModBus_slave_queue_in = xQueueCreate(1,sizeof(modbus_in_str));		// ������������� ������� �������� � ���� ������
	vModBus_slave_queue_out = xQueueCreate(1,sizeof(modbus_out_str));	// ������������� ������� ����� �� ����� �������

	SystemInit();
	init_timer2();										// ������������� ������� ���� ��������


	RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;   // ��������� ������������ GPIOA
	RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;   // ��������� ������������ GPIOB
	RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;   // ��������� ������������ GPIOC
	RCC->APB2ENR |= RCC_APB2ENR_IOPDEN;   // ��������� ������������ GPIOD
	RCC->APB2ENR |= RCC_APB2ENR_IOPEEN;	  // ��������� ������������ GPIOE




	if(pdTRUE != xTaskCreate(vBlinker,"Blinker", 	500, NULL, tskIDLE_PRIORITY + 1, NULL))
		{
			ERROR_ACTION(TASK_NOT_CREATE,0);
		}


	if(pdTRUE != xTaskCreate(vModBus_slave,"ModBus Slave", 	500, NULL, tskIDLE_PRIORITY + 1, NULL))
		{
			ERROR_ACTION(TASK_NOT_CREATE,0);
		}


	//NVIC_EnableIRQ (TIM2_IRQn);

	__enable_irq ();

	vTaskStartScheduler();						// ��������� ��������� � ���������.


}	// �������� �����







// ������� ��� ������� ����������� �����
unsigned short CRC16(int *puchMsg,  /* ���������       */
                            unsigned short usDataLen /* ����� ��������� */)
{
    unsigned short crc = 0xFFFF;
    unsigned short uIndex;
    int i;
    for (uIndex = 0; uIndex < usDataLen; uIndex += 1) {
        crc ^= (unsigned short)*(puchMsg + uIndex);
        for (i = 8; i != 0; i -= 1) {
          if ((crc & 0x0001) == 0) { crc >>= 1; }
          else { crc >>= 1; crc ^= 0xA001; }			// ������� ����� �����
        }
    }
    // ������� ������� ���������� ����
 //   crc = ((crc >> 8) & 0x00FF) | ((crc << 8) & 0xFF00);
    return crc;
}
