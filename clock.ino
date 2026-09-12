#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <math.h>
#include "BubblegumSans50.h"

#include <WiFi.h>
#include "time.h"
#include "wifi-settings.h"

const char* ntpServer = "pool.ntp.org"; // NTP server
const long gmtOffset_sec = 0; // Adjust for your timezone (e.g., GMT+1 = 3600)
const int daylightOffset_sec = 3600; // Daylight saving offset (set to 0 if not applicable)

// Waveshare ESP32-C6-LCD-1.47
#define TFT_MOSI  6
#define TFT_SCLK  7
#define TFT_CS   14
#define TFT_DC   15
#define TFT_RST  21
#define TFT_BL   22

#define TFT_WIDTH   172
#define TFT_HEIGHT  320
#define TFT_ROTATION 3

#if TFT_ROTATION == 0 || TFT_ROTATION == 2
#define ROT_WIDTH  TFT_WIDTH
#define ROT_HEIGHT TFT_HEIGHT
#elif TFT_ROTATION == 1 || TFT_ROTATION == 3
#define ROT_WIDTH  TFT_HEIGHT
#define ROT_HEIGHT TFT_WIDTH
#else
#error "TFT_ROTATION must be 0, 1, 2, or 3"
#endif

Adafruit_ST7789 tft(&SPI, TFT_CS, TFT_DC, TFT_RST);

#define CLOCK_SIZE 100
uint16_t char_width = 0;

void setup()
{
    Serial.begin(115200);

    // Backlight
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);

    // Configure hardware SPI on the Waveshare pins
    SPI.begin(
        TFT_SCLK,
        -1,          // MISO not used
        TFT_MOSI,
        TFT_CS
    );

    // ST7789
    tft.init(TFT_WIDTH, TFT_HEIGHT);
    tft.setRotation(TFT_ROTATION);
    tft.fillScreen(ST77XX_BLACK);

    initRenderer();

    // calc maximum character width

    for (char c = '0'; c <= '9'; c++)
    {
        int width = glyphScaledAdvance(bubblegum50GetGlyph(c), CLOCK_SIZE);
        if (width > char_width) {char_width = width;}
    }
    // Connect to Wi-Fi
    Serial.printf("Connecting to %s ", ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    }
    Serial.println(" CONNECTED");

    // Initialize and get time
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    struct tm timeinfo;
    // Wait for initial NTP synchronisation
    if (!getLocalTime(&timeinfo))
    {
        Serial.println("Failed to get time");
        return;
    }

    Serial.println("Time synchronised");

    
    // Disconnect Wi-Fi if no longer needed
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
}

uint32_t frame = 0;
uint32_t frame_plus = 2;
uint32_t frame_inc = 1;

void loop()
{
    drawFrame(frame+=frame_plus);

    if(frame_plus == 10) frame_inc = -1;
    if(frame_plus == 2) frame_inc = 1;
    frame += frame_inc;

    delay(33);
}


#define SEA_WIDTH              320
#define SEA_HEIGHT              86

#define SEA_WAVE_HEIGHT        200
#define SEA_WAVE_BREADTH       1000
#define SEA_HORIZON_DENSITY    800

#define SEA_DIRECTION_RIGHT      1
#define SEA_DIRECTION_LEFT      -1

#define SEA_DIRECTION  SEA_DIRECTION_RIGHT

#define SEA_SPEED                3
#define SEA_FLIP_Y               1

#define SEA_LIGHT_HORIZON_MIN  220
#define SEA_LIGHT_NEAR_MIN     200
#define SEA_LIGHT_MAX          220

#define SEA_NOISE_AMOUNT       20   // 0 = none, ~20-40 works well
#define SEA_NOISE_SCALE         4   // larger = broader noise features
#define SEA_NOISE_SPEED         1

#define CLOCK_SIZE 100
#define CLOCK_GLASSY 80

#define CLOCK_BOB_PIXELS       8

// Y position within the 86-pixel sea area.
// 0 = horizon, 85 = foreground
#define CLOCK_BOB_SAMPLE_Y    75

// Number of pixels either side to average.
// Larger = smoother/slower bobbing.
#define CLOCK_BOB_SPREAD      12

// Expected brightness range of the sea.
// Tune these if necessary.
#define CLOCK_BOB_DARK        110
#define CLOCK_BOB_BRIGHT      240

#define SKY_HEIGHT          (ROT_HEIGHT / 2)

#define SKY_SUNRISE_HOUR     7
#define SKY_SUNSET_HOUR     19
#define SKY_TWILIGHT_MINUTES 45

#define SKY_STAR_COUNT       55
#define SKY_STAR_SEED        12345

static int16_t sinTable[256];
uint16_t frameBuffer[ROT_WIDTH * ROT_HEIGHT];

void drawFrame(uint32_t frame)
{
    memset(frameBuffer, 0, sizeof(frameBuffer));
    renderSea(&frameBuffer[ROT_WIDTH * (ROT_HEIGHT / 2)], frame);

    uint16_t startX = (ROT_WIDTH - (char_width * 5) )/ 2;
    struct tm timeinfo;

    if (getLocalTime(&timeinfo))
    {
        // Top half
        drawSky(
            frameBuffer,
            timeinfo
        );
        char hh = '0' + timeinfo.tm_hour / 10;
        char h = '0' + timeinfo.tm_hour % 10;
        char mm = '0' + timeinfo.tm_min / 10;
        char m = '0' + timeinfo.tm_min % 10;
        char chars[] = {
            hh,
            h,
            ':',
            mm,
            m
        };

        for (int i = 0; i < 5; i++)
        {
            /*
            * Sample the wave underneath the centre
            * of this character.
            */
            int centerX =
                startX +
                char_width / 2;

            int bob =
                getClockBobOffset(
                    frameBuffer,
                    centerX
                );

            drawChar(
                chars[i],
                frameBuffer,
                startX + ((char_width - glyphScaledAdvance(bubblegum50GetGlyph(chars[i]), CLOCK_SIZE)) / 2),
                70 + bob,
                0xFFFF,
                CLOCK_SIZE,
                CLOCK_GLASSY
            );

            startX += char_width;
        }
    }
    tft.startWrite();
    tft.setAddrWindow(0, 0, ROT_WIDTH, ROT_HEIGHT);
    tft.writePixels(frameBuffer, ROT_WIDTH * ROT_HEIGHT);
    tft.endWrite();

}

void initRenderer()
{
    for (int i = 0; i < 256; i++)
    {
        float a = i * 2.0f * PI / 256.0f;
        sinTable[i] = sinf(a) * 32767.0f;
    }
}
void drawSky(
    uint16_t *frameBuffer,
    const struct tm &timeinfo)
{
    int minutes =
        timeinfo.tm_hour * 60 +
        timeinfo.tm_min;

    int sunrise =
        SKY_SUNRISE_HOUR * 60;

    int sunset =
        SKY_SUNSET_HOUR * 60;


    /*
     * darkness:
     *
     * 0   = full daylight
     * 255 = full night
     */
    int darkness = 0;

    if (minutes < sunrise - SKY_TWILIGHT_MINUTES)
    {
        darkness = 255;
    }
    else if (minutes < sunrise)
    {
        darkness =
            255 -
            (
                (minutes -
                 (sunrise - SKY_TWILIGHT_MINUTES))
                * 255 /
                SKY_TWILIGHT_MINUTES
            );
    }
    else if (minutes < sunset)
    {
        darkness = 0;
    }
    else if (minutes <
             sunset + SKY_TWILIGHT_MINUTES)
    {
        darkness =
            (minutes - sunset) *
            255 /
            SKY_TWILIGHT_MINUTES;
    }
    else
    {
        darkness = 255;
    }


    /*
     * --------------------------------------------------
     * SKY GRADIENT
     * --------------------------------------------------
     *
     * DAY:
     * top     = deeper blue
     * horizon = pale cyan
     *
     * NIGHT:
     * top     = near black / navy
     * horizon = slightly lighter blue
     */

    for (int y = 0; y < SKY_HEIGHT; y++)
    {
        uint8_t vertical =
            y * 255 /
            (SKY_HEIGHT - 1);


        // Day colours
        uint8_t dayR =
            lerp8(25, 125, vertical);

        uint8_t dayG =
            lerp8(105, 195, vertical);

        uint8_t dayB =
            lerp8(205, 235, vertical);


        // Night colours
        uint8_t nightR =
            lerp8(1, 8, vertical);

        uint8_t nightG =
            lerp8(4, 18, vertical);

        uint8_t nightB =
            lerp8(16, 38, vertical);


        uint8_t r =
            lerp8(dayR, nightR, darkness);

        uint8_t g =
            lerp8(dayG, nightG, darkness);

        uint8_t b =
            lerp8(dayB, nightB, darkness);


        uint16_t color =
            rgb565(r, g, b);


        uint16_t *line =
            frameBuffer +
            y * ROT_WIDTH;

        for (int x = 0;
             x < ROT_WIDTH;
             x++)
        {
            line[x] = color;
        }
    }


    /*
     * --------------------------------------------------
     * STARS
     * --------------------------------------------------
     *
     * Fade in during twilight.
     */
    if (darkness < 40)
        return;


    for (int i = 0;
         i < SKY_STAR_COUNT;
         i++)
    {
        uint32_t h =
            hash2D(
                i + SKY_STAR_SEED,
                SKY_STAR_SEED
            );


        int x =
            h % ROT_WIDTH;

        h = hash2D(
            h,
            i * 37
        );

        /*
         * Keep most stars away from the immediate
         * horizon.
         */
        int y =
            h %
            (SKY_HEIGHT - 6);


        /*
         * Different intrinsic star brightness.
         */
        uint8_t starLevel =
            130 +
            ((h >> 8) & 0x7F);


        /*
         * Fade stars according to nighttime darkness.
         */
        starLevel =
            starLevel *
            darkness /
            255;


        /*
         * Slightly cool white.
         */
        uint8_t r = starLevel;
        uint8_t g = starLevel;

        uint8_t b =
            min(
                255,
                starLevel + 20
            );


        frameBuffer[
            y * ROT_WIDTH + x
        ] = rgb565(r, g, b);


        /*
         * A few larger/brighter stars.
         */
        if ((h & 0x0F) == 0 &&
            x > 0 &&
            x < ROT_WIDTH - 1 &&
            y > 0 &&
            y < SKY_HEIGHT - 1)
        {
            uint16_t glow =
                rgb565(
                    starLevel / 2,
                    starLevel / 2,
                    min(255, starLevel / 2 + 20)
                );

            frameBuffer[
                y * ROT_WIDTH + x - 1
            ] = glow;

            frameBuffer[
                y * ROT_WIDTH + x + 1
            ] = glow;

            frameBuffer[
                (y - 1) * ROT_WIDTH + x
            ] = glow;

            frameBuffer[
                (y + 1) * ROT_WIDTH + x
            ] = glow;
        }
    }
}

static inline uint8_t lerp8(
    uint8_t a,
    uint8_t b,
    uint8_t amount)
{
    return a +
        ((int16_t)(b - a) * amount) / 255;
}


static inline uint16_t blendColor565(
    uint8_t r1,
    uint8_t g1,
    uint8_t b1,
    uint8_t r2,
    uint8_t g2,
    uint8_t b2,
    uint8_t amount)
{
    return rgb565(
        lerp8(r1, r2, amount),
        lerp8(g1, g2, amount),
        lerp8(b1, b2, amount)
    );
}

static inline int16_t fastSin(int p)
{
    return sinTable[p & 255];
}

static inline int16_t fastCos(int p)
{
    return sinTable[(p + 64) & 255];
}

static uint8_t rgb565Brightness(uint16_t color)
{
    // Expand RGB565 to approximately 0..255
    uint8_t r =
        ((color >> 11) & 0x1F) * 255 / 31;

    uint8_t g =
        ((color >> 5) & 0x3F) * 255 / 63;

    uint8_t b =
        (color & 0x1F) * 255 / 31;

    /*
     * Your sea palette's blue/green values correspond
     * closely to wave height, so weight those more
     * strongly than red.
     */
    return
        (r + g * 2 + b * 2) / 5;
}

// HSV -> RGB565
static uint16_t hsv565(uint8_t h, uint8_t s, uint8_t v)
{
    uint8_t region = h / 43;
    uint8_t rem = (h - region * 43) * 6;

    uint8_t p = (v * (255 - s)) >> 8;
    uint8_t q = (v * (255 - ((s * rem) >> 8))) >> 8;
    uint8_t t = (v * (255 - ((s * (255 - rem)) >> 8))) >> 8;

    uint8_t r, g, b;

    switch (region)
    {
        default:
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        case 5: r = v; g = p; b = q; break;
    }

    return ((r & 0xF8) << 8) |
           ((g & 0xFC) << 3) |
           (b >> 3);
}



static inline uint16_t rgb565(
    uint8_t r,
    uint8_t g,
    uint8_t b)
{
    return ((r & 0xF8) << 8) |
           ((g & 0xFC) << 3) |
           (b >> 3);
}

static inline uint32_t hash2D(int x, int y)
{
    uint32_t h = x * 374761393u + y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

static inline int noiseValue(int x, int y)
{
    return (int)(hash2D(x, y) & 255) - 128;
}

static int smoothNoise(int x, int y)
{
    const int scale = SEA_NOISE_SCALE;

    int gx = x / scale;
    int gy = y / scale;

    int fx = x % scale;
    int fy = y % scale;

    int n00 = noiseValue(gx,     gy);
    int n10 = noiseValue(gx + 1, gy);
    int n01 = noiseValue(gx,     gy + 1);
    int n11 = noiseValue(gx + 1, gy + 1);

    int nx0 =
        n00 +
        (n10 - n00) * fx / scale;

    int nx1 =
        n01 +
        (n11 - n01) * fx / scale;

    return
        nx0 +
        (nx1 - nx0) * fy / scale;
}

static int32_t sampleSeaHeight(
    int x,
    int outputY,
    uint32_t frame)
{
    x = constrain(x, 0, SEA_WIDTH - 1);
    outputY = constrain(outputY, 0, SEA_HEIGHT - 1);

#if SEA_FLIP_Y
    int y = SEA_HEIGHT - 1 - outputY;
#else
    int y = outputY;
#endif

    int density =
        100 +
        ((SEA_HEIGHT - 1 - outputY) *
         (SEA_HORIZON_DENSITY - 100)) /
        (SEA_HEIGHT - 1);

    int cx = x - SEA_WIDTH / 2;

    int movement =
        (int)frame *
        SEA_SPEED *
        SEA_DIRECTION;

    int spatial1 =
        (cx * 4 + y * 2) *
        100 / SEA_WAVE_BREADTH;

    int spatial2 =
        (cx * 7 - y * 3) *
        100 / SEA_WAVE_BREADTH;

    int spatial3 =
        (cx * 2 + y * 7) *
        100 / SEA_WAVE_BREADTH;

    spatial1 =
        spatial1 * density / 100;

    spatial2 =
        spatial2 * density / 100;

    spatial3 =
        spatial3 * density / 100;

    int p1 =
        spatial1 -
        movement;

    int p2 =
        spatial2 -
        movement * 2 / 3;

    int p3 =
        spatial3 -
        movement / 3;

    int32_t height =
        fastSin(p1) +
        fastSin(p2) / 2 +
        fastSin(p3) / 4;

    int depth =
        outputY *
        100 /
        (SEA_HEIGHT - 1);

    int waveHeightScale =
        SEA_WAVE_HEIGHT *
        depth *
        depth /
        10000;

    height =
        height *
        waveHeightScale /
        100;

    /*
     * Include the same procedural noise as renderSea().
     */
    int noise =
        smoothNoise(
            x + frame * SEA_NOISE_SPEED,
            y * 2
        );

    noise =
        noise *
        SEA_NOISE_AMOUNT *
        depth /
        10000;

    height += noise * 256;

    return height;
}

static int16_t getClockBobOffset(
    uint16_t *frameBuffer,
    int16_t screenX)
{
    /*
     * Sea begins halfway down the screen.
     */
    int seaScreenY =
        ROT_HEIGHT / 2 +
        CLOCK_BOB_SAMPLE_Y;

    int32_t total = 0;
    int count = 0;

    /*
     * Average a horizontal area beneath the character.
     * This removes small procedural-noise fluctuations
     * while retaining the large wave shape.
     */
    for (int sx = -CLOCK_BOB_SPREAD;
         sx <= CLOCK_BOB_SPREAD;
         sx += 2)
    {
        int sampleX =
            constrain(
                screenX + sx,
                0,
                ROT_WIDTH - 1
            );

        uint16_t pixel =
            frameBuffer[
                seaScreenY * ROT_WIDTH +
                sampleX
            ];

        total += rgb565Brightness(pixel);
        count++;
    }

    int brightness =
        total / count;

    brightness =
        constrain(
            brightness,
            CLOCK_BOB_DARK,
            CLOCK_BOB_BRIGHT
        );

    /*
     * Convert:
     *
     * dark trough  -> +CLOCK_BOB_PIXELS (down)
     * midpoint     -> 0
     * bright crest -> -CLOCK_BOB_PIXELS (up)
     */
    int bob =
        map(
            brightness,
            CLOCK_BOB_DARK,
            CLOCK_BOB_BRIGHT,
            CLOCK_BOB_PIXELS,
            -CLOCK_BOB_PIXELS
        );

    return bob;
}

void renderSea(
    uint16_t *buffer,
    uint32_t frame)
{
    for (int y = 0; y < SEA_HEIGHT; y++)
    {
#if SEA_FLIP_Y
        int outputY = SEA_HEIGHT - 1 - y;
#else
        int outputY = y;
#endif

        /*
         * Perspective:
         *
         * Near edge = 1x wave density
         * Horizon   = SEA_HORIZON_DENSITY / 100
         */
        int density =
            100 +
            ((SEA_HEIGHT - 1 - outputY) *
             (SEA_HORIZON_DENSITY - 100)) /
            (SEA_HEIGHT - 1);


        for (int x = 0; x < SEA_WIDTH; x++)
        {
            int cx = x - SEA_WIDTH / 2;

            /*
             * Horizontal movement.
             *
             * Reversing SEA_DIRECTION reverses the apparent
             * direction in which the water travels.
             */
            int movement =
                (int)frame *
                SEA_SPEED *
                SEA_DIRECTION;


            /*
             * Primary waves travel mostly horizontally.
             *
             * Secondary waves prevent everything looking like
             * perfectly parallel sine stripes.
             */
            int spatial1 =
                (
                    cx * 4 +
                    y * 2
                ) * 100 / SEA_WAVE_BREADTH;

            int spatial2 =
                (
                    cx * 7 -
                    y * 3
                ) * 100 / SEA_WAVE_BREADTH;

            int spatial3 =
                (
                    cx * 2 +
                    y * 7
                ) * 100 / SEA_WAVE_BREADTH;


            /*
             * Make waves progressively smaller toward horizon.
             */
            spatial1 = spatial1 * density / 100;
            spatial2 = spatial2 * density / 100;
            spatial3 = spatial3 * density / 100;


            /*
             * Most motion is along X.
             */
            int p1 =
                spatial1 -
                movement;

            int p2 =
                spatial2 -
                movement * 2 / 3;

            int p3 =
                spatial3 -
                movement / 3;


            int32_t s1 = fastSin(p1);
            int32_t s2 = fastSin(p2);
            int32_t s3 = fastSin(p3);


            int32_t height =
                s1 +
                s2 / 2 +
                s3 / 4;

            int depth = outputY * 100 / (SEA_HEIGHT - 1);

            /*
            * Slowly moving coherent surface noise.
            *
            * Noise amplitude disappears at the horizon and
            * becomes progressively stronger toward the viewer.
            */
            int noise =
                smoothNoise(
                    x + frame * SEA_NOISE_SPEED,
                    y * 2
                );

            noise =
                noise *
                SEA_NOISE_AMOUNT *
                depth /
                10000;
            int waveHeightScale =
                SEA_WAVE_HEIGHT *
                depth *
                depth /
                10000;

            height =
                height *
                waveHeightScale /
                100;

            /*
            * Break up the mathematically-perfect wave surface.
            */
            height += noise * 256;

            int32_t slopeX =
                fastCos(p1) * 5 +
                fastCos(p2) * 2 +
                fastCos(p3);

            int32_t slopeY =
                fastCos(p1) +
                fastCos(p2) * 2 +
                fastCos(p3) * 3;

            slopeX =
                slopeX *
                waveHeightScale /
                100;

            slopeY =
                slopeY *
                waveHeightScale /
                100;
            /*
             * Main directional lighting.
             */
            int light =
                135
                - slopeX / 5500
                - slopeY / 8500;

            light += noise / 3;

            /*
            * Distance-dependent lighting constraint.
            *
            * outputY = 0                 -> horizon
            * outputY = SEA_HEIGHT - 1   -> nearest water
            *
            * Horizon: 225..235
            * Near:    200..235
            */
            int minLight =
                SEA_LIGHT_HORIZON_MIN +
                (
                    (SEA_LIGHT_NEAR_MIN - SEA_LIGHT_HORIZON_MIN)
                    * outputY
                ) / (SEA_HEIGHT - 1);

            light = constrain(
                light,
                minLight,
                SEA_LIGHT_MAX
            );
            /*
             * Detect wave crests.
             *
             * Positive height approaching the peak becomes
             * increasingly bright.
             */
            int crest =
                (height - 18000) /
                300;

            crest = constrain(
                crest,
                0,
                110
            );


            /*
             * Specular reflection.
             */
            int32_t reflection =
                abs(slopeX + 45000) +
                abs(slopeY - 18000);

            int specular = 0;

            if (reflection < 60000)
            {
                specular =
                    (60000 - reflection) /
                    450;

                specular = constrain(
                    specular,
                    0,
                    100
                );
            }


            /*
             * Water depth colouring.
             *
             * Valleys = dark deep blue
             * Midwater = ocean blue
             * Crests = cyan / almost white
             */

            int h =
                constrain(
                    (height >> 9) + 128,
                    0,
                    255
                );

            h = constrain(
                h + noise / 2,
                0,
                255
            );

            int r = 20 + h / 8;
            int g = 75 + h * 2 / 5;
            int b = 110 + h / 2;

            /*
            * General lighting.
            *
            * Keep the floor fairly high so troughs
            * remain coloured instead of becoming black.
            */
            r = r * light / 170;
            g = g * light / 170;
            b = b * light / 170;


            /*
            * Pale cyan wave crests.
            *
            * Add more green than blue to prevent
            * highlights becoming strongly blue.
            */
            r += crest;
            g += crest;
            b += crest;


            /*
            * Specular highlights tend toward white.
            */
            r += specular;
            g += specular;
            b += specular;


            r = constrain(r, 0, 255);
            g = constrain(g, 0, 255);
            b = constrain(b, 0, 255);

            buffer[
                outputY * SEA_WIDTH + x
            ] = rgb565(r, g, b);
        }
    }
}

static inline void frameBufferHLine(
    uint16_t *frameBuffer,
    int16_t screenWidth,
    int16_t screenHeight,
    int16_t x,
    int16_t y,
    int16_t width,
    uint16_t color)
{
    // Vertical clipping
    if (y < 0 || y >= screenHeight)
        return;

    // Horizontal clipping
    if (x < 0)
    {
        width += x;
        x = 0;
    }

    if (x + width > screenWidth)
        width = screenWidth - x;

    if (width <= 0)
        return;

    uint16_t *pixel =
        frameBuffer + y * screenWidth + x;

    while (width--)
        *pixel++ = color;
}

static constexpr uint8_t BUBBLEGUM50_ROW_REFERENCE_FLAG = 0x80;
static constexpr uint8_t BUBBLEGUM50_ROW_REFERENCE_SPAN_FLAG = 0x40;
static constexpr uint8_t BUBBLEGUM50_ROW_REFERENCE_INDEX_MASK = 0x3F;

static inline void decodeGlyphRleRow(
    const uint8_t *const *rowPtrs,
    const uint8_t *const *encodedRowPtrs,
    int encodedRowCount,
    int rowIndex,
    int width,
    uint8_t *out)
{
    if (rowIndex < 0 ||
        rowIndex >= BUBBLEGUM50_HEIGHT)
    {
        memset(out, 0, width);
        return;
    }

    const uint8_t *rowData = rowPtrs[rowIndex];
    for (int depth = 0; depth < encodedRowCount; depth++)
    {
        if (rowData == nullptr)
            break;

        uint8_t control = pgm_read_byte(rowData++);

        if (control & BUBBLEGUM50_ROW_REFERENCE_FLAG)
        {
            if (control & BUBBLEGUM50_ROW_REFERENCE_SPAN_FLAG)
                rowData++;

            int refRow =
                control &
                BUBBLEGUM50_ROW_REFERENCE_INDEX_MASK;

            if (refRow >= 0 &&
                refRow < encodedRowCount)
            {
                rowData = encodedRowPtrs[refRow];
                continue;
            }

            break;
        }

        uint8_t runs = control;
        int x = 0;

        for (uint8_t i = 0; i < runs && x < width; i++)
        {
            uint8_t token = pgm_read_byte(rowData++);
            bool on = token & 0x80;
            int runLength = (token & 0x7F) + 1;
            int end = min(width, x + runLength);
            memset(out + x, on ? 255 : 0, end - x);
            x = end;
        }

        if (x < width)
            memset(out + x, 0, width - x);
        return;
    }

    memset(out, 0, width);
}

static inline uint8_t sampleDecodedGlyphRow(
    const uint8_t *row,
    int width,
    int x)
{
    if (row == nullptr || x < 0 || x >= width)
        return 0;

    return row[x];
}


static inline uint16_t blend565(
    uint16_t background,
    uint16_t foreground,
    uint8_t alpha)
{
    if (alpha == 0)
        return background;

    if (alpha == 255)
        return foreground;

    int br = (background >> 11) & 0x1F;
    int bg = (background >> 5)  & 0x3F;
    int bb =  background        & 0x1F;

    int fr = (foreground >> 11) & 0x1F;
    int fg = (foreground >> 5)  & 0x3F;
    int fb =  foreground        & 0x1F;

    br += ((fr - br) * alpha + 127) / 255;
    bg += ((fg - bg) * alpha + 127) / 255;
    bb += ((fb - bb) * alpha + 127) / 255;

    return
        (br << 11) |
        (bg << 5) |
        bb;
}


void drawGlyph(
    const Bubblegum50Glyph *glyph,
    uint16_t *frameBuffer,
    int16_t x,
    int16_t y,
    uint16_t color,
    uint16_t height,
    uint8_t glassy)
{
    if (height == 0)
        return;

    const uint8_t *data =
        (const uint8_t *)pgm_read_ptr(&glyph->data);

    int width = pgm_read_byte(&glyph->width);

    int scaledWidth =
        (width * height +
         BUBBLEGUM50_HEIGHT - 1) /
        BUBBLEGUM50_HEIGHT;

    const uint8_t *rowPtrs[BUBBLEGUM50_HEIGHT];
    const uint8_t *encodedRowPtrs[BUBBLEGUM50_HEIGHT];
    const uint8_t *scan = data;
    int row = 0;
    int encodedRowCount = 0;

    while (row < BUBBLEGUM50_HEIGHT &&
           encodedRowCount < BUBBLEGUM50_HEIGHT)
    {
        const uint8_t *encodedRow = scan;
        uint8_t control = pgm_read_byte(scan++);
        uint8_t rowSpan = 1;
        bool invalidRow = false;

        if (control & BUBBLEGUM50_ROW_REFERENCE_FLAG)
        {
            // Bit 6 is reserved for the optional span byte, so
            // reference rows address earlier encoded rows with 6 bits.
            if (control & BUBBLEGUM50_ROW_REFERENCE_SPAN_FLAG)
            {
                rowSpan = pgm_read_byte(scan++);

                if (rowSpan == 0)
                    invalidRow = true;
            }
        }
        else
            scan += control;

        encodedRowPtrs[encodedRowCount++] =
            invalidRow ? nullptr : encodedRow;

        if (invalidRow)
        {
            rowPtrs[row++] = nullptr;
            continue;
        }

        // A span-encoded reference repeats the same decoded row.
        for (uint8_t span = 0;
             span < rowSpan &&
             row < BUBBLEGUM50_HEIGHT;
             span++)
            rowPtrs[row++] = encodedRow;
    }

    while (row < BUBBLEGUM50_HEIGHT)
        rowPtrs[row++] = nullptr;

    uint8_t rowCacheA[BUBBLEGUM50_MAX_WIDTH];
    uint8_t rowCacheB[BUBBLEGUM50_MAX_WIDTH];
    int rowCacheAIndex = -1;
    int rowCacheBIndex = -1;


    for (int dy = 0; dy < height; dy++)
    {
        int screenY = y + dy;

        if (screenY < 0 ||
            screenY >= ROT_HEIGHT)
            continue;


        /*
         * Position through glyph: 0 = top, 255 = bottom.
         */
        int verticalPos =
            (dy * 255) /
            max(1, (int)height - 1);


        /*
         * Glass highlight.
         *
         * Strongest roughly 20-30% down the glyph,
         * fading above and below.
         */
        int highlightDistance =
            abs(verticalPos - 65);

        int highlightStrength =
            255 - highlightDistance * 5;

        highlightStrength =
            constrain(
                highlightStrength,
                0,
                255
            );


        int32_t sy =
            (((dy << 8) + 128) *
             BUBBLEGUM50_HEIGHT) /
            height -
            128;

        int sy0 = sy >> 8;
        int sy1 = sy0 + 1;

        uint8_t fy = sy & 0xFF;

        const uint8_t *row0 = nullptr;
        const uint8_t *row1 = nullptr;

        if (sy0 >= 0 && sy0 < BUBBLEGUM50_HEIGHT)
        {
            if (rowCacheAIndex != sy0)
            {
                decodeGlyphRleRow(
                    rowPtrs,
                    encodedRowPtrs,
                    encodedRowCount,
                    sy0,
                    width,
                    rowCacheA
                );
                rowCacheAIndex = sy0;
            }
            row0 = rowCacheA;
        }

        if (sy1 >= 0 && sy1 < BUBBLEGUM50_HEIGHT)
        {
            if (sy1 == rowCacheAIndex)
            {
                row1 = rowCacheA;
            }
            else
            {
                if (rowCacheBIndex != sy1)
                {
                    decodeGlyphRleRow(
                        rowPtrs,
                        encodedRowPtrs,
                        encodedRowCount,
                        sy1,
                        width,
                        rowCacheB
                    );
                    rowCacheBIndex = sy1;
                }
                row1 = rowCacheB;
            }
        }


        for (int dx = 0;
             dx < scaledWidth;
             dx++)
        {
            int screenX = x + dx;

            if (screenX < 0 ||
                screenX >= ROT_WIDTH)
                continue;


            int32_t sx =
                (((dx << 8) + 128) *
                 BUBBLEGUM50_HEIGHT) /
                height -
                128;

            int sx0 = sx >> 8;
            int sx1 = sx0 + 1;

            uint8_t fx = sx & 0xFF;


            int p00 =
                sampleDecodedGlyphRow(
                    row0,
                    width,
                    sx0
                );

            int p10 =
                sampleDecodedGlyphRow(
                    row0,
                    width,
                    sx1
                );

            int p01 =
                sampleDecodedGlyphRow(
                    row1,
                    width,
                    sx0
                );

            int p11 =
                sampleDecodedGlyphRow(
                    row1,
                    width,
                    sx1
                );


            /*
             * Bilinear smoothing.
             */
            int top =
                p00 +
                ((p10 - p00) * fx >> 8);

            int bottom =
                p01 +
                ((p11 - p01) * fx >> 8);

            int coverage =
                top +
                ((bottom - top) * fy >> 8);

            if (coverage <= 0)
                continue;


            uint16_t *pixel =
                &frameBuffer[
                    screenY * ROT_WIDTH +
                    screenX
                ];


            /*
             * -----------------------------------------
             * GLASS BODY
             * -----------------------------------------
             *
             * Normal:
             *   glassy = 0   -> opacity 255
             *
             * Maximum glass:
             *   glassy = 255 -> opacity ~75
             */
            int bodyOpacity =
                255 -
                ((int)glassy * 180 / 255);

            int bodyAlpha =
                coverage *
                bodyOpacity /
                255;


            /*
             * Tint the framebuffer with the requested
             * glyph colour.
             */
            uint16_t result =
                blend565(
                    *pixel,
                    color,
                    bodyAlpha
                );


            /*
             * -----------------------------------------
             * SPECULAR GLASS HIGHLIGHT
             * -----------------------------------------
             */
            if (glassy > 0 &&
                highlightStrength > 0)
            {
                int highlightAlpha =
                    coverage *
                    glassy /
                    255;

                highlightAlpha =
                    highlightAlpha *
                    highlightStrength /
                    255;

                /*
                 * Keep highlight from completely
                 * blowing out the colour.
                 */
                highlightAlpha =
                    highlightAlpha * 150 / 255;

                result =
                    blend565(
                        result,
                        0xFFFF,     // white reflection
                        highlightAlpha
                    );
            }


            *pixel = result;
        }
    }
}


void drawChar(
    char c,
    uint16_t *frameBuffer,
    int16_t x,
    int16_t y,
    uint16_t color,
    uint16_t height,
    uint8_t glassy)
{
    drawGlyph(
        bubblegum50GetGlyph(c),
        frameBuffer,
        x,
        y,
        color,
        height,
        glassy
    );
}

uint16_t glyphScaledAdvance(
    const Bubblegum50Glyph *glyph,
    uint16_t height)
{
    uint8_t advance =
        pgm_read_byte(&glyph->advance);

    return
        ((uint16_t)advance * height) /
        BUBBLEGUM50_HEIGHT;
}
