#include <msp430.h>
#include "RGB_LED.h"
#include "intrinsics.h"
#include <math.h>




int arrayCounter = 0;
//int keypadInput[16] = {0};  // initialize all elements to 0
int col1Input = 0;
//int row1Input = 0;
int col2Input = 0;
int col3Input = 0;
int col4Input = 0;
//bool testHigh = false;
char button_input;
int passcodeInput[] = {0, 0, 0, 0};
int inputOrder = 0;
char passkey[] = {'1', '3', 'C', '0'};
char user_input[4] = {'0'};
int locked = 0;


int portflag = 0;


int upperBits[8];
int lowerBits[8];
int binTemp[16];




float v;
float v0;
float numerator;
float divider;
float adder;
double square;
int adc_val = 0;
float rolling_temp_average = 0.0;
int totalTempsRead = 0;
float average_temp = 0;
float voltage = 0.0;
double base;
double exponent = .5;
float temp;




int data_cnt = 0;
//char led_packet[] = {0x69, '1'};
char led_packet[] = {'1','4','4',};
//               slave addr, data to send


unsigned char tx_data[4] = { '1', '3', 0xD4 }; // '1', '3', 0x00D4 = 212 (21.2°C)
unsigned char tx_index = 0;




void Keypad_init(void)
{
 
 
   // --- Setup Keypad Ports ---
   // Configure P3.4-P3.7 as inputs (for reading keypad rows)
   P1DIR |= BIT0; // setup LED
   P3DIR &= ~(BIT4 | BIT5 | BIT6 | BIT7); // setup for input
   P3OUT &= ~(BIT0 | BIT4 | BIT5 | BIT6 | BIT7); // start with all rows low
   P3IES &= ~(BIT4 | BIT5 | BIT6 | BIT7);
   P3IFG &= ~(BIT4 | BIT5 | BIT6 | BIT7);
   P3IE |= (BIT4 | BIT5 | BIT6 | BIT7);
 
   // Configure P5.0-P5.3 as outputs (for driving keypad columns)
   P5DIR |= (BIT0 | BIT1 | BIT2 | BIT3);
   P5REN &= ~(BIT0 | BIT1 | BIT2 | BIT3);
   P5OUT &= ~(BIT0 | BIT1 | BIT2 | BIT3);




   P6DIR |= BIT0;      // status indicator
   P6OUT &= ~BIT0;
 
   // --- Setup Timer B0 ---
   TB0CTL = TBCLR;                      // clear timer
   TB0CTL |= TBSSEL__SMCLK | MC__UP;      // use SMCLK in Up mode
   TB0CTL |= ID__4;                     // Divider /4
   TB0EX0 |= TBIDEX__2;                 // Extended divider /2 (total division factor = 8)
   TB0CCR0 = 9356;                      // Timer period (adjust as needed) PREV 4678 -> changed to double for better reads
 
   // Enable CCR0 interrupt
   TB0CCTL0 &= ~CCIFG;                  // Clear flag
   TB0CCTL0 |= CCIE;                    // Enable interrupt
 
}




void isLocked(){
   locked = 1;
   int i;
   for (i = 0; i < 3; i++) {
       passcodeInput[i] = 0;
   }
}




void unlocked(){
   locked =0;
}




void Passkey_func(char input){




   if(locked == 1){
     
   }else if(input == 'D'){
       isLocked();
   }
   //choose_pattern(input);








}




void initI2C() {
// 1 - Put eUSCI_B0 into software reset
   UCB0CTLW0 |= UCSWRST;           // UCSWRST = 1 for eUSCI_B0 in SW reset




   // 2 - Configure eUSCI_B0
   UCB0CTLW0 |= UCSSEL__SMCLK;     // chose BRCLK = SMCLK = 1 MHz
   UCB0BRW = 10;                   // divide BRCLK by 10 for SCL = 100 kHz




   UCB0CTLW0 |= UCMODE_3;          // put into I2C mode
   UCB0CTLW0 |= UCMST;             // put into master mode
   UCB0CTLW0 |= UCTR;              // put into Tx mode
   UCB0I2CSA = 0x48;             // slave address = 0x69




   UCB0CTLW1 |= UCASTP_2;          // auto STOP when UCB0TBCNT reached
   UCB0TBCNT = sizeof(led_packet);     // # of bytes in packet




   // 3 - configure ports
   P1SEL1 &= ~BIT3;
   P1SEL0 |= BIT3;                 // want P1.3 = SCL
   P1REN |= BIT3;
   P1OUT |= BIT3;




   P1SEL1 &= ~BIT2;
   P1SEL0 |= BIT2;                 // want P1.2 = SDA
   P1REN |= BIT2;
   P1OUT |= BIT2;




   // 4 - take eUSCI_B0 out of SW reset
   UCB0CTLW0 &= ~UCSWRST;          // UCSWRST = 1 for eUSCI_B0 in SW reset




   // 5 - enable interrupts
   UCB0IE |= UCTXIE0;              // enable I2C Tx0 IRQ
   //__enable_interrupt();           // enable global interrupts
}


void adc_init() {




   ADCCTL0 &= ~ADCSHT;         // clear ADCSHT from def. of ADCSHT = 01
   ADCCTL0 |= ADCSHT_2;        // conversion cycles = 16 (ADCSHT = 10)
   ADCCTL0 |= ADCON;           // turn ADC on




   ADCCTL1 |= ADCSSEL_2;       // ADC clock source = SMCLK
   ADCCTL1 |= ADCSHP;          // sample signal source = sampling timer




   ADCCTL2 &= ~ADCRES;         // clear ADCRES from def. of ADCRES = 01
   ADCCTL2 |= ADCRES_2;        // resolution = 12 bits (ADCRES = 10)
 
 
   ADCMCTL0 |= ADCINCH_1;      // in channel = A1 (P1.1)




   ADCIE |= ADCIE0;            // enable ADC conversion complete IRQ




}








// ADC interrupt vector for reading a temperature sensor
// keeps the rolling values of all readings to then get the average temp
#pragma vector=ADC_VECTOR
__interrupt void ADC_ISR(void) {
   __bic_SR_register_on_exit(LPM0_bits);
   // ~1980 is room temp (~21 deg C)
   int i;
   adc_val = ADCMEM0;
   voltage = adc_val * .00083;
   //v0 = (-.01169 * voltage) + 1.8663;
   totalTempsRead++;
   int avTemp;
   if (totalTempsRead >= 3) {
       //rolling_temp_average += adc_val;      
       //average_temp = rolling_temp_average / (totalTempsRead - 3);
       numerator = 1.8639 - voltage;
       divider = 3.88 * .00001;
       adder = 2.1962 * 1000000;
       base = (numerator / divider) + adder;
       square = pow(base, exponent);
       temp = -1481.96 + square;
       temp = temp * 10;
       rolling_temp_average += temp;
       average_temp = rolling_temp_average / (totalTempsRead - 2);
       average_temp *= 10;
       avTemp = (int) average_temp;
       tx_data[2] = avTemp;
   }
 
}




int main(void)
{
   // Stop watchdog timer
   
   WDTCTL = WDTPW | WDTHOLD;
 
 
 
   Keypad_init();
   //Initialize_LEDBAR();
   initPWM();
   //setColorLocked();
   initI2C();
   adc_init();




   PM5CTL0 &= ~LOCKLPM5;                   // Disable the GPIO power-on default high-impedance mode
                                           // to activate previously configured port settings
   __enable_interrupt();
   // Main loop enters low-power mode; all processing is in the ISR.
   int i;
   while(1){
       int j;
       if(portflag == 1 && locked == 0){

            UCB0I2CSA = 0x69;             // slave address = 0x69
            UCB0TBCNT = 1;               // # of bytes in packet
            UCB0CTLW0 |= UCTXSTT;       // generate start condition
            while (UCB0CTLW0 & UCTXSTP);  // Wait for stop condition
            UCB0IFG &= ~UCSTPIFG;   // clear STOP flag
           
            __delay_cycles(5000);

            UCB0I2CSA = 0x48;             // slave address = 0x69
            UCB0TBCNT = sizeof(led_packet);     // # of bytes in packet
            UCB0CTLW0 |= UCTXSTT;       // generate start condition
            while (UCB0CTLW0 & UCTXSTP);  // Wait for stop condition
            UCB0IFG &= ~UCSTPIFG;   // clear STOP flag
            portflag = 0;
       }
       
   }
}








// I2C interrupt for sending a single packet to the LED bar
#pragma vector=EUSCI_B0_VECTOR
__interrupt void EUSCI_B0_I2C_ISR(void) {
   int j;


    if(UCB0I2CSA == 0x48){
        if(data_cnt == (sizeof(tx_data) - 1)) {
            UCB0TXBUF = tx_data[data_cnt];
            data_cnt = 0;
        } else {
            UCB0TXBUF = tx_data[data_cnt];
            data_cnt++;
        }
    }else if(UCB0I2CSA == 0x69){
        UCB0TXBUF = led_packet[0];
    }
   //UCB0TXBUF = 0;
}




// Keypad interrupt
#pragma vector = TIMER0_B0_VECTOR
__interrupt void ISR_TB0_CCR0(void)
{
  // Static counter to toggle LED only every ~0.5s.
  // Based on a SMCLK = 1MHz, divided by 8 (from ID__4 and TBIDEX__2),
  // the timer clock is 125kHz and the period is ~4679/125000 ≈ 0.03743s.
  // About 14 interrupts yield ~0.524s.
  static unsigned int toggleCount = 0;
  // --- Keypad Scanning via Switch-Case ---
  if (passcodeInput[0] != 0 && passcodeInput[1] != 0 && passcodeInput[2] != 0 && passcodeInput[3] != 0 && inputOrder != 0) {
      unlocked();
  }








  switch(arrayCounter)
  {
      case 0:
          // Activate col 1 (P5.0) and clear other rows.
          P5OUT |= BIT0;
          P5OUT &= ~(BIT1 | BIT2 | BIT3);
          arrayCounter++;
          //col4Input = P5IN;  // Sample keypad column inputs
          break;
       
      case 1:
          // Activate col 2 (P5.1)
          P5OUT |= BIT1;
          P5OUT &= ~(BIT0 | BIT2 | BIT3);
          //col3Input = P5IN;
          arrayCounter++;
          break;
       
      case 2:
          // Activate col 3 (P5.2)
          P5OUT |= BIT2;
          P5OUT &= ~(BIT0 | BIT1 | BIT3);
          //col2Input = P5IN;
          arrayCounter++;
          break;
       
      case 3:
          // Activate col 4 (P5.3)
          P5OUT |= BIT3;
          P5OUT &= ~(BIT0 | BIT1 | BIT2);
          //col1Input = P5IN;
          arrayCounter = 0;
          break;
       
      default:
          break;
  }
  // Update the row counter, wrapping from 3 back to 0.
  // Increment toggleCount and toggle LED (P1.0) every ~0.5 seconds.
  toggleCount++;
  if(toggleCount >= 14)
  {
    ADCCTL0 |= ADCENC | ADCSC;   // enable and start conversion
   __bis_SR_register(GIE | LPM0_bits);      // enable maskable IRQ
      P1OUT ^= BIT0;  // Toggle LED on P1.0
      toggleCount = 0;
  }
  // Clear CCR0 interrupt flag
  TB0CCTL0 &= ~CCIFG;
}








#pragma vector=PORT3_VECTOR
__interrupt void port3_interrupt_ISR(void) {
 int t;
  switch(arrayCounter) {
      case 1:
          // Active is col 1 (P5.0)
       
          col4Input = P3IN & 0b11110000;  // Sample keypad column inputs
          switch(col4Input){
              case 0b00010000:
                  button_input = "*";
                  P6OUT ^= BIT6;
                  //passcodeInput[3] = 1;
                  if (locked == 0) {
                      setColorUnlocked();
                  }
                  if (locked == 0) {
                      led_packet[0] = '*';
                  }
                  break;
              case 0b00100000:
                  button_input = "7";
                  P6OUT ^= BIT6;
                  if (locked == 0) {
                      led_packet[0] = '7';
                  }
                  break;
              case 0b01000000:
                  button_input = "4";
                  P6OUT ^= BIT6;
                  if (locked == 0) {
                      led_packet[0] = '4';
                  }
                  break;
              case 0b10000000:
                  button_input = "1";
                  P6OUT ^= BIT6;
                  setCustomColor(0, 0, 0);
                  if (inputOrder == 0) {
                      passcodeInput[0] = 1;
                      inputOrder++;
                  }
               
                  //setColorUnlocking();
                  if (locked == 0) {
                      led_packet[0] = '1';
                      portflag = 1;
                  }
                  break;
              default:
                  break;
          };
      case 2:
          // Active is col 2 (P5.1)
          col3Input = P3IN & 0b11110000;
          switch(col3Input){
              case 0b00010000:
                  button_input = "0";
                  P6OUT ^= BIT6;
                  if (inputOrder == 3) {
                      passcodeInput[3] = 1;
                      setColorUnlocked();
                  }
               
                  if (locked == 0) {
                      led_packet[0] = '0';
                  }
                  break;
              case 0b00100000:
                  button_input = "8";
                  P6OUT ^= BIT6;
                  if (locked == 0) {
                      led_packet[0] = '8';
                  }
                  break;
              case 0b01000000:
                  button_input = "5";
                  P6OUT ^= BIT6;
                  if (locked == 0) {
                      led_packet[0] = '5';
                      portflag = 1;
                  }
                  break;
              case 0b10000000:
                  button_input = "2";
                  P6OUT ^= BIT6;
                  setCustomColor(255, 255, 255);
                  if (locked == 0) {
                      led_packet[0] = '2';
                      portflag = 1;
                  }
                  break;
            default:
                  break;
          };
         break;
      case 3:
         // Active is col 3 (P5.2)
         col2Input = P3IN & 0b11110000;
         switch(col2Input){
             case 0b00010000:
                 button_input = "#";
                 P6OUT ^= BIT6;
                 if (locked == 0) {
                     led_packet[0] = '#';
                 }
                 break;
             case 0b00100000:
                 button_input = "9";
                 P6OUT ^= BIT6;
                 if (locked == 0) {
                     led_packet[0] = '9';
                 }
                 break;
             case 0b01000000:
                 button_input = "6";
                 P6OUT ^= BIT6;
                 if (locked == 0) {
                     led_packet[0] = '6';
                     portflag = 1;
                 }
                 break;
             case 0b10000000:
                 button_input = "3";
                 P6OUT ^= BIT6;
                 setCustomColor(170, 170, 170);
                 if (inputOrder == 1) {
                  passcodeInput[1] = 1;
                  inputOrder++;
                 }
                 if (locked == 0) {
                     led_packet[0] = '3';
                     portflag = 1;
                 }
                 break;
            default:
                  break;
         };
         break;
      case 0:
         // Active is col 4 (P5.3)
         col1Input = P3IN & 0b11110000;
         switch(col1Input){
             case 0b00010000:
                 //setColorLocked();
                 button_input = "D";
                 P6OUT ^= BIT6;
                 int k;
                 //for (k = 0; k < 4; k++) {
                 // passcodeInput[k] = 0;
                 //}
                 //isLocked();
                 if (locked == 0) {
                     led_packet[0] = 'D';
                     portflag = 1;
                 }
                 break;
             case 0b00100000:
                 button_input = "C";
                 P6OUT ^= BIT6;
                 if (inputOrder == 2) {
                  passcodeInput[2] = 1;
                  inputOrder++;
                 }
               
                 if (locked == 0) {
                     led_packet[0] = 'C';
                     portflag = 1;
                 }
                 break;
             case 0b01000000:
                 button_input = "B";
                 P6OUT ^= BIT6;
                 if (locked == 0) {
                     led_packet[0] = 'B';
                     portflag = 1;
                 }
                 break;
             case 0b10000000:
                 button_input = "A";
                 P6OUT ^= BIT6;
                 if (locked == 0) {
                     led_packet[0] = 'A';
                     portflag = 1;
                 }
                 break;
            default:
                  break;
         };
         break;








  };
 //__delay_cycles(5000);
  P3IN &= ~(BIT4 | BIT5 | BIT6 | BIT7);
  P3IFG &= ~(BIT4 | BIT5 | BIT6 | BIT7);


}


