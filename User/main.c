#include "stm32f10x.h"
#include <stdio.h>
#include "Serial.h"
#include "delay.h"

#define NODE_POWER    1
#define NODE_BODY     2
#define NODE_CLUSTER  3
#define NODE_ROLE     1

void CAN_Node_Init(void)
{
    GPIO_InitTypeDef gpio;
    CAN_InitTypeDef can;
    CAN_FilterInitTypeDef filter;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_CAN1, ENABLE);

    gpio.GPIO_Pin = GPIO_Pin_11;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);

    gpio.GPIO_Pin = GPIO_Pin_12;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &gpio);

    CAN_DeInit(CAN1);
    CAN_StructInit(&can);
    can.CAN_TTCM = DISABLE;
    can.CAN_ABOM = ENABLE;
    can.CAN_AWUM = ENABLE;
    can.CAN_NART = DISABLE;
    can.CAN_RFLM = DISABLE;
    can.CAN_TXFP = DISABLE;
    can.CAN_Mode = CAN_Mode_Normal;
    can.CAN_SJW = CAN_SJW_1tq;
    can.CAN_BS1 = CAN_BS1_5tq;
    can.CAN_BS2 = CAN_BS2_2tq;
    can.CAN_Prescaler = 9;
    CAN_Init(CAN1, &can);

    filter.CAN_FilterNumber = 0;
    filter.CAN_FilterMode = CAN_FilterMode_IdMask;
    filter.CAN_FilterScale = CAN_FilterScale_32bit;
    filter.CAN_FilterIdHigh = 0x0000;
    filter.CAN_FilterIdLow = 0x0000;
    filter.CAN_FilterMaskIdHigh = 0x0000;
    filter.CAN_FilterMaskIdLow = 0x0000;
    filter.CAN_FilterFIFOAssignment = CAN_Filter_FIFO0;
    filter.CAN_FilterActivation = ENABLE;

#if NODE_ROLE == NODE_CLUSTER
    filter.CAN_FilterMode = CAN_FilterMode_IdList;
    filter.CAN_FilterIdHigh = 0x100 << 5;
    filter.CAN_FilterIdLow = 0x0000;
    filter.CAN_FilterMaskIdHigh = 0x200 << 5;
    filter.CAN_FilterMaskIdLow = 0x0000;
#endif

    CAN_FilterInit(&filter);
}

void EngineData_Pack(uint8_t *d, uint16_t rpm, uint8_t kmh, uint8_t thr_pct, int8_t temp_c)
{
    uint16_t r = rpm * 4;
    uint16_t v = (uint16_t)kmh * 100;
    d[0] = (uint8_t)(r & 0xFF);
    d[1] = (uint8_t)(r >> 8);
    d[2] = (uint8_t)(v & 0xFF);
    d[3] = (uint8_t)(v >> 8);
    d[4] = thr_pct * 2;
    d[5] = (uint8_t)(temp_c + 40);
    d[6] = 0;
    d[7] = 0;
}

void EngineData_Show(const uint8_t *d)
{
    uint16_t r = (uint16_t)d[0] | ((uint16_t)d[1] << 8);
    uint16_t v = (uint16_t)d[2] | ((uint16_t)d[3] << 8);
    printf("  ENGINE: RPM=%d.%02d Speed=%d.%02d Thr=%d.%d Temp=%dC\r\n",
           r / 4, (r % 4) * 25, v / 100, v % 100,
           d[4] / 2, (d[4] % 2) * 5, (int)d[5] - 40);
}

void BodyData_Show(const uint8_t *d)
{
    printf("  BODY: Door=%s Light=%s Wiper=%s\r\n",
           d[0] ? "OPEN" : "CLOSED",
           d[1] ? "ON" : "OFF",
           d[2] ? "ON" : "OFF");
}

int main(void)
{
    CanTxMsg tx;
    CanRxMsg rx;
    uint8_t i;

    Serial_Init();
    CAN_Node_Init();

    tx.ExtId = 0;
    tx.IDE = CAN_ID_STD;
    tx.RTR = CAN_RTR_DATA;
    tx.DLC = 8;

#if NODE_ROLE == NODE_POWER
    uint16_t rpm = 1000;
    printf("POWER ECU start\r\n");
    tx.StdId = 0x100;
    while (1)
    {
        EngineData_Pack(tx.Data, rpm, 48, 10, -19);
        CAN_Transmit(CAN1, &tx);
        rpm += 10;
        if (rpm > 3000) rpm = 1000;
        Delay_ms(500);
    }

#elif NODE_ROLE == NODE_BODY
    uint8_t tick = 0;
    printf("BODY ECU start\r\n");
    tx.StdId = 0x200;
    while (1)
    {
        tx.Data[0] = (tick / 5) % 2;
        tx.Data[1] = (tick / 3) % 2;
        tx.Data[2] = tick % 2;
        tx.Data[3] = 0; tx.Data[4] = 0; tx.Data[5] = 0; tx.Data[6] = 0; tx.Data[7] = 0;
        CAN_Transmit(CAN1, &tx);
        printf("BODY sent 0x200 tick=%d\r\n", tick);

        tick++;
        if (tick % 5 == 0)
        {
            tx.StdId = 0x400;
            tx.Data[0] = 0xAA;
            CAN_Transmit(CAN1, &tx);
            printf("BODY sent 0x400 (cluster should NOT see it)\r\n");
            tx.StdId = 0x200;
        }
        Delay_ms(1000);
    }

#else
    printf("CLUSTER ECU start (filter: 0x100, 0x200 only)\r\n");
    while (1)
    {
        if (CAN_MessagePending(CAN1, CAN_FIFO0) > 0)
        {
            CAN_Receive(CAN1, CAN_FIFO0, &rx);
            printf("RX ID=0x%03X DLC=%d Data=", rx.StdId, rx.DLC);
            for (i = 0; i < rx.DLC; i++) printf("%02X ", rx.Data[i]);
            printf("\r\n");
            if (rx.StdId == 0x100) EngineData_Show(rx.Data);
            if (rx.StdId == 0x200) BodyData_Show(rx.Data);
        }
    }
#endif
}
