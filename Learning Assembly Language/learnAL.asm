 ******************************************************************************************
*
* Title:      	60 Second Clock, Calculator, and ADC Data Acquisition Program
*
* Objective:  	CMPEN	472	Homework 12
*
* Date:       	Dec. 3, 2020
*
* Programmer: 	Gabien Bryan
*
* Company: 			The Pennsylvania State University Department of Computer Science
*          			and Engineering
*
* Program: 			Simple SCI Serial Port I/O and Real Time Interupts
*
* Algorithm:  	Serial Port input
*								Character to decimal conversion
*           	 
* Register Use: A: Serial port data   
*            	 
*            	 
* Memory use: 	RAM Locations from $3000 for data
*                           	from $3100 for program
*
* Input:      	Parameters hard coded in the program           	 
*
* Output:     	Return information in terminal
*
* Observation:  This program starts a clock based on user input.   
* 
* Term ATD = ADC = Analog to Digital Converter
*
* For Simulator:
*		Serial communication at fast rate (750K baud).
*		Typewriter program, but when 'enter' key hit, ADC number printed.
*		Single AD conversion, 8bit, right justified.
* 	For CSM-12C128 Board:
*	Serial communication at 9600 baud only (different from Homework 11/12)
*		This program prints what user types on a terminal 
*		window - ie. a typewriter program.  However, when a user hits an 
*		'enter' key, a hexadecimal number will be printed on the terminal. 
*		The hexadecimal number represents the analog voltage level of the
*		ADC input channel 7 (pin number 10 of the CSM-12C128 board Connector J1).
*		The program does single AD conversion in 10bit but only the upper
*		8bit is used for the conversion to the hexadecimal number.
*		Use Freescale CodeWarrior, for the MC9S12C128 Programming
*            	 
******************************************************************************************
***************************************************************************************************
; export symbols - program starting point
            XDEF        Entry        ; export 'Entry' symbol
            ABSENTRY    Entry        ; for assembly entry point

; include derivative specific macros
PORTA				EQU					$0000
DDRA				EQU					$0002

PORTB       EQU         $0001
DDRB        EQU         $0003

ATDCTL2     EQU  			$0082   ; Analog-to-Digital Converter (ADC) registers
ATDCTL3     EQU  			$0083
ATDCTL4     EQU  			$0084
ATDCTL5     EQU  			$0085
ATDSTAT0    EQU  			$0086
ATDDR0H     EQU  			$0090
ATDDR0L     EQU  			$0091
ATDDR7H     EQU  			$009e
ATDDR7L     EQU  			$009f

SCIBDH      EQU         $00C8        ; Serial port (SCI) Baud Register H
SCIBDL      EQU         $00C9        ; Serial port (SCI) Baud Register L
SCICR2      EQU         $00CB        ; Serial port (SCI) Control Register 2
SCISR1      EQU         $00CC        ; Serial port (SCI) Status Register 1
SCIDRL      EQU         $00CF        ; Serial port (SCI) Data Register

CRGFLG      EQU         $0037        ; Clock and Reset Generator Flags
CRGINT      EQU         $0038        ; Clock and Reset Generator Interrupts
RTICTL      EQU         $003B        ; Real Time Interrupt Control

;*   CodeWarrior project MUST specify MC9S12C32 chip for the terminal simulation to work.

TIOS				EQU				$0040		;	Time Input Capture (IC) or Output Compare (OC) select
TIE					EQU				$004C		;	Timer Interrupt enable register
TCNTH				EQU				$0044		;	Timer free running main counter
TSCR1				EQU				$0046		; Timer system control 1
TSCR2				EQU				$004D		; Timer system control 2
TFLG1				EQU				$004E		; Timer interrupt flag 1
TC2H				EQU				$0054		; Timer channel 2 register

CR          equ         $0d          ; carriage return, ASCII 'Return' key
LF          equ         $0a          ; line feed, ASCII 'next line' character
SPA					equ					$20					 ; space, ASCII 'Space' key
PLUS      	EQU     		$2B
MINUS     	EQU    	 		$2D
MULT      	EQU     		$2A
DIV       	EQU     		$2F
TAB1				EQU					$09

;*******************************************************
; variable/data section
            ORG    $3000             ; RAMStart defined as $3000
                                     ; in MC9S12C128 chip

ATDdone			DS.B			1				; ADC finish indicator, 1 = ATD finished
countusec		DS.W			1				; 16 bit interrupt counter for usec
ret					DS.B			1				; decimal return, 8 bit
buff				DS.B			3				
          	
num1      	DS.B    4
opChar		 	DS.B    1
num2      	DS.B    4
num1count 	DS.B    1             	; number values in NUM1
num2count 	DS.B    1
num1sum   	DS.B    1
num2sum   	DS.B    1
result    	DS.B    8             	; stores the result of the operation (+-/*)
rescount		DS.B	  1
ctr2p5m     DS.W   1                 ; interrupt counter for 2.5 mSec. of time
times       DS.B   1
buff1				DS.B	 1								 ; stores each character inputed by user
buff2				DS.B	 1
buff3				DS.B	 1
buff4				DS.B	 1
buff5				DS.B	 1
buff6				DS.B	 1
buff7				DS.B	 1
buff8				DS.B	 1								 ; end buffer
buffcount		DS.B	 1								 ; keeps count of buffers used to detect excess input
dig1				DS.B	 1								 ; used for clock input time
dig2				DS.B	 1
digT				DS.B	 1
retLED			DS.B	 1
retCalc			DS.B	 10
sign				DS.B	 1


msg1        DC.B   'Tcalc> ', $00
msg2				DC.B	 'Invalid input format',$00
msg3				DC.B	 'Early Enter',	$00

;*******************************************************
; interrupt vector section
            ORG    $FFF0             ; RTI interrupt vector setup for the simulator
;            ORG    $3FF0             ; RTI interrupt vector setup for the CSM-12C128 board
            DC.W   rtiisr

						ORG		 $FFEA
						DC.W	 ocisr

;*******************************************************
; code section

            ORG    $3100
Entry
            LDS    #Entry         ; initialize the stack pointer

            LDAA   #%11111111   ; Set PORTB bit 0,1,2,3,4,5,6,7
            STAA   DDRB         ; as output
            STAA   PORTB        ; set all bits of PORTB, initialize

						bset		DDRA, #%00000001	; Button switch SW0 at porta bit 0
						ldaa	 	#%00000001
						staa	 	PORTA

            ldaa   #$0C         ; Enable SCI port Tx and Rx units
            staa   SCICR2       ; disable SCI interrupts
						cli                     ; enable interrupt, global

            ldd    #$0002       ; Set SCI Baud Register = $0002 => 1M baud at 24MHz
;            ldd    #$000D       ; Set SCI Baud Register = $000D => 115200 baud at 24MHz
;            ldd    #$009C       ; Set SCI Baud Register = $009C => 9600 baud at 24MHz
            std    SCIBDH       ; SCI port baud rate change
            
            bset   RTICTL,%00011001 ; set RTI: dev=10*(2**10)=2.555msec for C128 board
                                    ;      4MHz quartz oscillator clock
            bset   CRGINT,%10000000 ; enable RTI interrupt
            bset   CRGFLG,%10000000 ; clear RTI IF (Interrupt Flag)

            ldx    #0
            stx    ctr2p5m          ; initialize interrupt counter with 0.


; ATD initialization
						ldaa			#%11000000		; Turn ON ADC, clear flags, Disable ATD interrupt
						staa			ATDCTL2				 
						ldaa			#%00001000		;	Single conversion per sequence, no FIFO	
						staa			ATDCTL3
						ldaa			#%10000111		;	8bit, ADCLK=24MHz/16=1.5MHz,. sampling time=2*(1/ADCLK)
						staa			ATDCTL4				; for SIMULATION

						ldx			#msg8
						jsr			printmsg
						jsr			nextline

main				
						ldx			#msg1
						jsr			printmsg
				
						ldx		 #0
						stx		 buffcount

						clr		 buff1
						clr		 buff2
						clr		 buff3
						clr		 buff4
						clr		 buff5
						clr		 buff6
						clr    buff7
						clr    buff8
          	CLR     num1
          	CLR     num2
          	CLR     num1count
          	CLR     num2count
          	CLR     opChar
						CLR		  rescount
						clr			sign

looop       jsr    LEDtoggle        ; if 0.5 second is up, toggle the LED 

            jsr    getchar          ; type writer - check the key board
            tsta                    ;  if nothing typed, keep checking
            beq    looop
                                    ;  otherwise - what is typed on key board
            jsr    putchar          ; is displayed on the terminal window
						ldab	 buffcount
						cmpb	 #0
						lbeq		 checkDataCap
contlooop
						inc		 buffcount				; increase buff count for inputed characters
						ldab	 buffcount
						cmpb	 #9
						bne		 lessThan9
						lbra	 Error1

lessThan9   
						cmpb	 #1
						beq		 loadbuff1
						cmpb	 #2
						beq		 loadbuff2
						cmpb	 #3
						beq		 loadbuff3
						cmpb	 #4
						beq	   loadbuff4
						cmpb	 #5
						beq		 loadbuff5
						cmpb	 #6
						beq		 loadbuff6
						cmpb	 #7
						beq		 loadbuff7
						cmpb	 #8
						beq	   loadbuff8
						bra		 looop

loadbuff1
						staa	 buff1
						cmpa   #CR
            lbeq    userRet            ; if Enter/Return key is pressed
						lbra		 looop
loadbuff2
						staa	 buff2
						cmpa   #CR
            lbeq    userRet            ; if Enter/Return key is pressed
						lbra		 looop
loadbuff3
						staa	 buff3
						cmpa   #CR
            lbeq    userRet            ; if Enter/Return key is pressed
						lbra		 looop
loadbuff4	
						staa	 buff4
						cmpa   #CR
            lbeq    userRet            ; if Enter/Return key is pressed
						lbra		 looop
loadbuff5
						staa	 buff5
						cmpa   #CR
            lbeq    userRet            ; if Enter/Return key is pressed
						lbra		 looop
loadbuff6
						staa	 buff6
						cmpa   #CR
            lbeq    userRet            ; if Enter/Return key is pressed
						lbra		 looop
loadbuff7
						staa	 buff7
						cmpa   #CR
            lbeq    userRet            ; if Enter/Return key is pressed
						lbra		 looop
loadbuff8	
						staa	 buff8
						cmpa   #CR
            lbeq    userRet            ; if Enter/Return key is pressed
						lbra		 looop
;*******************************************************
; subroutine section

;***********checkEnt*******************
; Check to see if user only hit Enter key
checkDataCap
						cmpa	 #CR
						beq		 startCap
						jsr		 contlooop	
startCap
						ldx			#msg6
						jsr			printmsg
						jsr			nextline
loop1
						jsr		 LEDtoggle
						ldaa	 PORTA
						anda	 #%00000001
						bne		 loop1
						jsr		 	tInter
loop2			
						jsr			LEDtoggle
						ldx			countusec
						cpx			#1024
						bne			loop2
						sei
						ldx			#0
						stx			countusec
						ldaa	 	#%00000001
						staa	 	PORTA
						lbra		main

;***********tInter*********************
; Timer interrupt start, output compare
tInter			pshd
						ldaa			#%00000100
						staa			TIOS
						staa			TIE
						ldaa			#%10010000
						staa			TSCR1
						ldaa			#%00000000
						staa			TSCR2

						ldd				#3000
						addd			TCNTH
						std				TC2H

						puld
						bset			TFLG1, %00000100
						cli
						rts
;***********end of tInter**************

;***********RTI interrupt service routine***************
rtiisr      bset   CRGFLG,%10000000 ; clear RTI Interrupt Flag - for the next one
						cli
            ldx    ctr2p5m          ; every time the RTI occur, increase
            inx                     ;    the 16bit interrupt count
            stx    ctr2p5m
rtidone     RTI
;***********end of RTI interrupt service routine********

;***********ocisr**********************
; timer OC interrupt service routine
ocisr 			jsr				go2ADC
						ldd				#3000
						addd			TC2H
						std				TC2H
						bset			TFLG1, #00000100
						cli
						ldx				countusec
						inx
						stx				countusec

ocdone			rti
;***********end of ocisr***************  

;***********single AD conversiton******
; This is a sample, non-interrupt, busy wait method
;
go2ADC
            PSHA                   ; Start ATD conversion
            LDAA  #%10000111       ; right justified, unsigned, single conversion,
            STAA  ATDCTL5          ; single channel, CHANNEL 7, start the conversion

adcwait     ldaa  ATDSTAT0         ; Wait until ATD conversion finish
            anda  #%10000000       ; check SCF bit, wait for ATD conversion to finish
            beq   adcwait

            ldaa  ATDDR0L          ; for SIMULATOR, pick up the lower 8bit result
            jsr   printDec         ; print the ATD result
            

            PULA
            RTS
;***********end of AD conversiton******

;***********userRet Routine***************
userRet
						ldx    #buff1
						ldaa	 x
						cmpa	 #CR
						lbeq	 EarlyEnter
						cmpa	 #$73
						lbeq	 sbranch
						cmpa	 #$40
						lblo   calcbranch
						cmpa	 #$71
						lbeq	 qbranch
						lbra	 Error1
;***********end of userRet Routine********

;***************LEDtoggle**********************
;* Program: toggle LED if 0.5 second is up
;* Input:   ctr2p5m variable
;* Output:  ctr2p5m variable and LED1
;* Registers modified: CCR
;* Algorithm:
;    Check for 0.5 second passed
;      if not 0.5 second yet, just pass
;      if 0.5 second has reached, then toggle LED and reset ctr2p5m
;**********************************************
LEDtoggle   psha
            pshx
						pshb

            ldx    ctr2p5m          ; check for 0.5 sec
;            cpx    #200             ; 2.5msec * 200 = 0.5 sec
            cpx    #40             ; 2.5msec * 200 = 0.5 sec
            blo    doneLED          ; NOT yet

            ldx    #0               ; 0.5sec is up,
            stx    ctr2p5m          ;     clear counter to restart

            ldaa   digT
						cmpa	 #$3C
						beq		 done60
						jsr		 setLED
						ldab	 retLED
            stab   PORTB
						inc    digT
						bra		 doneLED
done60
						ldx 	 #0
						stx 	 digT
						ldab	 #$00
						stab	 PORTB
						inc		 digT	

doneLED     pulx
            pula
						pulb
            rts
;***************end of LEDtoggle***************

;***********sbranch routine***************
sbranch
						ldx			#buff2
						ldaa		x						;	check second char to see if user inputed space
						cmpa		#CR
						lbeq		EarlyEnter
						cmpa		#SPA
						lbne		Error3
						ldab		buffcount
						cmpb		#4
						beq			second1
						cmpb		#5
						beq			second2
						lbra		Error3
second1
						ldx			#buff3			; load first digit and check to make sure it is a valid num
						ldaa		x
						cmpa		#$39
						lbhi		Error3
						cmpa		#$30
						lblo		Error3
						staa		dig1
						lbra		setClock
second2
						ldx			#buff3		  ; load first digit and check to make sure it is a valid num	
						ldaa		x
						cmpa		#$36
						lbhi		Error3
						cmpa		#$30
						lblo		Error3
						staa		dig1
						ldx			#buff4			; load second digit and check to make sure it is a valid num
						ldaa		x
						cmpa		#$39
						lbhi		Error3
						cmpa		#$30
						lblo		Error3
						staa		dig2
						lbra		setClock										
;***********end of sbranch routine********

;***********setClock routine***************
setClock
						ldx			#dig1							; convert user input to get time set
						ldaa		X
						suba		#$30
						staa		dig1
						ldab		buffcount
						cmpb		#4
						beq			setonly1

						ldx			#dig2
						ldaa		X
						suba		#$30
						staa		dig2

						ldaa		dig1
						ldab		#10
						mul			
						addb		dig2
						stab		digT
						jsr			LEDtoggle					; go to set display
						ldx			#msg1
						jsr 		printmsg
						clr			buffcount
						lbra		looop

setonly1
						ldaa		dig1
						staa		digT
						jsr			LEDtoggle					; go to set display
						ldx			#msg1
						jsr 		printmsg
						clr			buffcount
						lbra		looop
;***********end of setClock routine********

;***********setLED routine***************
setLED
						psha
						
						ldaa		digT							; checks for what should be displayed and finds and sets that ouput to the display
						cmpa		#$3B
						lbeq		set59
						cmpa		#$3A
						lbeq		set58
						cmpa		#$39
						lbeq		set57
						cmpa		#$38
						lbeq		set56
						cmpa		#$37
						lbeq		set55
						cmpa		#$36
						lbeq		set54
						cmpa		#$35
						lbeq		set53
						cmpa		#$34
						lbeq		set52
						cmpa		#$33
						lbeq		set51
						cmpa		#$32
						lbeq		set50
						cmpa		#$31
						lbeq		set49
						cmpa		#$30
						lbeq		set48
						cmpa		#$2F
						lbeq		set47
						cmpa		#$2E
						lbeq		set46
						cmpa		#$2D
						lbeq		set45
						cmpa		#$2C
						lbeq		set44
						cmpa		#$2B
						lbeq		set43
						cmpa		#$2A
						lbeq		set42
						cmpa		#$29
						lbeq		set41
						cmpa		#$28
						lbeq		set40
						cmpa		#$27
						lbeq		set39
						cmpa		#$26
						lbeq		set38
						cmpa		#$25
						lbeq		set37
						cmpa		#$24
						lbeq		set36
						cmpa		#$23
						lbeq		set35
						cmpa		#$22
						lbeq		set34
						cmpa		#$21
						lbeq		set33
						cmpa		#$20
						lbeq		set32
						cmpa		#$1F
						lbeq		set31
						cmpa		#$1E
						lbeq		set30
						cmpa		#$1D
						lbeq		set29
						cmpa		#$1C
						lbeq		set28
						cmpa		#$1B
						lbeq		set27
						cmpa		#$1A
						lbeq		set26
						cmpa		#$19
						lbeq		set25
						cmpa		#$18
						lbeq		set24
						cmpa		#$17
						lbeq		set23
						cmpa		#$16
						lbeq		set22
						cmpa		#$15
						lbeq		set21
						cmpa		#$14
						lbeq		set20
						cmpa		#$13
						lbeq		set19
						cmpa		#$12
						lbeq		set18
						cmpa		#$11
						lbeq		set17
						cmpa		#$10
						lbeq		set16
						cmpa		#$0F
						lbeq		set15
						cmpa		#$0E
						lbeq		set14
						cmpa		#$0D
						lbeq		set13
						cmpa		#$0C
						lbeq		set12
						cmpa		#$0B
						lbeq		set11
						cmpa		#$0A
						lbeq		set10
						cmpa		#$09
						lbeq		set9
						cmpa		#$08
						lbeq		set8
						cmpa		#$07
						lbeq		set7
						cmpa		#$06
						lbeq		set6
						cmpa		#$05
						lbeq		set5
						cmpa		#$04
						lbeq		set4
						cmpa		#$03
						lbeq		set3
						cmpa		#$02
						lbeq		set2
						cmpa		#$01
						lbeq		set1
						cmpa		#$00
						lbeq		set0

set59
						ldaa		#$59
						staa		retLED
						lbra		setDone
set58
						ldaa		#$58
						staa		retLED
						lbra		setDone
set57
						ldaa		#$57
						staa		retLED
						lbra		setDone
set56
						ldaa		#$56
						staa		retLED
						lbra		setDone
set55
						ldaa		#$55
						staa		retLED
						lbra		setDone
set54
						ldaa		#$54
						staa		retLED
						lbra		setDone
set53
						ldaa		#$53
						staa		retLED
						lbra		setDone
set52
						ldaa		#$52
						staa		retLED
						lbra		setDone
set51
						ldaa		#$51
						staa		retLED
						lbra		setDone
set50
						ldaa		#$50
						staa		retLED
						lbra		setDone
set49
						ldaa		#$49
						staa		retLED
						lbra		setDone
set48
						ldaa		#$48
						staa		retLED
						lbra		setDone
set47
						ldaa		#$47
						staa		retLED
						lbra		setDone
set46
						ldaa		#$46
						staa		retLED
						lbra		setDone
set45
						ldaa		#$45
						staa		retLED
						lbra		setDone
set44
						ldaa		#$44
						staa		retLED
						lbra		setDone
set43
						ldaa		#$43
						staa		retLED
						lbra		setDone
set42
						ldaa		#$42
						staa		retLED
						lbra		setDone
set41
						ldaa		#$41
						staa		retLED
						lbra		setDone
set40
						ldaa		#$40
						staa		retLED
						lbra		setDone
set39
						ldaa		#$39
						staa		retLED
						lbra		setDone
set38
						ldaa		#$38
						staa		retLED
						lbra		setDone
set37
						ldaa		#$37
						staa		retLED
						lbra		setDone
set36
						ldaa		#$36
						staa		retLED
						lbra		setDone
set35
						ldaa		#$35
						staa		retLED
						lbra		setDone
set34
						ldaa		#$34
						staa		retLED
						lbra		setDone
set33
						ldaa		#$33
						staa		retLED
						lbra		setDone
set32
						ldaa		#$32
						staa		retLED
						lbra		setDone
set31
						ldaa		#$31
						staa		retLED
						lbra		setDone
set30
						ldaa		#$30
						staa		retLED
						lbra		setDone
set29
						ldaa		#$29
						staa		retLED
						lbra		setDone
set28
						ldaa		#$28
						staa		retLED
						lbra		setDone
set27
						ldaa		#$27
						staa		retLED
						lbra		setDone
set26
						ldaa	  #$26
						staa		retLED
						lbra		setDone
set25
						ldaa		#$25
						staa		retLED
						lbra		setDone
set24
						ldaa		#$24
						staa		retLED
						lbra		setDone
set23
						ldaa		#$23
						staa		retLED
						lbra		setDone
set22
						ldaa		#$22
						staa		retLED
						lbra		setDone
set21
						ldaa		#$21
						staa		retLED
						lbra		setDone
set20
						ldaa		#$20
						staa		retLED
						lbra		setDone
set19
						ldaa		#$19
						staa		retLED
						lbra		setDone
set18
						ldaa		#$18
						staa		retLED
						lbra		setDone
set17
						ldaa		#$17
						staa		retLED
						lbra		setDone
set16
						ldaa		#$16
						staa		retLED
						lbra		setDone
set15
						ldaa		#$15
						staa		retLED
						lbra		setDone
set14
						ldaa		#$14
						staa		retLED
						lbra		setDone
set13
						ldaa		#$13
						staa		retLED
						lbra		setDone
set12
						ldaa		#$12
						staa		retLED
						lbra		setDone
set11
						ldaa		#$11
						staa		retLED
						lbra		setDone
set10
						ldaa		#$10
						staa		retLED
						lbra		setDone
set9
						ldaa		#$09
						staa		retLED
						lbra		setDone
set8
						ldaa		#$08
						staa		retLED
						lbra		setDone
set7
						ldaa		#$07
						staa		retLED
						lbra		setDone
set6
						ldaa		#$06
						staa		retLED
						lbra		setDone
set5
						ldaa		#$05
						staa		retLED
						lbra		setDone
set4
						ldaa		#$04
						staa		retLED
						lbra		setDone
set3
						ldaa		#$03
						staa		retLED
						lbra		setDone
set2
						ldaa		#$02
						staa		retLED
						lbra		setDone
set1
						ldaa		#$01
						staa		retLED
						lbra		setDone
set0
						ldaa		00
						staa		retLED
						lbra		setDone

setDone
						pula
						rts
						
						
;***********end of setLED routine********

;***********calcbranch routine***************
calcbranch
						ldx			#buff1
						ldaa		x
						staa		num1
						inc			num1count

						ldx			#buff2				; check second bit
						ldaa		x
						CMPA    	#CR        	; If 2nd digit of buffer is CR, then only one number was entered	ex: 1
          	LBEQ    	EarlyEnter  
          	CMPA    	#PLUS     	 
          	BNE     	MINUSCheck
          	STAA    	opChar
          	LBRA    	secondNum                                           	 
MINUSCheck	CMPA    	#MINUS
          	BNE     	MULTCheck 	 
          	STAA    	opChar
          	LBRA    	secondNum
MULTCheck 	CMPA    	#MULT
          	BNE     	DIVCheck
          	STAA    	opChar
          	LBRA    	secondNum
DIVCheck  	CMPA    	#DIV
          	BNE     	DIGCheck
          	STAA    	opChar
          	LBRA    	secondNum
DIGCheck		CMPA    	#$39
          	LBHI    	Error1     	; check if higher than 9
          	CMPA    	#$30
          	LBLO    	Error1     	; check if lower than 0
          	STAA    	num1+1
          	INC     	num1count

						ldx			#buff3				; check third bit
						ldaa		x
						CMPA    	#CR        	 
          	LBEQ    	EarlyEnter 	; ex: 12'CR'
          	CMPA    	#PLUS      	 
          	BNE     	MINUSCheck2
          	STAA    	opChar
          	BRA     	secondNum                                           	 
MINUSCheck2 CMPA    	#MINUS
          	BNE     	MULTCheck2 	 
          	STAA    	opChar
          	BRA     	secondNum
MULTCheck2	CMPA    	#MULT
          	BNE     	DIVCheck2
          	STAA    	opChar
          	BRA     	secondNum
DIVCheck2 	CMPA    	#DIV
          	BNE     	DIGCheck2    
          	STAA    	opChar
          	BRA     	secondNum  
   
DIGCheck2   CMPA    	#$39
          	LBHI    	Error1     	; check if higher than 9
          	CMPA    	#$30
          	LBLO    	Error1     	; check if lower than 0
          	STAA    	num1+2
          	INC     	num1count

						ldx			#buff4
						ldaa		x
						CMPA    	#CR     	 
          	LBEQ    	EarlyEnter
          	CMPA    	#PLUS      	 
          	BNE     	MINUSCheck3
          	STAA    	opChar
          	BRA     	secondNum   
                                                      	 
MINUSCheck3 CMPA    	#MINUS
          	BNE     	MULTCheck3 	 
          	STAA    	opChar
          	BRA     	secondNum
         	 
MULTCheck3	CMPA    	#MULT
          	BNE     	DIVCheck3
          	STAA    	opChar
          	BRA     	secondNum
         	 
DIVCheck3 	CMPA    	#DIV
           	LBNE    	Error4    	; if the fourth bit is not a character and no characters have been previously entered, then the input is invalid
          	STAA    	opChar
          	BRA     	secondNum
						
;***********end of calcbranch********

;***********secondNum routine***************
secondNum 	 
          	ldab    	num1count
						cmpb			#1
						lbeq				check1
						cmpb			#2
						lbeq				check2
						cmpb			#3
						lbeq				check3
          	 
check1
          	ldx				#buff3
						ldaa			x			
          	CMPA    	#CR            	; If first memory location in second num is CR, then the input is invalid EX: 123+
          	LBEQ    	EarlyEnter
          	CMPA    	#$39
          	LBHI    	Error2    	; check if higher than 9
          	CMPA    	#$30
          	LBLO    	Error2    	; check if lower than 0
          	STAA    	num2
          	INC     	num2count

          	ldx				#buff4
						ldaa			x
          	CMPA    	#CR        	 
          	LBEQ    	Conversion      	; ex: 123+1 or 1+1
          	CMPA    	#$39
          	LBHI    	Error2     	; check if higher than 9
          	CMPA    	#$30
          	LBLO    	Error2     	; check if lower than 0
          	STAA    	num2+1 
          	INC     	num2count
         	 
						ldx				#buff5
						ldaa			x             	 
          	CMPA    	#CR        	 
          	LBEQ     	Conversion
          	CMPA    	#$39
          	LBHI    	Error2     	; check if higher than 9
          	CMPA    	#$30
          	LBLO    	Error2     	; check if lower than 0
          	STAA    	num2+2
          	INC     	num2count
						lbra			Conversion

check2
          	ldx				#buff4
						ldaa			x			
          	CMPA    	#CR            	; If first memory location in second num is CR, then the input is invalid EX: 123+
          	LBEQ    	EarlyEnter
          	CMPA    	#$39
          	LBHI    	Error2    	; check if higher than 9
          	CMPA    	#$30
          	LBLO    	Error2    	; check if lower than 0
          	STAA    	num2
          	INC     	num2count

          	ldx				#buff5
						ldaa			x
          	CMPA    	#CR        	 
          	LBEQ    	Conversion      	; ex: 123+1 or 1+1
          	CMPA    	#$39
          	LBHI    	Error2     	; check if higher than 9
          	CMPA    	#$30
          	LBLO    	Error2     	; check if lower than 0
          	STAA    	num2+1 
          	INC     	num2count
         	 
						ldx				#buff6
						ldaa			x             	 
          	CMPA    	#CR        	 
          	LBEQ     	Conversion
          	CMPA    	#$39
          	LBHI    	Error2     	; check if higher than 9
          	CMPA    	#$30
          	LBLO    	Error2     	; check if lower than 0
          	STAA    	num2+2
          	INC     	num2count
						lbra			Conversion

check3
          	ldx				#buff5
						ldaa			x			
          	CMPA    	#CR            	; If first memory location in second num is CR, then the input is invalid EX: 123+
          	LBEQ    	EarlyEnter
          	CMPA    	#$39
          	LBHI    	Error2    	; check if higher than 9
          	CMPA    	#$30
          	LBLO    	Error2    	; check if lower than 0
          	STAA    	num2
          	INC     	num2count

          	ldx				#buff6
						ldaa			x
          	CMPA    	#CR        	 
          	LBEQ    	Conversion      	; ex: 123+1 or 1+1
          	CMPA    	#$39
          	LBHI    	Error2     	; check if higher than 9
          	CMPA    	#$30
          	LBLO    	Error2     	; check if lower than 0
          	STAA    	num2+1 
          	INC     	num2count
         	 
						ldx				#buff7
						ldaa			x             	 
          	CMPA    	#CR        	 
          	LBEQ     	Conversion
          	CMPA    	#$39
          	LBHI    	Error2     	; check if higher than 9
          	CMPA    	#$30
          	LBLO    	Error2     	; check if lower than 0
          	STAA    	num2+2
          	INC     	num2count
						lbra			Conversion
         	 
;****************************************************************************
;* Converting from character to number
;****************************************************************************
     	 
Conversion
          	LDX     	#num1
          	LDAB    	num1count 

num1c     	
						LDAA			X   	 
          	SUBA    	#$30
          	STAA    	1,X+
          	DECB
          	CMPB    	#0
          	BEQ     	here
						BRA				num1c
         	 
here      	LDX     	#num2
          	LDAB    	num2count

         	 
num2c    	   	 
 						LDAA			X
          	SUBA    	#$30
          	STAA    	1,X+
          	DECB   	 
          	CMPB    	#0
          	BEQ     	num1makeSum
          	BRA     	num2c

;****************************************************************************
;* Converting from number to character
;****************************************************************************

Conversion2
						LDX				#num1
						LDAB			num1count
num1c1
						LDAA			X   	 
          	ADDA    	#$30
          	STAA    	1,X+
          	DECB
          	CMPB    	#0
          	BEQ     	here1
						BRA				num1c1
         	 
here1      	LDX     	#num2
          	LDAB    	num2count

         	 
num2c2    	     	 
 						LDAA			X
          	ADDA    	#$30
          	STAA    	1,X+
          	DECB   	 
          	CMPB    	#0
						BEQ				result_
						BRA				num2c2

result_
						LDAA			#0
						STAA			rescount
						LDY				#retCalc
result_1
						LDD				result
						LDX				#10
						IDIV			
						STX				result
						ADDD			#$30
						STD			  2,Y+
						INC				rescount
						cpx			  #0
						beq				done_
						BRA				result_1
done_
						RTS     	          	 
;*********************************************************************************
;* Converting from discrete digits in num1/num2 to a single number ex 3|2|1 -> 321
;*********************************************************************************
          	 
                        	 
num1makeSum         	 
          	LDAB    	num1count
          	CMPB    	#3                  	; check if num1 has 3 numbers
          	BEQ     	num1Has3
          	CMPB    	#2
          	BEQ     	num1Has2
          	CMPB    	#1
          	BEQ     	num1Has1
          	LDAA    	num1count
          	JSR     	nextline
          	LDAA    	num1count        	; test code
          	JSR     	putchar
          	JSR     	nextline
           	LBRA    	Error5
                                            	 
num1Has3
          	LDAA    	num1
          	LDAB    	#100
          	MUL                             	; 100*first digit of num1 ----> B
          	STAB    	num1sum            	; put 100*first digit of num1 into the byte designated by NUM1SUM
          	LDAA    	num1+1
          	LDAB    	#10
          	MUL                             	; 10*second digit of num1 ----> B
          	ADDB    	num1sum             	; 100*first digit of num1 + 10*second digit of num1 ----> B
          	ADDB    	num1+2              	; no need to load 1 into B for MUL, just directly add the third digit
          	STAB    	num1sum
						cmpb			#$0
						lbeq			Overflow           	 
          	BRA     	num2makeSum

num1Has2
						LDAA			num1
          	LDAB    	#10
          	MUL                             	; 10*first digit of num1 ----> B         	 
          	ADDB    	num1+1         	 
          	STAB    	num1sum            	 
          	BRA     	num2makeSum
         	 


num1Has1  	LDAA    	num1
          	STAA    	num1sum
          	BRA     	num2makeSum
 
          	 

num2makeSum
          	LDAB    	num2count  
          	CMPB    	#3                  	; check if num1 has 3 numbers
          	BEQ     	num2Has3
          	CMPB    	#2
          	BEQ     	num2Has2
          	CMPB    	#1
          	BEQ     	num2Has1
                                            	 
num2Has3
          	LDAA    	num2
          	LDAB    	#100
          	MUL                             	; 100*first digit of num2 ----> B
          	STAB    	num2sum             	; put 100*first digit of num1 into the byte designated by NUM1SUM
          	LDAA    	num2+1
          	LDAB    	#10
          	MUL                             	; 10*second digit of num2 ----> B
          	ADDB    	num2sum             	; 100*first digit of num2 + 10*second digit of num2 ----> B
          	ADDB    	num2+2              	; no need to load 1 into B for MUL, just directly add the third digit
          	STAB    	num2sum
						cmpb			#$0
						lbeq			Overflow           	 
          	BRA     	Operation

num2Has2
						LDAA			num2
          	LDAB    	#10
          	MUL                             	; 10*first digit of num2 ----> B         	 
          	ADDB    	num2+1         	 
          	STAB    	num2sum            	 
          	BRA     	Operation
         	 
num2Has1  	LDAA    	num2
          	STAA    	num2sum
          	BRA     	Operation       	 
       	 
           	 
;****************************************************************************
Operation              	 
          	LDAA     	opChar  
          	CMPA     	#PLUS
          	BNE      	subtract
          	BRA      	addFunct
subtract 	 
          	CMPA     	#MINUS
          	BNE      	multiply        	 
          	BRA      	subFunct
multiply 	 
          	CMPA     	#MULT
          	BNE      	divide
          	BRA      	mulFunct
divide   	 
          	CMPA     	#DIV
          	LBNE      Error1
          	BRA      	divFunct	 
   
addFunct  	LDAA     	num1sum
          	ADDA      num2sum
						cmpa			num1sum
						lblo			Overflow
						cmpa			num2sum
						lblo			Overflow
          	STAA      result+1
						JSR				Conversion2
						BRA				Finish

subFunct
						LDAA			num1sum
						SUBA			num2sum
						STAA			result+1
						JSR				Conversion2
						BRA				Finish

mulFunct
						LDAA			num1sum
						LDAB			num2sum
						MUL
						STD				result
						ldaa			result+1
						cmpa			num1sum
						lblo			Overflow
						cmpa			num2sum
						lblo			Overflow
						cmpa			#$FF
						lbeq			Overflow
						JSR				Conversion2
						BRA				Finish

divFunct
						LDD				num1sum
						LDX				num2sum
						IDIV
						STX			result
						JSR				Conversion2
						BRA				Finish   

Finish
						LDAA			#CR
						JSR				putchar
						LDAA			#LF
						JSR				putchar
						LDAA			#$20
						JSR				putchar
						LDX				#num1
						LDAB			num1count
finish1_1
						LDAA			1,X+   	 
          	JSR				putchar
          	DECB
          	CMPB    	#0
          	BEQ     	finNext
						BRA				finish1_1
         	 
finNext     
						LDAA			opChar
						JSR				putchar
						LDX     	#num2
          	LDAB    	num2count
    	 
finish2_1    	     	 
 						LDAA			1,X+
						JSR				putchar
          	DECB   	 
          	CMPB    	#0
						BEQ				finDone
						BRA				finish2_1
finDone
						LDAA			#$3D
						JSR				putchar
finPrint
						ldab			rescount
						cmpb			#5
						lbeq			Overflow
						cmpb			#4
						lbeq			fin4
						cmpb			#3
						lbeq			fin3
						cmpb			#2
						lbeq			fin2
						cmpb			#1
						lbeq			fin1
 						lbra			Error5

fin4
						LDAA			retCalc+7
						JSR 			putchar
						LDAA			retCalc+5
						JSR 			putchar
						LDAA			retCalc+3
						JSR 			putchar
						LDAA			retCalc+1
						JSR 			putchar
						LDAA			#LF
						JSR				putchar
						LBRA			main
fin3
						LDAA			retCalc+5
						JSR 			putchar
						LDAA			retCalc+3
						JSR 			putchar
						LDAA			retCalc+1
						JSR 			putchar
						LDAA			#LF
						JSR				putchar
						LBRA			main
fin2
						LDAA			retCalc+3
						JSR 			putchar
						LDAA			retCalc+1
						JSR 			putchar
						LDAA			#LF
						JSR				putchar
						LBRA			main
fin1
						LDAA			retCalc+1
						JSR 			putchar
						LDAA			#LF
						JSR				putchar
						LBRA			main
 
;***********end of secondNum********

;***********qbranch routine***************
qbranch			
						ldx		#msg4
						jsr		nextline
						jsr		printmsg
						jsr   nextline
typewriter			
						LDAA				#%11111111	; Set PORTB bit 0,1,2,3,4,5,6,7
						STAA 				DDRB				;		as output

						JSR					getchar			; typewriter - check the keyboard
						CMPA				#$00				;		if nothing typed, keep checking
						beq					typewriter
																		; Otherwise - what is typed on the keyboard
						JSR					putchar			;		is displayed on the terminal window - echo print

						STAA				PORTB				; show the character on PORTB

						CMPA				#CR					
						BNE					typewriter	; if Enter/Return key is pressed, move the
						LDAA				#LF					; 	cursor to the next line
						JSR					putchar
						BRA				  typewriter	;
;***********end of qbranch routine********

;****************Error1**********************
Error1
						ldx		#msg2
						jsr		nextline
						jsr		printmsg
						jsr   nextline
	
						lbra	main
;****************end of Error1***************

;****************Error2*****************
;* Program: Displays error for not inputting
;*      	a digit  
;*     	 
;* Register in use: X for printing message
Error2	 
           	LDX   	#mustBeDigit
           	JSR   	nextline
           	JSR   	printmsg
           	JSR   	nextline
           	
						lbra	main   
;****************endError2**************

;****************Error3**********************
Error3
						ldx		#msg5
						jsr		nextline
						jsr		printmsg
						jsr   nextline
	
						lbra	main
;****************end of Error3***************

;****************EarlyEnter**********************
EarlyEnter
						ldx		#msg3
						jsr		nextline
						jsr		printmsg
						jsr   nextline
	
						lbra	main
;****************end of EarlyEnter***************    

;****************Error4*************
;* Program: Displays an error for not inputting
;*      	an operator at a valid point
Error4
          	LDX   	#missingChar
          	JSR   	nextline
          	JSR   	printmsg
          	JSR   	nextline
          
						lbra	  main
;**********************************************  	 

;****************displayMadeItToHere*************
;* Program: Displays an error for not inputting
;*      	an operator at a valid point
Error5
          	LDX   	#miscError
          	JSR   	nextline
          	JSR   	printmsg
          	JSR   	nextline
          	
						lbra	  main
;**********************************************

;****************displayOverflowError**********
;* Program: Displays error in the case of
;*      	overflow
Overflow
          	LDX   	#OVError
          	JSR   	nextline
          	JSR   	printmsg
          	JSR   	nextline
          	
						lbra		main
;**********************************************	 
                  
;***********printDec*******************
printDec		pshd
						pshx
						pshy
						clr		ret
						clr		buff
						staa	buff+1
						ldd		buff
						ldy		#buff

printDec1		ldx		#10
						idiv
						beq		printDec2
						stab	1, y+
						inc 	ret
						tfr		x, d
						bra		printDec1

printDec2		stab	y
						inc   ret
;------------------------------------------------------
printDec3		ldaa	#$30
						adda	1, y-
						jsr		putchar
						dec		ret
						bne		printDec3
						jsr		nextline

						puly
						pulx
						puld
						
						rts						
;***********end of printDec************

;***********printmsg***************************
;* Program: Output character string to SCI port, print message
;* Input:   Register X points to ASCII characters in memory
;* Output:  message printed on the terminal connected to SCI port
;* C
;* Registers modified: CCR
;* Algorithm:
;     Pick up 1 byte from memory where X register is pointing
;     Send it out to SCI port
;     Update X register to point to the next byte
;     Repeat until the byte data $00 is encountered
;       (String is terminated with NULL=$00)
;**********************************************
NULL            equ     $00
printmsg        psha                   ;Save registers
                pshx
printmsgloop    ldaa    1,X+           ;pick up an ASCII character from string
                                       ;   pointed by X register
                                       ;then update the X register to point to
                                       ;   the next byte
                cmpa    #NULL
                beq     printmsgdone   ;end of strint yet?
                jsr     putchar        ;if not, print character and do next
                bra     printmsgloop
printmsgdone    pulx 
                pula
                rts
;***********end of printmsg********************

;***************putchar************************
;* Program: Send one character to SCI port, terminal
;* Input:   Accumulator A contains an ASCII character, 8bit
;* Output:  Send one character to SCI port, terminal
;* Registers modified: CCR
;* Algorithm:
;    Wait for transmit buffer become empty
;      Transmit buffer empty is indicated by TDRE bit
;      TDRE = 1 : empty - Transmit Data Register Empty, ready to transmit
;      TDRE = 0 : not empty, transmission in progress
;**********************************************
putchar     brclr SCISR1,#%10000000,putchar   ; wait for transmit buffer empty
            staa  SCIDRL                       ; send a character
            rts
;***************end of putchar*****************

;****************getchar***********************
;* Program: Input one character from SCI port (terminal/keyboard)
;*             if a character is received, other wise return NULL
;* Input:   none    
;* Output:  Accumulator A containing the received ASCII character
;*          if a character is received.
;*          Otherwise Accumulator A will contain a NULL character, $00.
;* Registers modified: CCR
;* Algorithm:
;    Check for receive buffer become full
;      Receive buffer full is indicated by RDRF bit
;      RDRF = 1 : full - Receive Data Register Full, 1 byte received
;      RDRF = 0 : not full, 0 byte received
;**********************************************

getchar     brclr SCISR1,#%00100000,getchar7
            ldaa  SCIDRL
            rts
getchar7    clra
            rts
;****************end of getchar**************** 

;****************nextline**********************
nextline    ldaa  #CR              ; move the cursor to beginning of the line
            jsr   putchar          ;   Cariage Return/Enter key
            ldaa  #LF              ; move the cursor to next line, Line Feed
            jsr   putchar
            rts
;****************end of nextline***************
msg4				DC.B	 'Stop clock and calculator, start Typewriter program', $00
msg5				DC.B	 'Invalid time format. Correct example => 0 to 59',$00
msg6				DC.B	 'When ready, hit SW0 to start the 1024 point ADC data capture', $00
msg7				DC.B	 'To continue with Calculator or to run data capture again chang SW0 to off', $00
msg8				DC.B	 'To run ADC data capture hit enter with no other input', $00
testError  	DC.B  	'Error: test', $00
mustBeDigit	DC.B  	'Non-Digit Input', $00
missingChar	DC.B  	'No Operation Character', $00
miscError  	DC.B  	'Miscellaneous Error', $00
OVError    	DC.B  	'OVERFLOW', $00

            END               ; this is end of assembly source file
                              ; lines below are ignored - not assembled/compiled;***************putchar************************
						