#include <msp430.h>
#include <string.h>

// Define bit masks for LED and LCD pins
#define LED         BIT0          // Heartbeat LED on P2.0

// LCD data lines on P1.4, P1.5, P1.6, and P1.7
#define LCD_D4      BIT4
#define LCD_D5      BIT5
#define LCD_D6      BIT6
#define LCD_D7      BIT7
#define LCD_DATA    (LCD_D4 | LCD_D5 | LCD_D6 | LCD_D7)

// LCD control pins on P2.6 (RS) and P2.7 (Enable)
#define LCD_RS      BIT6          // P2.6
#define LCD_E       BIT7          // P2.7

// Global variables for I2C input

volatile char button = 'B';
volatile char window = '3';
volatile char temp[8] = "21.2";
volatile int temp_raw = 212; // Raw temperature in tenths of °C
volatile unsigned char rx_index = 0;
volatile unsigned char rx_buffer[3]; // Holds button, window, temp_high, temp_low
volatile int  type_select = 0;         // Holds value to select type of screen output.
volatile unsigned char newButtonFlag = 0; // Flag set when new button received



//-------------------------------------------------
// Timer-Based Delay Function using Timer_B
//-------------------------------------------------
void delay_timer_ms(unsigned int ms) {
    unsigned int counts = (ms * 32768UL) / 1000; // Convert ms to timer counts
    unsigned int start = TB0R;
    unsigned int elapsed;
    while (1) {
        unsigned int current = TB0R;
        if (current >= start)
            elapsed = current - start;
        else
            // Handle counter wrap-around (period = TB0CCR0 + 1 counts)
            elapsed = (TB0CCR0 - start) + current + 1;
        if (elapsed >= counts)
            break;
    }
}

//-------------------------------------------------
// LCD Low-Level Functions
//-------------------------------------------------
void lcd_pulse_enable(void) {
    P2OUT |= LCD_E;         // Set Enable high on P2.7
    __delay_cycles(50);       // Short fixed delay for edge timing
    P2OUT &= ~LCD_E;        // Set Enable low
    __delay_cycles(50);
}

void lcd_send_nibble(unsigned char nibble) {
    P1OUT &= ~LCD_DATA;                     // Clear LCD data pins (P1.4–P1.7)
    P1OUT |= ((nibble & 0x0F) << 4);          // Map nibble to P1.4–P1.7
    lcd_pulse_enable();
}

void lcd_send_byte(unsigned char byte, unsigned char rs) {
    if(rs)
        P2OUT |= LCD_RS;   // Set RS high for data (on P2.6)
    else
        P2OUT &= ~LCD_RS;  // Clear RS for command

    lcd_send_nibble(byte >> 4);     // Send high nibble
    lcd_send_nibble(byte & 0x0F);     // Send low nibble
    delay_timer_ms(2);              // Allow LCD time to process the byte
}

void lcd_command(unsigned char cmd) {
    lcd_send_byte(cmd, 0);
}

void lcd_data(unsigned char data) {
    if(data == 0xB0) {
         data = 0xDF;
    }
    lcd_send_byte(data, 1);
}

void lcd_print(const char *str) {
    while (*str) {
        lcd_data(*str++);
    }
}

//-------------------------------------------------
// LCD Initialization (4-Bit Mode)
//-------------------------------------------------
void lcd_init(void) {
    delay_timer_ms(20); // Wait >15ms after power-up
    P2OUT &= ~LCD_RS;
    lcd_send_nibble(0x03);
    delay_timer_ms(5);
    lcd_send_nibble(0x03);
    delay_timer_ms(5);
    lcd_send_nibble(0x03);
    delay_timer_ms(1);
    lcd_send_nibble(0x02);
    delay_timer_ms(1);
    lcd_command(0x28); // Function set: 4-bit, 2-line display, 5x8 font.
    lcd_command(0x0C); // Display ON, cursor OFF, blink OFF.
    lcd_command(0x01); // Clear display.
    delay_timer_ms(2);
    lcd_command(0x06); // Entry mode set: Increment cursor, no shift.
}

//-------------------------------------------------
// Update Display Based on the "button" Value
//-------------------------------------------------
void update_display(void) {

    switch (button) {
        case 'D': normal_set(); break;
        case 'A': window_set(); break;
        case 'B': led_set(); break;
        default: normal_set(); break;
    }
    
    
    lcd_command(0xC0);   // Set cursor to start of second line.
    
    lcd_print("T=");
    //intToCharArray(temp_raw,temp);
    lcd_print(temp);

    lcd_command(0xC0 + 6);   // Set cursor to end of temp amount.
    lcd_print("C");
    
    
    lcd_command(0xC0 + 13); // Set cursor to bottom-right (for a 16x2 LCD).
    lcd_print("N=");
    lcd_data(window);
}

void window_set(void) {    
    lcd_command(0x01);   // Clear display.
    delay_timer_ms(2);
    lcd_command(0x80);   // Set cursor to start of first line.
    lcd_print("set window size");

}

void led_set(void) {
    lcd_command(0x01);   // Clear display.
    delay_timer_ms(2);
    lcd_command(0x80);   // Set cursor to start of first line.
    lcd_print("set pattern");


}
void normal_set(void) {
    char *pattern;
    switch(button) {
        case '0': pattern = "static"; break;
        case '1': pattern = "toggle"; break;
        case '2': pattern = "upcounter"; break;
        case '3': pattern = "inandout"; break;
        case '4': pattern = "downcounter"; break;
        case '5': pattern = "rotate1left"; break;
        case '6': pattern = "rotate7right"; break;
        case '7': pattern = "fillleft"; break;
        default:  pattern = "unknown"; break;
    }
    
    lcd_command(0x01);   // Clear display.
    delay_timer_ms(2);
    lcd_command(0x80);   // Set cursor to start of first line.
    lcd_print(pattern);
}


//-------------------------------------------------
// Timer Initialization for Heartbeat LED & Delay
//-------------------------------------------------
void timer_init(void) {
    TB0CCR0 = 16384 - 1;                      // Period = 16384 counts (~500ms)
    TB0CCTL0 = CCIE;                          // Enable CCR0 interrupt
    TB0CTL = TBSSEL__ACLK | MC__UP | TBCLR;     // Use ACLK, up mode, clear timer
}

//-------------------------------------------------
// I2C Slave Initialization
//-------------------------------------------------
void i2c_init(void) {
    UCB0CTLW0 = UCSWRST;                      // Put eUSCI_B0 in reset

    // Configure P1.2 and P1.3 for I2C mode
    P1SEL1 &= ~(BIT3 | BIT2);
    P1SEL0 |= (BIT3 | BIT2);
    
    UCB0CTLW0 &= ~UCTR;                       // Receiver mode
    UCB0CTLW0 &= ~UCMST;                       // Slave mode
    UCB0CTLW0 |= UCMODE_3 | UCSYNC;            // I2C, synchronous mode
    UCB0I2COA0 = 0x48 | UCOAEN;                // Set own address to 0x48 and enable it
    UCB0I2COA0 |= UCGCEN;                      // Enable general call response

    UCB0CTLW0 &= ~UCSWRST;                     // Release from reset
    
    // Enable I2C receive and start condition interrupts
    UCB0IE |= (UCRXIE | UCSTTIE);
}



//-------------------------------------------------
// Timer_B CCR0 Interrupt Service Routine
//-------------------------------------------------
#pragma vector=TIMER0_B0_VECTOR
__interrupt void Timer_B_ISR(void) {
    P2OUT ^= LED;  // Toggle heartbeat LED on P2.0
}

//-------------------------------------------------
// I2C Slave Interrupt Service Routine
//-------------------------------------------------
#pragma vector=EUSCI_B0_VECTOR
__interrupt void EUSCI_B0_ISR(void) {
    unsigned char received = UCB0RXBUF;

    rx_buffer[rx_index++] = received;

    if (rx_index >= 3) {

        button = rx_buffer[0];
        window = rx_buffer[1];
        temp_raw = rx_buffer[2];
        newButtonFlag = 1;
        rx_index = 0; // Reset for next transmission
    }

    UCB0IFG &= ~UCRXIFG;  // Clear receive interrupt flag

}

// This function converts an integer to a string and stores it in 'temp'.

/*void intToCharArray(int number, char *buffer) {
    int i = 0;
    int is_negative = 0;

    // Handle negative numbers.
    if (number < 0) {
        is_negative = 1;
        number = -number;
    }

    // Handle the case of zero explicitly.
    if (number == 0) {
        buffer[i++] = '0';
    } else {
        // Extract digits and store them in reverse order.
        while (number > 0) {
            int digit = number % 10;
            buffer[i++] = '0' + digit;  // Convert digit to character.
            number /= 10;
        }
    }

    // If the number was negative, append the negative sign.
    if (is_negative) {
        buffer[i++] = '-';
    }

    // Terminate the string.
    buffer[i] = '\0';

    // Reverse the string to get the correct order.
    int start = 0;
    int end = i - 1;
    while (start < end) {
        char temp = buffer[start];
        buffer[start] = buffer[end];
        buffer[end] = temp;
        start++;
        end--;
    }

    buffer[3] = buffer[2];
    buffer[2] = '.';
}
*/

int main(void) {
    WDTCTL = WDTPW | WDTHOLD;  // Stop watchdog timer
    PM5CTL0 &= ~LOCKLPM5;      // Unlock GPIO (for MSP430FR2311)
    
    // Configure heartbeat LED on P2.0 as output.
    P2DIR |= LED;
    P2OUT &= ~LED;
    
    // Configure LCD data pins (P1.4–P1.7) as outputs.
    P1DIR |= LCD_DATA;
    P1OUT &= ~LCD_DATA;
    
    // Configure LCD control pins (P2.6 and P2.7) as outputs.
    P2DIR |= (LCD_RS | LCD_E);
    P2OUT &= ~(LCD_RS | LCD_E);
    
    timer_init();   // Initialize Timer_B for delays and LED heartbeat.
    i2c_init();     // Initialize I2C in slave mode.
    lcd_init();     // Initialize the LCD.
    
    update_display(); // Initial display update.
    
    __bis_SR_register(GIE);  // Enable global interrupts.
    
    while(1) {
        update_display();  // Update display based on new I2C input.
        __delay_cycles(500000);
        __delay_cycles(500000);
        __delay_cycles(500000);
        button = 'A';
        window = '5';
        update_display();
        __delay_cycles(500000);
        __delay_cycles(500000);
        window = '6';
        update_display();
        __delay_cycles(500000);
        __delay_cycles(500000);
        button = 'B';
        window = '6';
        update_display();
        __delay_cycles(500000);
        __delay_cycles(500000);
        window = '9';
        update_display();
        __delay_cycles(500000);
        button = 'D';
        update_display();
        button = '3';
        __delay_cycles(500000);
        __delay_cycles(500000);
        __delay_cycles(500000);
        __delay_cycles(500000);



        /*switch(button) {
        case '9': button = 'A'; break;
        case 'A': button = 'B'; break;
        case 'B': button = 'C'; break;
        case 'C': button = 'D'; break;      // demo code incase nothing works properly
        case 'D': button = '*'; break;
        case '*': button = '#'; break;
        case '#': button = '0'; break;
        default:  button++; break;*/ 
    
    }
    
    return 0;
}
