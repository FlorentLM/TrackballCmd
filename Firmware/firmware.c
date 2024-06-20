
// ------------------------ Ports IOD ------------------------
// |   7  |   6  |   5    |   4    |   3  |   2  |   1   |  0  |
// | ---- | ---- | MISO 2 | MISO 1 | ---- | MOSI | CHSEL | CLK |

// D0 (~CLK)   : connected to all cable sockets pin 2
// D1 (~CHSEL) : connected to all cable sockets pin 1 (white paint)
// D2 (~MOSI)  : connected to all cable sockets pin 6
// D3 		   : no cable, may serve as trigger
// D4 (MISO1)  : connected to cable 1 socket pin 4
// D5 (MISO2)  : connected to cable 2 socket pin 4
// D6 		   : no cable, may serve as trigger
// D7		   : no cable, may serve as trigger


// ----------------------- Ports IOB -----------------------
// |   7  |   6  |   5  |   4  |   3  |   2  |   1  |   0  |
// | ---- | ---- | ---- |  IN  |  OUT |  OUT |  OUT | ---- |

// B1 (OUT)	: 1
// B2 (OUT)	: 0
// B3 (OUT)	: may serve as trigger
// B4 (IN)	: Red button


// All the other pins are inactive


//__sfr at 0x80 IOA;		// Port A data (bit addressable)
__sfr __at 0x90 IOB;		// Port B data (bit addressable)
__sfr __at 0xB0 IOD;		// Port D data (bit addressable)

__sfr __at 0x92 _XPAGE;		// Set _XPAGE at MPAGE = upper addr byte of MOVX instr

__sfr __at 0xBA EP01STAT;	// EP0-EP1 status (EP1IN, EP1OUT, EP0 busy)

__sfr __at 0xB2 OEA;		// Port A output enable (0 = in, 1 = out)
__sfr __at 0xB3 OEB;		// Port B output enable (0 = in, 1 = out)
__sfr __at 0xB5 OED;		// Port D output enable (0 = in, 1 = out)

// These variables must be declared into the Cypress xdata storage space (because they're global)
volatile __xdata __at 0xE670 unsigned char PORTACFG;		// Port A config (0x00 = normal I/O)
volatile __xdata __at 0xE68D unsigned char EP1OUTBC;		// EP1 OUT byte count
volatile __xdata __at 0xE68F unsigned char EP1INBC;			// EP1 IN byte count
volatile __xdata __at 0xE780 unsigned char EP1OUTBUF[2];	// Buffer for EP1 OUT data (transferred)
volatile __xdata __at 0xE7C0 unsigned char EP1INBUF[64];	// Buffer for EP1 IN data (to be transferred)

__data signed char resBuf[4];    // Response buffer for data of sensors 1 and 2
__data int siz;

// ------------------------------------------------

void toSensors( unsigned char dat );
void fromSensors( __data signed char resBuf[] );
void toHost( __data signed char resBuf[], __data int siz );

// ------------------------------------------------

void toSensors( unsigned char dat )
{
    __data int i;

	for( i = 7; i >= 0; i-- )		// Send 8 bits to MOSI data line
	{
		IOD = 0x08;			       	// 0000 1000	--> D7-D4 = 0, D3 = 1, MOSI = 0, CHSEL = 0, CLK = 0
		if ( dat & ( 0x01 << i ) )	// if dat[i] = 1	(0x01 shifted left by i)
		{
			IOD |= 0x04;			// 0000 0100	--> Set MOSI = 1
		}

		IOD |= 0x01;			   	// 0000 0001	--> keep D7-D1, set CLK = 1
	}

    for( i = 0; i < 10; i++ ) {;}   // Small delay
}

void toHost( __data signed char resBuf[], __data int siz )
{
    __data int i;

	while ( EP01STAT & 0x04 )	// 0000 0100 	--> If 3rd bit of EP01STAT is 1 (busy bit 3), a transfer is ongoing
	{
	    ;       // Nothing, just wait for bulk-in ack (busy bit 3 = 0)
	}

	for ( i = 0; i < siz; i++ )
	{
		EP1INBUF[i] = resBuf[i]; // Copy resBuf bits to the beginning of EP1INBUF buffer (= buffer towards host)
	}

	EP1INBC = siz;               // Arm EP1 IN byte count with the appropriate number of bytes to be transferred to host
}

void fromSensors( __data signed char resBuf[] )
{
    __data int i;

    resBuf[0] = 0;
	resBuf[1] = 0;

	for( i = 7; i >= 0; i-- )	// sample MISO bits from portD
	{
		IOD &= 0x3E;			// 0011 1110 	--> D7 = 0, D6 = 0, CLK = 0, keep the rest

		IOD |= 0x01;			// 0000 0001 	--> Set CLK = 1, keep the rest

		if ( IOD & 0x10 )		// 0001 0000 	--> if MISO 1 == 1, listen to Sensor 1
		{
			resBuf[0] |= ( 0x01 << i );	// Save into resBuf[0] relevant data lines (IOD for Sensor 1)
		}

		if ( IOD & 0x20 )		// 0010 0000 	--> if MISO 2 == 1, listen to Sensor 2
		{
			resBuf[1] |= ( 0x01 << i );	// Save into resBuf[1] relevant data lines (IOD for Sensor 2)
		}
	}

	IOB = 0x02;				// 0000 0010 	--> Set B1 = 1 (3.2 V), B2 = 0 (0.0 V), the rest at 0
	resBuf[2] = IOB & 0x10;	// 0001 0000 	--> Put IOB into resBuf, but set the 5th bit to 0 if B4 == 0 (red button pressed)
}

void main()
{
	__data unsigned char addr;				// ADNS Addresses
	__data unsigned char command = 0x00;	// Command to send
	__data signed char inBuf[7];			// Input buffer (from the ADNS)
    __data int i;

	OEB = 0x0F;		// 0000 1111 	--> PortB pins B0-B3 = OUT, pins B4-B7 = IN
	OED = 0x0F;		// 0000 1111 	--> PortD pins D0-D3 = OUT (MOSI, CHSEL, CLK), pins D4-D7 = IN (MISO 1, MISO 2)

	while ( 1 )
	{
		EP1OUTBC = 0x01;				// 0000 0001 	--> (Re)arm EP1 OUT byte count (= read 1 byte sent by host)
		while ( EP01STAT & 0x02 )		// 0000 0010 	--> If 2nd bit of EP01STAT is 1 (busy bit 2), a transfer is ongoing
		{
			;	// Nothing, just wait for bulk-out ack (busy bit 2 = 0)
		}

		addr = EP1OUTBUF[0] & 0xFF;		// 1111 1111 	--> Get host command to be transferred to the ADNS

		switch ( addr )
		{
			case 0xBA:				// 0x3A == "RESET" address for ADNS
			{
                toSensors( addr );			// Write 0xBA to ADNS (0x3A & 0x80 = 0xBA, because 7th bit must be 1 for writing)
                command = 0x5A;
                toSensors( command );		// Send 0x5A ("Reset" command) to ADNS
				break;
			}

			case 0xA2:				// 0x22 == "NAV_CTRL2" address for ADNS			TODO: change A2 to 22
			{
                toSensors( addr );			// Write 0xA2 to ADNS (0x22 & 0x80 = 0xA2, because 7th bit must be 1 for writing)
                command = 0x80;
                toSensors( command );		// Send 0x80 command ("Disable rest mode") to ADNS
				break;
			}

			case 0x0B:			        	   // 0x0B == "PIX_GRAB" address for ADNS
			{
				toSensors( addr | 0x80 );	   // Write 0x8B to ADNS (0x0B & 0x80 = 0x8B, 7th bit must be 1 for writing)
				toSensors( 0x00 );		       // Write anything to addr 0x0B to reset pixel counter = 0

				for ( i = 1; i <= 361; i++ )   // We must perform 361 read operations (19x19 pixels) for PIX_GRAB to end
				{
					while ( 1 )           	   // While current pixel not valid
					{
						toSensors( addr );	   // Keep reading ADNS address 0x0B while pixel is not marked valid
						fromSensors( resBuf ); // and keep getting response into resBuf

						// If 7th bit of resBuf[0] AND 7th bit of resBuf[1] are both = 1,
						// it means the two sensors say 'Pixel valid' so we break the while loop
						if ( (resBuf[0] & 0x80) && (resBuf[1] & 0x80) )
						{
							break;
						}
					}

					// Clear 'Pixel valid' bit for both sensors
					resBuf[0] &= 0x7f;	// 0x7f = 0111 1111
					resBuf[1] &= 0x7f;

					toHost( resBuf, 2 );		// Transfer resBuf to host for the 2 current pixels
				}

				break;
			}

			case 0x02:						// 0x02 == "MOTION_STATUS" address for ADNS
			{
				toSensors( addr );			// Write to ADNS' address 0x02
				fromSensors( resBuf );		// Read response from ADNS (into resBuf)

                addr = 0x03;				// 0x03 == "DELTA_X" address
                toSensors( addr );
                fromSensors( resBuf );		// Get DX

                for ( i = 0; i < 2; i++ )
                {
                    inBuf[i] = resBuf[i];	// Start to fill inBuf with DX
                }
                addr = 0x04;				// 0x04 == "DELTA_Y" address
                toSensors( addr );
                fromSensors( resBuf );		// Get DY

                for ( i = 0; i < 2; i++ )
                {
                    inBuf[i + 2] = resBuf[i];	// Continue to fill inBuf with DY
                }
                addr = 0x05;				// 0x05 == "SQUAL" address
                toSensors( addr );
                fromSensors( resBuf );		// Get SQ

                for ( i = 0; i < 3; i++ )
                {
                    inBuf[i + 4] = resBuf[i];		// Continue to fill inBuf with SQ
                }

                toHost( inBuf, 7 );			    	// Finally send inBuf to host

                for ( i = 0; i < 200; i++ ) {;}     // Delay

				break;
			}

			default:
			{
				toSensors( addr );
				fromSensors( resBuf );
				toHost( resBuf, 2 );
			}
		}

		IOD = 0x03;			// 0000 0011	--> Set all triggers = 0, MOSI = 0, CHSEL = 1, CLK = 1
	}
}
