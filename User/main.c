#include "stm32f10x.h"
#include <stdio.h>
#include <serial.h>
#include <Delay.h>

#define NODE_A  1     /* A板写1，B板写0 */

/* CAN 初始化：和 L1 一样，只把模式改成 Normal */
void CAN_Node_Init(void)
{
    GPIO_InitTypeDef        gpio;
    CAN_InitTypeDef         can;
    CAN_FilterInitTypeDef   filter;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_CAN1, ENABLE);

    gpio.GPIO_Pin   = GPIO_Pin_11;         /* PA11 = CAN_RX */
    gpio.GPIO_Mode  = GPIO_Mode_IPU;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);

    gpio.GPIO_Pin   = GPIO_Pin_12;         /* PA12 = CAN_TX */
    gpio.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &gpio);

    CAN_DeInit(CAN1);
    CAN_StructInit(&can);
    can.CAN_TTCM = DISABLE;
    can.CAN_ABOM = ENABLE;                 /* BUS-OFF 自动恢复，L4 要用 */
    can.CAN_AWUM = ENABLE;
    can.CAN_NART = DISABLE;                /* 反逻辑：DISABLE = 允许出错自动重发 */
    can.CAN_RFLM = DISABLE;
    can.CAN_TXFP = DISABLE;                /* 按 ID 优先级发 */
    can.CAN_Mode = CAN_Mode_Normal;        /* ← L1 是 LoopBack，这里是 Normal */
    can.CAN_SJW  = CAN_SJW_1tq;
    can.CAN_BS1  = CAN_BS1_5tq;
    can.CAN_BS2  = CAN_BS2_2tq;
    can.CAN_Prescaler = 9;                 /* 36MHz / (9*8) = 500kbps */
    CAN_Init(CAN1, &can);

    filter.CAN_FilterNumber         = 0;
    filter.CAN_FilterMode           = CAN_FilterMode_IdMask;
    filter.CAN_FilterScale          = CAN_FilterScale_32bit;
    filter.CAN_FilterIdHigh         = 0x0000;
    filter.CAN_FilterIdLow          = 0x0000;
    filter.CAN_FilterMaskIdHigh     = 0x0000;   /* 掩码全0 = 全接收 */
    filter.CAN_FilterMaskIdLow      = 0x0000;
    filter.CAN_FilterFIFOAssignment = CAN_Filter_FIFO0;
    filter.CAN_FilterActivation     = ENABLE;
    CAN_FilterInit(&filter);
}

/* 打包：物理量 → 8 字节（过渡课的函数，原样搬过来） */
void EngineData_Pack(uint8_t *d, uint16_t rpm, uint8_t kmh, uint8_t thr_pct, int8_t temp_c)
{
    uint16_t r = rpm * 4;
    uint16_t v = (uint16_t)kmh * 100;
    uint8_t  t = thr_pct * 2;
    uint8_t  c = (uint8_t)(temp_c + 40);
    d[0] = (uint8_t)(r & 0xFF);   d[1] = (uint8_t)(r >> 8);
    d[2] = (uint8_t)(v & 0xFF);   d[3] = (uint8_t)(v >> 8);
    d[4] = t;  d[5] = c;  d[6] = 0;  d[7] = 0;
}

/* 解包 + 打印：8 字节 → 人能看懂的数 */
void EngineData_Show(const uint8_t *d)
{
    uint16_t r = (uint16_t)d[0] | ((uint16_t)d[1] << 8);
    uint16_t v = (uint16_t)d[2] | ((uint16_t)d[3] << 8);
    printf("RPM=%d.%02d  Speed=%d.%02d km/h  Thr=%d.%d %%  Temp=%d C\r\n",
           r / 4, (r % 4) * 25, v / 100, v % 100,
           d[4] / 2, (d[4] % 2) * 5, (int)d[5] - 40);
}

int main(void)
{
    CanTxMsg tx;
    CanRxMsg rx;
    uint8_t i;

    Serial_Init();            /* 工程里的串口初始化，115200 */
    CAN_Node_Init();

#if NODE_A
    /* ===== A 板：只发，每 500ms 一帧，转速缓慢变化 ===== */
    uint16_t rpm = 1000;
    printf("NODE A: send 0x100 every 500ms\r\n");

    tx.StdId = 0x100;
    tx.ExtId = 0;
    tx.IDE   = CAN_ID_STD;
    tx.RTR   = CAN_RTR_DATA;
    tx.DLC   = 8;

    while (1)
    {
        EngineData_Pack(tx.Data, rpm, 48, 10, -19);
        CAN_Transmit(CAN1, &tx);
        printf("A sent RPM=%d\r\n", rpm);
        rpm += 10;
        if (rpm > 3000) rpm = 1000;
        Delay_ms(500);
    }
#else
    /* ===== B 板：只收，收到就打印 ===== */
    printf("NODE B: wait for 0x100\r\n");

    while (1)
    {
        if (CAN_MessagePending(CAN1, CAN_FIFO0) > 0)
        {
            CAN_Receive(CAN1, CAN_FIFO0, &rx);
            printf("ID=0x%03X DLC=%d Data=", rx.StdId, rx.DLC);
            for (i = 0; i < rx.DLC; i++) printf("%02X ", rx.Data[i]);
            printf("\r\n");
            EngineData_Show(rx.Data);
        }
    }
#endif
}
