# STM32L476RG

This repository contains firmware for the STM32L476RG microcontroller used in project E676-002 Rev. B (an automated testing tool). The firmware implements the following functionality:

- **UART4/UART5**: Speed Conversion
- **TIM15**: Pulse Width Modulation of a Square Wave
- **TIM4**: Frequency Capture
- **OPAMP2**: Amplification of an Analog Signal
- **ADC_IN**: Analog-to-Digital Conversion for DSP

## Pin Layout and Allocation
### Pin Configuration Nucleo Board and STM32

| Nucleo Pin | Nucleo Block | STM32 Pin Name   | STM32 Pin Mark | Description                                                 |
|------------|--------------|------------------|----------------|-------------------------------------------------------------|
| 38         | CN7          | ADC_IN           | PC0            | Beta_RX_Amplified - Conversion to digital signal            |
| 28         | CN7          | UART4_TX         | PA0            | TXD 250000 bits/s                                           |
| 30         | CN7          | UART4_RX         | PA1            | RXD 250000 bits/s                                           |
| 35         | CN10         | TIM15_CH1_PA2    | PA2            | Beta_TX - PWM - Outputs square wave at defined frequency    |
| 13         | CN10         | OPAMP2_VINP      | PA6            | Beta_RX_A - Analog Input                                    |
| 15         | CN10         | OPAMP2_VINM      | PA7            | Beta_RX_Ref - Analog Reference                              |
| 34         | CN7          | OPAMP_VOUT       | PB0            | Beta_RX_A_Amplified - Analog Output - Connects to ADC_IN    |
| 3          | CN7          | UART5_TX         | PC12           | TXD 171 bits/s                                              |
| 5          | CN7          | UART5_RX         | PD2            | RXD 171 bits/s                                              |
| 17         | CN10         | TIM4_CH1         | PB6            | Beta_RX_D - Frequency recognition                           |

### Pinout View
| ![image](https://github.com/user-attachments/assets/78737947-a364-4d14-b793-77794824b3e1) |
|-|

## UART4/UART5: Speed Conversion
### Speed Specifications

**UART 4**

| Parameter         | Value |
|-------------------|-------|
| Baud Rate         | $$250,000 \ \text{bits/s} = 31250 \ \text{bytes/s}$$ |
| Overrun Time      | $$\frac{255 \ \text{bytes}}{31250 \ \text{bytes/s}} \approx 0.0082 \ \text{s}$$ |

**UART 5**

| Parameter         | Value |
|-------------------|-------|
| Baud Rate         | $$171 \ \text{bits/s} = 21.38 \ \text{bytes/s}$$ | 
| Overrun Time      | $$\frac{255 \ \text{bytes}}{21.38 \ \text{bytes/s}} \approx 11.93 \ \text{s}$$ |


### Buffer Size and Speed Ratio

| Parameter    | Formula                                                                                          |
|--------------|--------------------------------------------------------------------------------------------------|
| Max Buffer   | $$2040 \ \text{bits} = 255 \ \text{bytes}$$                                      |
| Speed Ratio  | $$\frac{\text{UART5 Speed}}{\text{UART4 Speed}} = \frac{31250}{21.38} \approx 1.462$$ |

UART4 transmits at 31250 bits per second, and the buffer is 2040 bits in size, meaning it can overflow quickly if a large amount of data is processed at once. The circular buffer helps by allowing new data to overwrite old data once it has been transmitted, effectively managing the speed difference. However, the available space will vary. To avoid overflow, the data should be sent in chunks that fit within the buffer, ensuring that each chunk is fully transmitted before new data is introduced.

### DMA Settings

| UART      | RX Mode          | TX Mode      |
|-----------|------------------|--------------|
| UART4     | Circular         | Normal       |
| UART5     | Circular         | Normal       |

### NVIC Settings

| UART   | Global Interrupt          | DMA Channel 1 Global Interrupt | DMA Channel 2 Global Interrupt |
|--------|---------------------------|--------------------------------|--------------------------------|
| UART4  | Enabled                   | Enabled                        | Enabled                        |
| UART5  | Enabled                   | Enabled                        | Enabled                        |
	
Enabling the UART global interrupt is essential for the callback functions to work correctly.

### RTOS
Communication between UART4 and UART5 can occur simultaneously, implemented using the RTOS available on the STM32 chip. The system runs two custom tasks:

- `void StartUartPathIn(void const * argument)` – Handles the path from the FTDI to the STM32 chip.
- `void StartUartPathOut(void const * argument)` – Handles the path from the STM32 chip to the UUT (Unit Under Test).

### Functions implemented 
To ease working with the code, communication is implemented in a function called transmitData which is used in both tasks defined in the RTOS. This function is responsible for communication between the UARTs and is described in more detail in the attached code. Additionally, the UART functionality includes an algorithm in the commandRecognition function, which recognizes a 5-word long code. If the code is recognized, it triggers a defined event.

- ` void transmitData(UART_HandleTypeDef * huart, uint8_t dmaBufferHS[BUFFER_SIZE_Y][BUFFER_SIZE_X], int *counterBufferCpltHS_rx_ptr, int *counterBufferCpltHS_tx_ptr, int *counterHS_tx_ptr)`
- `int commandRecognition(const char code[], int *counterCommands_ptr, int *counterBufferCpltHS_rx_ptr, uint8_t dmaBufferHS[BUFFER_SIZE_Y][BUFFER_SIZE_X])`

### Potential Improvemements
If continuous transmission is required between the STM32 chip and the FTDI chip, software flow control can be implemented. The FTDI chip used on the PCB supports software handshakes such as XON/XOFF.

## Crystal - LSE - OSC32 
The selected crystal has a specific transconductance (gm), and the STM32 chip has requirements for a critical transconductance (gm_crit). Calculations for the transconductance of the chosen crystal indicate that the STM32 chip needs to be configured with high drive strength for the low-speed external (LSE) crystal oscillator in order to meet the required transconductance and achieve optimal operating conditions.

`LSEDRV[1:0] = 10` // Medium High Drive  
`LSEDRV[1:0] = 11` // High Drive

## TIM15: Pulse Width Modulation of a Square Wave
Pin TIM15_CH1 on the Nucleo board is reserved for the virtual COM port as part of the programmer circuitry. The functionality testing for the code was performed on TIM1_CH1. When the RTOS is enabled, TIM1 is reserved, and TIM8 can be used for testing purposes.

### Clocks
 
**Testing SET UP:**
HSI powers up the PLLCLK
HCLK = 8MHZ

**Recommended Set Up on board:**
The board should be running with HSE

### PWM Configuration
PCLK = 8MHZ
PSC (Pre-Scaling) = 0
f_tim = 8 MHz
f_pwm = 8.76119 KHz

ARR (Auto Reload Register) = 913
PULSE = 457

### Frequency Output Characteristics

| Parameter   | Value                      |
|-------------|----------------------------|
| ΔT          | 1.7496618951312977 s       |
| Nfalling    | 15.334                     |
| Nrising     | 15.334                     |
| fmin        | 8756.567425431042 Hz       |
| fmax        | 8771.929824593875 Hz       |
| fmean       | 8763.952048190406 Hz       |
| Tstd        | 2.8630427804397434e-8 s    |
| fbaud       | 17543.859649467682 Hz      |
| Δf          | 15.362399162833754 Hz      |

**Formulas**
| Variable                     		  | Formula                                                                                         |
|-----------------------------------------|-------------------------------------------------------------------------------------------------|
| f<sub>PWM</sub>              	          | $$f_{PWM} = \frac{f_{TIM}}{(ARR + 1) \times (PSC + 1)}$$                                        |
| ARR                            	  | $$ARR = \frac{f_{TIM}}{f_{PWM}} - 1$$ 							    |
| ARR with scalling		          | $$ARR = \frac{f_{TIM}/n}{f_{PWM}} - 1$$                                 			    |
  
## TIM4: Frequency Capture

### Configuration:
- Parameter: Channel 1->Input Capture Direct Mode 
- NVIC: TIM4-> Global Interrupt Enabled

The clocks was set as HSI->PLCK when testing. The clocks should be set to HSE->HSE for more accurate results. All the clocks run at 8MHZ.  
 
### Frequency:
The frequency can be set by adjusting variables: 
- int f_trigger = 8763;
- int f_threshold = 200;
- int f_debounce = 30;

The readings are not entirely accurate, so a tolerance threshold is added. This threshold is adjustable through the variable f_threshold. To ease testing with external signal generators, a step size of 30 Hz is included, which can be adjusted using the variable f_debounce.

## OPAMP2: Amplification of an Analog Signal

### Configuration
- Mode: PGA Connected
- Parameter: Power Mode -> Normal
- Parameter: PGA Gain -> 2

| **Gain**     | **Min GBW (550 kHz)**  | **Typical GBW (1600 kHz)** | **Max GBW (2200 kHz)** |
|--------------|------------------------|----------------------------|------------------------|
| **Gain = 2** | 275 kHz                | 800 kHz                    | 1100 kHz               |
| **Gain = 4** | 137.5 kHz              | 400 kHz                    | 550 kHz                |
| **Gain = 8** | 68.75 kHz              | 200 kHz                    | 275 kHz                |
| **Gain = 16**| 34.38 kHz              | 100 kHz                    | 137.5 kHz              |

When testing the functionality, the op-amp showed that it outputs a stable voltage when the signal oscillates below a frequency of 25 kHz.
Maximum applification is 3.3V

### Offset 
The bias voltage should be set to avoid amplifying the offset voltage. The offset can be subtracted from the signal by applying a bias voltage to the non-inverting input.

**Testing set up:**
- **Signal Generator** - Simulating beta_rx_a
	- Pin: V+
	- Sin wave - 8761 kHz
	- Amplitude: 250mV pp
	- Offset: 150mV pp
- **Power Supply** - Simulating beta_rx_ref
	- Pin: V-
	- Amplitude: 300mV

### Results
| ![OPAMP2 Output](https://github.com/user-attachments/assets/44c8b0e4-b6d1-46e7-87c1-0dd758194e9e) |
|-|

*Figure: Input versus output using OPAMP2 showimg clipped peaks and excessive gain*

**Amplitude**

The amplitude is effectively clipped at both the top and bottom peaks if the offset is correctly subtracted. This efficiently converts the sinusoidal wave into a square wave which is sufficient for frequency detection through further processing.

**Frequency** 
- Sin wave - 8761 kHz
- F_in = 8761 kHz
- F_out = 8761 kHz

The output frequency matches the input frequency when the offset on the inverting rail is set equally to the offset of the signal input on the non inverting rail.

**'PGA not connected' Configuration**
The amplifier was also tested in its 'PGA not connected' mode, where the inverting input was grounded. This resulted in an output with a very high offset and maximum amplification. Additionally, the frequency output was slightly distorted.

### Potential Improvements
During the PCB design stage, OPAMP2 was mistakenly considered a differential op-amp because it exposed the V+ and V- pins, allowing for programmable amplification through the STM32 chip. However, after further research, it was determined that OPAMP2 operates as a non-inverting amplifier in PGA mode and does not support differential configuration. The current configuration should be replaced with a more suitable alternative.

OPAMP2 also supports a standalone configuration that can be used as a differential op-amp; however, this disables the PGA functionality.

One option is to add DC-blocking circuitry ahead of the non-inverting operational amplifier input on the STM32 chip to retain the PGA capabilities. Alternatively, the amplification stage could be replaced with an external op-amp configured in differential mode.

## ADC_IN: Analog-to-Digital Conversion for DSP
The ADC_IN opens possibilities for digital signal processing. The input to ADC_IN is a square wave, as shown in the OPAMP2 section. The functionality for ADC_IN is not implemented, as it is not necessary for the scope of the project. Potential functionalities that could be implemented include amplitude recognition or frequency recognition.

- **Amplitude recognition** can be implemented simply using the functionality provided on board the STM32.
- **Frequency recognition** can be implemented using an internally wired timer supporting Input Capture mode or Fast Fourier Transform (FFT).

### Sampling Time Calculation
The calculation shows that the maximum sampling time for capturing a wave with a frequency as high as 8 kHz needs to be at most 444.28 cycles. The nearest available value that includes the 8 kHz wave in range, when the ADC clock frequency is set to 8 MHz, is 247.5 cycles. 

By reducing the sampling time to 247.5 cycles, it allows for the capture of higher frequency signals. The maximum frequency that can be captured when the ADC clock is set to 8 MHz and the sampling time is 247.5 cycles is: 30769 Hz.

#### Calculating for Sampling time

|$$\text{Sample Rate} = 2.2 \times 8 \ \text{kHz} = 17600 \ \text{Hz}$$|
|-|

|$$17600 = \frac{1}{T_{\text{conv}}} \Rightarrow T_{\text{conv}} = \frac{1}{17600} = 56.81 \ \mu s$$|
|-|

|$$56.81 \ \mu s = \frac{\text{Sampling Time}}{8 \ \text{MHz}} + \frac{12.5}{8 \ \text{MHz}} \Rightarrow \text{Sampling Time} = (56.81 \ \mu s - \frac{12.5}{8 \ \text{MHz}}) \times 8 \ \text{MHz} = 444.28 \ cycles$$|
|-|

#### Calculating Sample Rate when Sampling time is 247.5 cycles

|$$T_{\text{conv}} = \left(\frac{247.5}{8 \text{MHz}} + \frac{12.5}{8 \text{MHz}}\right) = T_{\text{conv}} = 3.25 \times 10^{-5} \ \text{s}$$|
|-|

|$$\text{Sample Rate} = \frac{1}{T_{\text{conv}}} = \frac{1}{3.25 \times 10^{-5}} = 30769 \ \text{Hz} \ \approx 30.769 \ \text{kHz}$$|
|-|

### Resolution 
VDDA/VREF on the STM32 chip is set to the same level as VDD, which is 3.3 V. The 12-bit resolution of 4096 steps is then mapped across the 0 to 3.3 V range. 

|$$\text{Smallest Step} = \frac{3.3 \ V}{4096} \approx 0.00080586 \ \text{V = } 805.86 \ \mu\text{V}$$|
|-|

### Formulas
| Variable                     | Formula                                                   			        								|
|------------------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------|
| T<sub>conv</sub>             | $$\frac{\text{Sampling Time} \ \text{Cycles}}{f_{\text{PLLSAI1R}}} + \frac{{T_{\text{SAR}}} \ \text{Cycles}}{f_{\text{PLLSAI1R}}}$$					|
| Sampling Rate                | $$\frac{1}{T_{conv}}$$            															|
| V<sub>in</sub>               | $$\text{ADC Resolution} \times \left( \frac{\text{Reference Voltage}}{4096} \right)$$									|
| Reference Voltage            | $$V_{REF+} - V_{REF-}$$        					                								|

- T_SAR = 12.5 for 12bit resolution; SAR stands for Successive Approximation Register
- 
### DMA Settings
| ADC       | Mode         |
|-----------|--------------|
| ADC1      | Circular     |

### NVIC Settings
| ADC Interrupts		 | Enabled |
| -------------------------------| ------- |
| DMA1 Channel1 Global Interrupt | Yes     |
| ADC1 and ADC2 Interrupts       | Yes     |


  
## Conclusion: 
Any configuration not mentioned should be left as default.
 
## Frequently Used Resources

- [STM32L476RG Datasheet](https://github.com/user-attachments/files/17096444/stm32l476rg.pdf)
- [STM32L476RG User Manual](https://github.com/user-attachments/files/17096471/um1724-stm32-nucleo64-boards-mb1136-stmicroelectronics.pdf)
- [FT4233HPQ Datasheet](https://github.com/user-attachments/files/17096477/FTDI-FT4233HPQ-TRAY-datasheet.pdf)
- [STM32L4/L4+ HAL and low-layer drivers](https://github.com/user-attachments/files/17145554/um1884-description-of-stm32l4l4-hal-and-lowlayer-drivers-stmicroelectronics.pdf)
