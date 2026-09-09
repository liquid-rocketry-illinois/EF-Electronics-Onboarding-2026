# Session 1 Speaker Notes

[Slides](https://docs.google.com/presentation/d/1FYvPtxzgp9Z8kreNQ4ZyVJ6RvzKiU2WYXGnqfnrndk0/edit?usp=sharing)

[Workshop Recording](https://youtu.be/0P-nO1aQ5uo)

## Slide 1

- yayyyy onboarding
- let everyone introduce themselves

## Slides 2-5 (? what do you guys think)

- project slides for each lead
- don't give full explanation, but general overview of tech team responsibilities and goals for the year. recap from
  what you talked about in the tech team general meetings
- short and sweet, just as a reminder to people

## Slide 6

- Lightly go through TOC
- More of: we will learn what a mcu is, and we'll get through some introductory examples on the topic
- at this point I will ask if anyone needs help with their environment setup from the intro materials (am making a video
  on installing everything). If they do, would it be ok if you (joseph) can help these people, while I move on with
  content that way we can do a little multitasking

## Slide 7

- Overview of what an mcu is
- Go through the points on the table
- The picture has an mcu on it
- Arduino is an mcu platform
- Mention how LRI is mostly using STM32 for our mcu platform, so which is why we will be covering it in workshopss
- Main purpose of an mcu is to provide easy access to hardware from code, in a device simpler than a PC
- Mcu does still do the core things a computer does:
    - Has CPU, RAM, IO
    - Executes code
- Show a nucleo in person. We will be coding this as a follow along

## Coding Portion

Walkthrough portion going over some basic coding. Stop frequently for questions and help. Again joseph, if you can walk
the room and help people who need it while im talking that would help move the workshop along so that im not constantly
stopped for tech help (ofc if you cant fix whatevers broken we can stop and ill help). If someone has a content question
though that is good to ask to everyone so that the whole session can hear the answer.

- We will be going through some basic examples using the mcu
- First open cubemx and start new project
- Search for mcu `STM32H753Z` and select the only option, and click start project
- Click yes to configure the mpu, after loading you should be in the mcu view
- Talk about how this is the actual package layout for the chip
- Go over the different types of pins (power is pale yellow, greenish have set purposes, gray is gpio)
- what is a gpio
- Controls for panning camera
- Click on gpio -> can see all the things it can do
- Most of these things are from _peripherals_, additional hardware in the silicon that is for a specific purpose
    - Ex adcs, ethernet controller, usb controller
    - These peripherals can also be configured on the side panel (we will get to that another day)
- For now, we will just be doing a blink program with the onboard LEDs and button
- Set PB14 -> LED, PE1 -> LED2, PC13 -> usrbtn
- enable swd, save explanation for later
- we will make one led blink at a constant rate, and another copies the button status
- Leave the clocks alone
- give project name/path (mx will make a folder)
- change toolchain to cmake
- generate

In the coding, first need to setup environment:

- Set cmake options (-DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake)
- Setup openocd download and run
- talk about generated code later

Setup the LEDs:

- paste code, then talk about it
- talk about user code blocks
- explain program itself
    - we can use HAL_GPIO_Read/Write/TogglePin to control a GPIO pin
    - For the led that follows the button, we read from the pin. If its high, indicating the button was pushed, we turn
      the LED on, else turn it off
    - For the time delayed toggle, we use TogglePin so we dont have to keep track of the pin state. We also dont use
      HAL_Delay because it would mean we cant check the button
- now talk about the surround code
    - the function we were editing was main, which is the entrypoint just like on a computer
    - First some system things run that configure various things about the mcu itself
    - then we initialize the gpios
        - go into MX_GPIO_Init()
        - the code needs to set up the gpios before we can use them, and this is what this function is doing
        - for each gpio we need, we have to tell it what function we want it to perform (remember each gpio can do a
          variety of things)
        - we will look at the register version later
- for the rest of the generated code, whats important to know is that the remaining files mostly deal with system level
  code. If anyone is interested, they can look at the full mcu startup process by looking at the assembly
- upload and test code
- yay it works
- Switch back to slides to talk about debugging and upload process

## Slide 9

[//]: # (I dont like this section, skip it)

On the slides:

**VERY** lightly cover the debugging pipeline

- while it looks like we are connected to the mcu with usb, in reality we are going through a chain of steps that end
  with us communicating in a simpler protocol called SWD
- SWD allows debugging access into the mcu internals
- There are a few pieces of software we need to run on our computers to connect our ide to the mcu, namely gdb and
  openocd
- From there, the computer magically talks to the mcu

[//]: # (I dont like this section, hence why it is commented out)

[//]: # (- One thing that is good to know is how we are actually interacting with the mcu)

[//]: # (- For an mcu, usb is very hard, so we are not actually debugging directly with usb)

[//]: # (- we go through the *pipeline*)

[//]: # (    - MCU supports a hardware protocol specifically for debugging called SWD)

[//]: # (    - This SWD is then routed to a debug probe, which acts as a mediator between computer and mcu connecting the usb and)

[//]: # (      swd busses together)

[//]: # (    - on the computer, openocd conencts to the debug probe connected over usb, and begins hosting a connection we can)

[//]: # (      use to interact with the debugging probe and mcu)

[//]: # (    - We use a general purpose debugger called GDB which we connect to openocd, to perform the actual debugging)

[//]: # (    - Finally, the IDE connects to gdb to control the debugging process and present the results to the user)

[//]: # (- On a nucleo, the debugging probe is actually a second stm mcu &#40;point to it on the board&#41;)

[//]: # (- this chain of processes is how we can actually debug the mcu)

Set up printf:

- Blinking leds is nice, but we want to do more
- Debugging capabilities are nice
- Our debugging probe embedded on the nucleo has a serial port available for this purpose
    - when we say serial, we mean an uart/RS232 port, or the serial port like on an arduino.
- to connect the mcu to this serial port for output, we connect it to the uart port on the debug probe
- the debug probe will forward anything sent on its uart bus over usb to our computer
- in fact, with clions serial monitor we can see there is a device called stlink already available
- so, we can use a uart peripheral to output text characters we can read to have a print function
- to start, enable usart3 on pd8 and 9. On this nucleo model, these pins are conencted to the debug probe's uart pins,
  so by using this uart device we can output directly through the debug probe to usb
- enable usart3 in async mode
- run clock solver, we wont talk about clocks right now
- regenerate

Do manual text transmission first:

- setup static char buffer
- use hal transmit to send, observe in serial monitor
- now we want to connect to printf
- we will implement the _write syscall
    - a syscall is a routing provided by the operating system to perform some kind of IO
    - since we have no os, we have to implement the syscall ourselves
    - go to user code section
    - _write syscall takes in 2 important things: a pointer to a buffer containing the data we want to write, and the
      amount of data to write
    - it also takes a file parameter, but this is related to an actual system which we don't want to have. if youre that
      curious, we can talk later
    - anyways, we will pass this buffer and length to our uart write function
    - now include stdio
    - can now use printf yippee

Back to slides,

## Slide 10

- Computer theory time!
- Modern computers follow the von neumann model:
    - cpu
    - memory
        - storage space for data the cpu is working on
        - there are multiple types that do different things
        - but to the von neumann model, memory is the data the cpu is operating on
    - io
        - without inputs and outputs, the cpu+memory cant really do much (like output its result!)
        - is how the machine interacts with the world

## Slide 11

- contains the control circuitry which coordinates every other part of the machine
- contains the arithmatic and logic units which perform the actual calculations
- also responsible for moving data around between memories
- Modern cpus are based on the x86 architecture introduced by the intel 8086 processor in the 1970s
- Later extensions introduced the 64 bit variant called x64, amd64, or x86_64
- These kinds of cpus dominate laptop/PC computers, and are large enough for us to see
- x86 too big, we go smol with arm cortex M

## Slide 12

- Memory stores data for the computer
- RAM is probably the most familiar type of memory
    - used for fast access to data the cpu is working on
    - resets on power loss, so not suitable for long term storage
- Flash is the same when we refer to flash drive
    - can be used for long term storage
    - slower than ram
    - can be bigger for cheaper
- In an mcu, we need flash for storing program code, and ram for storing working data

## Slide 13

The memory rabbit hole goes much deeper:

- RAM is actually kinda slow relative to the cpu, so we introduce another memory layer called cache which is like even
  faster ram, but is very small
- Tightly coupled memory: Like cache but EVEN FASTER. Fastest type of *memory*
- CPU Registers: The data fastest accessible to the cpu. literally in the cpu itself. cant get much more direct than
  this. extremely limited in quantity

## Slide 14

- IO devices in an mcu are called peripherals
- gpios, uart, usb, spi, i2c, timers, analog sensors, etc
- going to talk about these more later

## Slide 15

- How do all these things connect to each other?
- The BUS is a common set of circuity that connects all the parts together
- Bus masters are things that initiate data transfers between devices, most notably the CPU
- Bus slaves are things that receive and respond to data sent on the bus, most notably the peripherals
- The bus structure is complex and is actually composed of multiple smaller busses
- There are even bus masters besides the cpu for doing other cool things
- this diagram does NOT need to be understood right now, but it is the bus diagram for the mcus

## Slide 16

- talk about how we actually interact with these peripherals
- peripherals all have a set of registers that allow us to control their behavior
- the registers of each peripheral are mapped into the address space of the cpu, so we can access them like any other
  memory location
- this means accessing them through pointers to memory locations
- as an example, the usart3 device we have been using:
    - it is base located at 0x40004800
    - each register is an offset into that base, for example these are the first few registers

## Slide 17

- as more practical example, we will replicate the led code we made by going directly to the hardware registers, instead
  of the HAL functions
- gpios are organized into groups of 16 into ports
- each port contains the control registers for its associated gpios
- to set up a gpio, we need to tell it to be in output mode, and we need to be able to control the output
- pull up datasheet, section 11.4
    - scroll through the register list
    - the ones we need are:
        - 11.4.1
            - this register allows us to set the function of the gpio
            - we can see it organizes each bit in the register into groups of 2
            - these groups of 2 control the mode for the pin that is connected
        - 11.4.5 for input
            - by reading from this register, we can get the value at the pin its connected to
        - 11.4.6 for output
            - by writing to this register, we control the output level of the pin

Back to code:

- Create a new function that gets called before all the random stuff
- init uart so we can use it for printf
- init clocks for gpios `__HAL_RCC_GPIOx_CLK_ENABLE()`
- Use the GPIOx macro for convenient access to registers
- show how this macro is a pointer to the gpio base location
- set moder
- construct the loop
- we dont have a toggle function, so we need an extra state variable for the blinker
- everything else is self explanatory
