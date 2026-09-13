# Session 2 Speaker Notes

[Slides]([https://example.com](https://docs.google.com/presentation/d/1QSbIQ8ZsvT15QcZd6lBcqJOw_FmQIInytP8Eg_i__rY/edit?usp=sharing))
[Video](https://example.com)

## Slide 1

- Welcome back everyone yay

## Slide 2

- Todays content: communication protocols
- Covering SPI, as well as implementing a custom protocol
- Perhaps some topics we don't have a huge amount of time for

## Slide 3

- It would be great if everything we ever needed was baked into the mcu, and coiuld be accessed via mmio
- Unfortunately, this is too expensive
- We need other chips that do sme of the work we can't do
    - like sensors
    - actuation
    - communication
- Somehow, our mcu needs to talk to these other chips
- For that, we have communication protocols that define the language for chip-to-chip communcation
- Definition: Communication Protocol: A well defined grammar for transferring information from one agent to another
- Protocols are seperated into layers:
    - Physical layer: the actual electrical signals. Things like voltage, polarity, etc.. The physical layer is usually
      decided for us
    - Transport layer: How we get the bytes from one place to another. SPI, I2C, UART, these are what we are interested
      in
    - Application layer: Chip-dependant. For an accelerometer we'll be transferring acceleration data, for uart text,
      etc.

## Slide 4

- English example
- We are speaking english, talking about programming. This is an application/context specific language (i wouldnt use
  the same words talking about food)
- The english itself is being transported to you all via speech. I could alternatively transport it to you via text
- The speech is physically represented by vibrations in the air. I could use water, or another medium for the physical
  speech
- Etc.

## Slide 5

- I2C is a common chip-to-chip protocol
- Based on 2 wires: SCL and SDA. One clock and one data line
- Devices each have an ID to choose which one we talk to
- I2C is *independent of the physical layer!*. I2C can work on any voltage

## Slide 6

- SPI is another commonly found protocol
- 1 clock, 2 data: can send and receive data at the same time
- Also physical layer independent!

## Slide 7

- As an example, we will connect to a thermocouple over I2C

## Slide 8

- Timing diagram of mcp9600
- When communicating, we first transmit the id of the chip
- Then, we send what "register" we want to R/W from
- Then, we send the data/receive the data

## Slide 9

- The primary register we are interested in is the hot-junction temperature: 0x00

## Coding Time!

- First need to enable I2C1 on PB8/9
- Need to write some code to set up the chip:
    - Check the chip responds on the I2C bus
    - Check it reports the correct revision - if its wrong then the chip is probably broken
    - Set up the config to use the right thermocouple
    - Using hal instead of register-direct because RD is too much work
- To actually pull data off, we need to read from the correct register
    - Read data with I2C
    - Convert the raw value from the chip into a float with some bit trickery and math given by datasheet
- To use, just call our setup and read functions
- Output the value over uart
- demo i2c transaction with SALEAE

## Slide 11

- Interrupts!!
- Sometimes, we need the cpu to stop drop and roll to some other code for a bit
- Ex: if we are receiving text from uart, we want to grab it and store it in memory so the hardware can receive the next
  char, and then continue whatever we were doing before
- Ex: I am giving a lecture to you all. I dont want to sit here waiting for a question. When you raise your hand, you
  are raising an interrupt so I stop what Im doing, answer your question, then continue where I left off. If I do stop
  and ask for questions, that is an example of "polling"
- How does the CPU know what function to call when an interrupt occurs? IVT in startup_stm32h753xx.s
    - Each entry in the table is an interrupt defined by the ARM architecture, and we give each position the name of the
      function to call
    - Lots of other neat stuff in the startup assembly, as later if interested

## Slide 12

- The HX711 is a load cell board that allows us to get the weight of something
- It is cringe and has its own, custom protocol for transferring data
- Describe the protocol

## Slide 13

- This is the software setup we're going to use
- Red are real life events that can happen at any time
- Blue are what the MCU does in response

## Slide 14

- We have all these random events that can occur, and we want the cpu to stop and deal with the events as they come
- When an event happens, we need to create a specific waveform with the clock, and have some decently-precise timing
  requirements
- For this we can use a counter
- Describe counter: count register, incremented per clock pulse, automatically reloads after some count, can compare
  with the count value to trigger other events

## Slide 15

- Datasheet time! Have everyone look through section 38, subsection 4 to see how the timer works and what registers
  might be useful. (~5ish minutes?)
- Hints on the slide!

## Slide 16

- Mostly complete list of registers we need

## Slide 17

- Coding time!
- Set up cubemx things:
    - PB15 set to GPIO_EXT15
    - Pin lock PC6
    - ENABLE NVIC FOR EXTI!!
- Copy in the constants for port and pins, timer to use, etc.
- Talk about how we are actually going to use the interrupts to track the data
    - DIN interrupt fires when the DIN line goes H2L, indicating there is data ready. This interrupt will then set up
      the variables to store the data, and kick off the timer
    - The timer will automatically be generating the pulses for us
    - The CC interrupt will trigger at half-pulse, at which point we read in one bit of data and set it at the
      appropriate position. We only want to read 24 bits, since the 25th is just to select the next conversion type and
      is not actual data.
    - The UPDATE interrupt is called once the 25 pulses have completed. At this point, we can clean up the timer,
      finalize the reading, and set up the DIN interrupt to receive the next data frame
    - Must disable DIN interrupt in the DIN interrupt, and re-enable in UPDATE so that the actual data signal doesnt
      continuously trigger the start of the process
    - Create state variables (inprogressreading, latestReading, mask)
- Set up DIN irq: copy in function
    - We have to tell the cpu we have actually handled the interrupt with
      __HAL_GPIO_EXTI_CLEAR_IT, otherwise once we returned from the interrupt it would get re-triggered
    - Must disable DIN irq so its not retriggered
    - start counter
- Set up CC irq:
    - Acknowledge interrupt
    - Read in bit
- Set up UPDATE irq:
    - Acknowledge interrupt
    - finalize reading
    - Re-enable DIN
- Modify IVT to point to correct functions:
    - 188
    - 192
    - 194
- Copy in setup function:
    - Have to first configure PC6 to be alternate function
    - setup various timer registers
    - Enable the interrupts both in timer->DIER and in NVIC
- Show process with SALEAE

## Slide 18

- We only cover these if we have time

## Slide 19

- DMA!!!
- We want to be able to asynchronously read data from peripherals without having to sit there and wait for transfers
- DMA handles that for us

## Slide 20

- We have a few different DMA types, and the bus diagram shows us what each DMA is connected to and can access
- IMPORTANTLY: DMA cant access the TCMs of the CPU. Need to make sure DMA location is somewhere in SRAM so that the DMA
  can actually access it
- We will talk more about DMA in engine, since that is the current area of optimization

## Slide 21

- Freertos!
- Threading!
- Timers!
- Yay!
- Its been a long day im tired of typing my speakernotes :(

## Engine Projects

- FreeRTOS integration, Ethernet, and threading model for RCP
- ARM CMSIS DSP signal processing instead of first-google-result low pass filter - actually analyze frequency response 
  and phase shifts
- Asynchronizing all drivers
- Backup SRAM shenanigans
- Networking and web app for RCI
- Ebox wires gooder
- Hotfire week 8?