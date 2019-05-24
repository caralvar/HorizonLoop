/* --COPYRIGHT--,BSD
 * Copyright (c) 2015, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * --/COPYRIGHT--*/
//****************************************************************************
//
// main.c - MSP-EXP432P401R + Educational Boosterpack MkII - Accelerometer
//
//          Displays raw 14-bit ADC measurements for X, Y, and Z axis
//          on the colored LCD.
//
//          Tilting the boosterpack to the 4 different vertical orientations
//          also controls the LCD orientation accordingly.
//
//****************************************************************************

#include <LcdDriver/Crystalfontz128x128_ST7735.hpp>
#include <ti/devices/msp432p4xx/inc/msp.h>
#include <ti/devices/msp432p4xx/driverlib/driverlib.h>
#include <ti/grlib/grlib.h>
#include <stdio.h>

/* Graphic library context */
Graphics_Context g_sContext;

/* ADC results buffer */
static uint16_t resultsBuffer[3];
uint8_t indicatorHeight[2] = { 0, 0};

bool initialReading = true;
void ADCSetup();
void LCDSetup();
void drawTitle(void);
void drawAccelData(void);
uint8_t conversion(Graphics_Context *ctxt, uint16_t x);
/*
 * Main function
 */
int main(void)
{
    /* Halting WDT and disabling master interrupts */
    MAP_WDT_A_holdTimer();
    MAP_Interrupt_disableMaster();

    /* Set the core voltage level to VCORE1 */
    MAP_PCM_setCoreVoltageLevel(PCM_VCORE1);

    /* Set 2 flash wait states for Flash bank 0 and 1*/
    MAP_FlashCtl_setWaitState(FLASH_BANK0, 2);
    MAP_FlashCtl_setWaitState(FLASH_BANK1, 2);

    /* Initializes Clock System */
    MAP_CS_setDCOCenteredFrequency(CS_DCO_FREQUENCY_48);
    MAP_CS_initClockSignal(CS_MCLK, CS_DCOCLK_SELECT, CS_CLOCK_DIVIDER_1);
    MAP_CS_initClockSignal(CS_HSMCLK, CS_DCOCLK_SELECT, CS_CLOCK_DIVIDER_1);
    MAP_CS_initClockSignal(CS_SMCLK, CS_DCOCLK_SELECT, CS_CLOCK_DIVIDER_1);
    MAP_CS_initClockSignal(CS_ACLK, CS_REFOCLK_SELECT, CS_CLOCK_DIVIDER_1);

    LCDSetup();
    ADCSetup();

    while(1)
    {
        MAP_PCM_gotoLPM0();
    }
}

void LCDSetup()
{
    /* Initializes display */
    Crystalfontz128x128_Init();

    /* Set default screen orientation */
    Crystalfontz128x128_SetOrientation(LCD_ORIENTATION_UP); // ORIENTACION SIEMPRE HACIA ARRIBA

    /* Initializes graphics context */
    Graphics_initContext(&g_sContext, &g_sCrystalfontz128x128, &g_sCrystalfontz128x128_funcs);
    Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_GREEN);     // FOREGROUND COLOR (PINTA)
    Graphics_setBackgroundColor(&g_sContext, GRAPHICS_COLOR_BLUE); // BACKGROUND COLOR
    GrContextFontSet(&g_sContext, &g_sFontFixed6x8);
    Graphics_clearDisplay(&g_sContext);
    //drawTitle();
}

void ADCSetup()
{
    /* Configures Pin 4.0, 4.2, and 6.1 as ADC input */
    MAP_GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P4, GPIO_PIN0 | GPIO_PIN2, GPIO_TERTIARY_MODULE_FUNCTION);
    MAP_GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P6, GPIO_PIN1, GPIO_TERTIARY_MODULE_FUNCTION);

    /* Initializing ADC (ADCOSC/64/8) */
    MAP_ADC14_enableModule();
    MAP_ADC14_initModule(ADC_CLOCKSOURCE_ADCOSC, ADC_PREDIVIDER_64, ADC_DIVIDER_8,
            0);

    /* Configuring ADC Memory (ADC_MEM0 - ADC_MEM2 (A11, A13, A14)  with no repeat)
         * with internal 2.5v reference */
    MAP_ADC14_configureMultiSequenceMode(ADC_MEM0, ADC_MEM2, true);
    MAP_ADC14_configureConversionMemory(ADC_MEM0,
            ADC_VREFPOS_AVCC_VREFNEG_VSS,
            ADC_INPUT_A14, ADC_NONDIFFERENTIAL_INPUTS);

    MAP_ADC14_configureConversionMemory(ADC_MEM1,
            ADC_VREFPOS_AVCC_VREFNEG_VSS,
            ADC_INPUT_A13, ADC_NONDIFFERENTIAL_INPUTS);

    MAP_ADC14_configureConversionMemory(ADC_MEM2,
            ADC_VREFPOS_AVCC_VREFNEG_VSS,
            ADC_INPUT_A11, ADC_NONDIFFERENTIAL_INPUTS);

    /* Enabling the interrupt when a conversion on channel 2 (end of sequence)
     *  is complete and enabling conversions */
    MAP_ADC14_enableInterrupt(ADC_INT2);

    /* Enabling Interrupts */
    MAP_Interrupt_enableInterrupt(INT_ADC14);
    MAP_Interrupt_enableMaster();

    /* Setting up the sample timer to automatically step through the sequence
     * convert.
     */
    MAP_ADC14_enableSampleTimer(ADC_AUTOMATIC_ITERATION);

    /* Triggering the start of the sample */
    MAP_ADC14_enableConversion();
    MAP_ADC14_toggleConversionTrigger();
}

uint8_t conversion(Graphics_Context *ctxt, uint16_t x)
{
    float m = 0 - Graphics_getDisplayHeight(ctxt)/(11440.0 - 4900.0);
    float b = -m * 11440.0;
    return (uint8_t) (m * x + b) ;
}

/*
 * Clear display and redraw title + accelerometer data
 */
//void drawTitle()
//{
//
//    Graphics_drawStringCentered(&g_sContext,
//                                    (int8_t *)"Accelerometer:",
//                                    AUTO_STRING_LENGTH,
//                                    64,
//                                    30,
//                                    OPAQUE_TEXT);
//    //drawAccelData();
//}
//
//
///*
// * Redraw accelerometer data
// */
//void drawAccelData()
//{
//    char string[10];
//    sprintf(string, "X: %5d", resultsBuffer[0]);
//    Graphics_drawStringCentered(&g_sContext,
//                                    (int8_t *)string,
//                                    8,
//                                    64,
//                                    50,
//                                    OPAQUE_TEXT);
//
//    sprintf(string, "Y: %5d", resultsBuffer[1]);
//    Graphics_drawStringCentered(&g_sContext,
//                                    (int8_t *)string,
//                                    8,
//                                    64,
//                                    70,
//                                    OPAQUE_TEXT);
//
//    sprintf(string, "Z: %5d", resultsBuffer[2]);
//    Graphics_drawStringCentered(&g_sContext,
//                                    (int8_t *)string,
//                                    8,
//                                    64,
//                                    90,
//                                    OPAQUE_TEXT);
//
//}


/* This interrupt is fired whenever a conversion is completed and placed in
 * ADC_MEM2. This signals the end of conversion and the results array is
 * grabbed and placed in resultsBuffer */
extern "C"
{
    void ADC14_IRQHandler(void)
    {
        uint64_t status;

        status = MAP_ADC14_getEnabledInterruptStatus();
        MAP_ADC14_clearInterruptFlag(status);

        /* ADC_MEM2 conversion completed */
        if(status & ADC_INT2)
        {
            /* Store ADC14 conversion results */
            resultsBuffer[0] = ADC14_getResult(ADC_MEM0);
            resultsBuffer[1] = ADC14_getResult(ADC_MEM1);
            resultsBuffer[2] = ADC14_getResult(ADC_MEM2);

            indicatorHeight[0] = conversion(&g_sContext, resultsBuffer[2]); //ALTURA ACTUAL

            if(initialReading)
            {
                struct Graphics_Rectangle initialRectangle = {0, Graphics_getDisplayHeight(&g_sContext), Graphics_getDisplayWidth(&g_sContext), indicatorHeight[0]};
                Graphics_fillRectangle(&g_sContext, &initialRectangle);
                Graphics_drawRectangle(&g_sContext, &initialRectangle);
                initialReading = false;
            }
            else
            {
                if (indicatorHeight[0] > indicatorHeight[1] + 2 || indicatorHeight[0] < indicatorHeight[1] - 2)
                {
                     if (indicatorHeight[0] > indicatorHeight[1])
                     {
                         int i;
                         Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_BLUE);     // FOREGROUND COLOR (PINTA)
                         for (i = 0; i < indicatorHeight[0] - indicatorHeight[1] + 1; i++)
                         {
                             Graphics_drawLineH(&g_sContext, 0, Graphics_getDisplayWidth(&g_sContext), indicatorHeight[0] - i);
                         }

                     }
                     else
                     {
                         int i;
                         Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_GREEN);     // FOREGROUND COLOR (PINTA)
                         for (i = 0; i < indicatorHeight[1] - indicatorHeight[0] + 1; i++)
                         {
                             Graphics_drawLineH(&g_sContext, 0, Graphics_getDisplayWidth(&g_sContext), indicatorHeight[0] + i);
                         }
                     }
                     indicatorHeight[1] = indicatorHeight[0];
                }
            }
        }
    }
}
