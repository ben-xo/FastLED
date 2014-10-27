 #ifndef __INC_BLOCK_CLOCKLESS_H
#define __INC_BLOCK_CLOCKLESS_H

#include "controller.h"
#include "lib8tion.h"
#include "led_sysdefs.h"
#include "delay.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Base template for clockless controllers.  These controllers have 3 control points in their cycle for each bit.  The first point
// is where the line is raised hi.  The second pointsnt is where the line is dropped low for a zero.  The third point is where the
// line is dropped low for a one.  T1, T2, and T3 correspond to the timings for those three in clock cycles.
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if defined(__SAM3X8E__)
#define PORT_MASK (((1<<LANES)-1) & ((FIRST_PIN==2) ? 0xFF : 0xFF))

#define HAS_BLOCKLESS 1

#define PORTD_FIRST_PIN 25
#define PORTA_FIRST_PIN 69
#define PORTB_FIRST_PIN 90

typedef union {
  uint8_t bytes[8];
  uint32_t raw[2];
} Lines;

#define TADJUST 0
#define TOTAL ( (T1+TADJUST) + (T2+TADJUST) + (T3+TADJUST) )
#define T1_MARK (TOTAL - (T1+TADJUST))
#define T2_MARK (T1_MARK - (T2+TADJUST))
template <uint8_t LANES, int FIRST_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 50>
class InlineBlockClocklessController : public CLEDController {
	typedef typename FastPin<FIRST_PIN>::port_ptr_t data_ptr_t;
	typedef typename FastPin<FIRST_PIN>::port_t data_t;

	data_t mPinMask;
	data_ptr_t mPort;
	CMinWait<WAIT_TIME> mWait;
public:
	virtual void init() {
    if(FIRST_PIN == 69) {
      switch(LANES) {
        case 8: FastPin<31>::setOutput();
        case 7: FastPin<58>::setOutput();
        case 6: FastPin<100>::setOutput();
        case 5: FastPin<59>::setOutput();
        case 4: FastPin<60>::setOutput();
        case 3: FastPin<61>::setOutput();
        case 2: FastPin<68>::setOutput();
        case 1: FastPin<69>::setOutput();
      }
    } else if(FIRST_PIN == 25) {
      switch(LANES) {
        case 8: FastPin<11>::setOutput();
        case 7: FastPin<29>::setOutput();
        case 6: FastPin<15>::setOutput();
        case 5: FastPin<14>::setOutput();
        case 4: FastPin<28>::setOutput();
        case 3: FastPin<27>::setOutput();
        case 2: FastPin<26>::setOutput();
        case 1: FastPin<25>::setOutput();
      }
    } else if(FIRST_PIN == 90) {
      switch(LANES) {
        case 8: FastPin<97>::setOutput();
        case 7: FastPin<96>::setOutput();
        case 6: FastPin<95>::setOutput();
        case 5: FastPin<94>::setOutput();
        case 4: FastPin<93>::setOutput();
        case 3: FastPin<92>::setOutput();
        case 2: FastPin<91>::setOutput();
        case 1: FastPin<90>::setOutput();
      }
    }
    mPinMask = FastPin<FIRST_PIN>::mask();
    mPort = FastPin<FIRST_PIN>::port();
	}

	virtual void clearLeds(int nLeds) {
		showColor(CRGB(0, 0, 0), nLeds, 0);
	}

	// set all the leds on the controller to a given color
	virtual void showColor(const struct CRGB & rgbdata, int nLeds, CRGB scale) {
    MultiPixelController<LANES,PORT_MASK,RGB_ORDER> pixels(rgbdata,nLeds, scale, getDither() );
		mWait.wait();
		cli();
		SysClockSaver savedClock(TOTAL);

    uint32_t clocks = showRGBInternal(pixels, nLeds);

		long microsTaken = CLKS_TO_MICROS(clocks);
		long millisTaken = (microsTaken / 1000);
		savedClock.restore();
		while(millisTaken-- > 0) { TimeTick_Increment(); }
		sei();
		mWait.mark();
	}

// #define ADV_RGB
#define ADV_RGB if(maskbit & PORT_MASK) { rgbdata += nLeds; } maskbit <<= 1;

	virtual void show(const struct CRGB *rgbdata, int nLeds, CRGB scale) {
    MultiPixelController<LANES,PORT_MASK,RGB_ORDER> pixels(rgbdata,nLeds, scale, getDither() );
		mWait.wait();
		cli();
		SysClockSaver savedClock(TOTAL);

		uint32_t clocks = showRGBInternal(pixels, nLeds);


		long microsTaken = CLKS_TO_MICROS(clocks);
		long millisTaken = (microsTaken / 1000);
		savedClock.restore();
		while(millisTaken-- > 0) { TimeTick_Increment(); }
		sei();
		mWait.mark();
	}

#ifdef SUPPORT_ARGB
	virtual void show(const struct CARGB *rgbdata, int nLeds, CRGB scale) {
		mWait.wait();
		cli();

		showRGBInternal(PixelController<RGB_ORDER>(rgbdata, nLeds, scale, getDither()));


		// Adjust the timer
		long microsTaken = nLeds * CLKS_TO_MICROS(24 * (T1 + T2 + T3));
		MS_COUNTER += (microsTaken / 1000);
		sei();
		mWait.mark();
	}
#endif

#define VAL *((uint32_t*)(SysTick_BASE + 8))

	template<int BITS,int PX> __attribute__ ((always_inline)) inline static void writeBits(register uint32_t & next_mark, register Lines & b, MultiPixelController<LANES, PORT_MASK, RGB_ORDER> &pixels) { // , register uint32_t & b2)  {
		register Lines b2;
    transpose8x1(b.bytes,b2.bytes);

    register uint8_t d = pixels.template getd<PX>(pixels);
    register uint8_t scale = pixels.template getscale<PX>(pixels);

		for(uint32_t i = 0; (i < LANES) && (i<8); i++) {
			while(VAL > next_mark);

			next_mark = VAL - (TOTAL);
			*FastPin<33>::sport() = PORT_MASK;

			while((VAL-next_mark) > (T2+T3+6));
			*FastPin<33>::cport() = (~b2.bytes[7-i]) & PORT_MASK;

			while((VAL - next_mark) > T3);
			*FastPin<33>::cport() = PORT_MASK;

      b.bytes[i] = pixels.template loadAndScale<PX>(pixels,i,d,scale);
		}

    for(uint32_t i = LANES; i < 8; i++) {
      while(VAL > next_mark);

      next_mark = VAL - (TOTAL);
      *FastPin<33>::sport() = PORT_MASK;

      while((VAL-next_mark) > (T2+T3+6));
      *FastPin<33>::cport() = (~b2.bytes[7-i]) & PORT_MASK;

      while((VAL - next_mark) > T3);
      *FastPin<33>::cport() = PORT_MASK;
    }
	}

	// This method is made static to force making register Y available to use for data on AVR - if the method is non-static, then
	// gcc will use register Y for the this pointer.
	static uint32_t showRGBInternal(MultiPixelController<LANES, PORT_MASK, RGB_ORDER> &allpixels, int nLeds) {
		// Serial.println("Entering show");
		// Setup the pixel controller and load/scale the first byte
		Lines b0;
    allpixels.preStepFirstByteDithering();
		for(int i = 0; i < LANES; i++) {
			b0.bytes[i] = allpixels.loadAndScale0(i);
		}

		// Setup and start the clock
		register volatile uint32_t *CTPTR asm("r6")= &SysTick->CTRL; FORCE_REFERENCE(CTPTR);
		_LOAD = 0x00FFFFFF;
		_VAL = 0;
		_CTRL |= SysTick_CTRL_CLKSOURCE_Msk;
		_CTRL |= SysTick_CTRL_ENABLE_Msk;

		VAL = 0;
		uint32_t next_mark = (VAL - (TOTAL));
		while(nLeds--) {
      allpixels.stepDithering();

			// Write first byte, read next byte
			writeBits<8+XTRA0,1>(next_mark, b0, allpixels);

			// Write second byte, read 3rd byte
			writeBits<8+XTRA0,2>(next_mark, b0, allpixels);

      allpixels.advanceData();
			// Write third byte
			writeBits<8+XTRA0,0>(next_mark, b0, allpixels);
		}

		return 0x00FFFFFF - _VAL;
	}


};

#endif

#endif
