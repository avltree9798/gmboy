#include <apu.h>
#include <interrupts.h>
#include <cpu.h>
#include <string.h>
#include <SDL2/SDL.h>

/* -------------------------
   Overview (DMG APU, minimal)
   - FS (Frame Sequencer) runs at 512 Hz (step every 8192 ticks).
   - Steps 0,2,4,6: length
   - Steps 2,6     : sweep (ch1)
   - Step  7       : envelope
   Channels:
     1) Square + sweep (NR10..NR14)
     2) Square        (NR21..NR24)
     3) Wave          (NR30..NR34)  [stubbed off for now]
     4) Noise         (NR41..NR44)
   Mixer:
     NR50 (FF24): master L/R volume (3-bit each), VIN ignored
     NR51 (FF25): route ch1-4 to L/R
     NR52 (FF26): power + ch on flags
   SDL:
     Simple ring buffer + callback. CPU produces samples inside apu_tick()
--------------------------*/

#define RING_SAMPLES   (48000 * 2) // ~1s stereo buffer
#define CLAMP(v, lo, hi) ((v)<(lo)?(lo):((v)>(hi)?(hi):(v)))

typedef struct {
    // Common APU power & mixer
    bool power;
    u8 nr50, nr51, nr52;

    // Frame Sequencer
    u32 fs_cycle_accum;  // tick counter for 512Hz
    u8  fs_step;         // 0..7

    // Sample rate conversion
    int sample_rate;
    double cycles_per_sample;  // 4194304 / sample_rate
    double sample_accum;
    double acc_l, acc_r;        // running sum of instantaneous L/R
    u32    acc_n;               // how many APU cycles accumulated

    // SDL audio
    SDL_AudioDeviceID dev;
    int16_t ring[RING_SAMPLES * 2]; // stereo interleaved
    volatile u32 rhead, rtail;

    // Channel 1: Square + sweep
    struct {
        bool enabled;
        u8 duty;              // 0..3
        u16 freq;             // 11-bit
        u16 timer;            // down-counter
        u8  duty_pos;         // 0..7
        u8  length;           // 0..63 (64 steps)
        bool length_enable;

        // Envelope
        u8  env_period;       // 0..7 (0 => special means no ticks)
        u8  env_vol;          // 0..15 current volume
        bool env_increase;    // true=up, false=down
        u8  env_counter;      // countdown

        // Sweep
        u8  sweep_period;     // 0..7
        bool sweep_negate;
        u8  sweep_shift;      // 0..7
        u8  sweep_counter;    // countdown
        bool sweep_enabled;   // as HW

        // Trigger latch of NRx2 initial volume (for retrigger)
        u8  init_volume;
    } ch1;

    // Channel 2: Square (no sweep)
    struct {
        bool enabled;
        u8 duty;
        u16 freq;
        u16 timer;
        u8 duty_pos;
        u8 length;
        bool length_enable;

        u8  env_period;
        u8  env_vol;
        bool env_increase;
        u8  env_counter;

        u8  init_volume;
    } ch2;

    // Channel 3: Wave (stubbed: always zero output)
    struct {
        bool enabled;       // NR30 bit7
        bool dac_on;        // NR30 bit7 (same)
        u8  length;         // 0..255
        bool length_enable; // NR34 bit6
        u16 freq;
        u16 timer;
        u8  pos;            // 0..31
        u8  level;          // NR32 (00:mute, 01:100%, 10:50%, 11:25%)
        u8  wave_ram[16];   // 32 samples (4-bit) – two per byte
    } ch3;

    // Channel 4: Noise
    struct {
        bool enabled;
        u8 length;           // 0..63 (64 steps)
        bool length_enable;

        u8  env_period;
        u8  env_vol;
        bool env_increase;
        u8  env_counter;

        u16 lfsr;            // 15-bit LFSR
        u8  clock_shift;     // NR43 bits6..4
        u8  width_mode7;     // NR43 bit3 (1=7-bit)
        u8  divisor_code;    // NR43 bits2..0 (0=>8)
        u16 timer;           // noise timer
    } ch4;

} apu_t;

static apu_t A;

// ---- SDL audio callback: read from ring buffer ----
static void sdl_cb(void* userdata, Uint8* stream, int len_bytes) {
    (void)userdata;
    int16_t* out = (int16_t*)stream;
    int n = len_bytes / sizeof(int16_t);
    for (int i=0; i<n; i++) {
        if (A.rtail != A.rhead) {
            out[i] = A.ring[A.rtail];
            A.rtail = (A.rtail + 1) % (RING_SAMPLES*2);
        } else {
            out[i] = 0;
        }
    }
}

static inline void ring_push_stereo(int16_t L, int16_t R) {
    u32 nh = (A.rhead + 2) % (RING_SAMPLES*2);
    // overwrite if overrun (simple)
    A.ring[A.rhead] = L;
    A.ring[(A.rhead+1)%(RING_SAMPLES*2)] = R;
    A.rhead = nh;
}

// Duty tables (8-step)
static const int DUTY[4][8] = {
    {0,0,0,0,0,0,0,1}, // 12.5%
    {1,0,0,0,0,0,0,1}, // 25%
    {1,0,0,0,0,1,1,1}, // 50%
    {0,1,1,1,1,1,1,0}, // 75%
};

// ---- helpers ----
static inline void chx_length_tick(u8* length, bool enable, bool* on_flag) {
    if (enable && *length) {
        *length -= 1;
        if (*length == 0 && on_flag) *on_flag = false;
    }
}

static inline u16 sq_period(u16 freq) { // (2048 - freq) * 4
    u16 base = 2048 - (freq & 0x7FF);
    return base ? (base << 2) : 4; // avoid zero
}

static inline u16 noise_period(u8 div_code, u8 shift) {
    static const int divisors[8] = {8,16,32,48,64,80,96,112};
    int d = divisors[div_code & 7];
    return (u16)(d << (shift & 0xF));
}

// ---- envelope tick (64 Hz via FS step 7) ----
static void envelope_tick(u8 period, bool increase, u8* env_counter, u8* vol) {
    if (period == 0) return; // freezes on HW
    if (*env_counter == 0) *env_counter = period;
    if (--(*env_counter) == 0) {
        *env_counter = period;
        if (increase && *vol < 15) (*vol)++;
        else if (!increase && *vol > 0) (*vol)--;
    }
}

// ---- sweep tick (128 Hz via FS steps 2 & 6) ----
static bool sweep_apply(void) {
    // Compute next freq from current
    u16 f = A.ch1.freq & 0x7FF;
    u16 delta = f >> (A.ch1.sweep_shift & 7);
    if (A.ch1.sweep_negate) {
        f = f - delta;
        if ((int)f < 0) return false;
    } else {
        f = f + delta;
        if (f > 2047) return false;
    }
    A.ch1.freq = f;
    // overflow => disable ch1
    if (f > 2047) return false;
    return true;
}

static void ch1_sweep_tick(void) {
    u8 p = A.ch1.sweep_period & 7;
    if (p == 0) return;
    if (--A.ch1.sweep_counter == 0) {
        A.ch1.sweep_counter = p;
        if (A.ch1.sweep_shift != 0) {
            if (!sweep_apply()) {
                A.ch1.enabled = false;
            } else {
                // second calc for overflow check (not applied)
                u16 save = A.ch1.freq;
                if (!sweep_apply()) A.ch1.enabled = false;
                A.ch1.freq = save;
            }
        }
    }
}

// ---- channel outputs (0..15) per channel, unmixed ----
static inline int ch1_output(void) {
    if (!A.ch1.enabled) return 0;
    if (A.ch1.timer == 0) A.ch1.timer = sq_period(A.ch1.freq);
    // square step
    if (--A.ch1.timer == 0) {
        A.ch1.timer = sq_period(A.ch1.freq);
        A.ch1.duty_pos = (A.ch1.duty_pos + 1) & 7;
    }
    int bit = DUTY[A.ch1.duty & 3][A.ch1.duty_pos];
    return bit ? A.ch1.env_vol : 0;
}

static inline int ch2_output(void) {
    if (!A.ch2.enabled) return 0;
    if (A.ch2.timer == 0) A.ch2.timer = sq_period(A.ch2.freq);
    if (--A.ch2.timer == 0) {
        A.ch2.timer = sq_period(A.ch2.freq);
        A.ch2.duty_pos = (A.ch2.duty_pos + 1) & 7;
    }
    int bit = DUTY[A.ch2.duty & 3][A.ch2.duty_pos];
    return bit ? A.ch2.env_vol : 0;
}

static inline int ch3_output(void) {
    // Stub: return 0 until you wire NR30-34 fully
    (void)A;
    return 0;
}

static inline int ch4_output(void) {
    if (!A.ch4.enabled) return 0;
    // timer
    if (A.ch4.timer == 0) {
        A.ch4.timer = noise_period(A.ch4.divisor_code, A.ch4.clock_shift);
    }
    if (--A.ch4.timer == 0) {
        A.ch4.timer = noise_period(A.ch4.divisor_code, A.ch4.clock_shift);
        // 15-bit LFSR tap xor (bit0 ^ bit1) then shift in
        u16 x = (A.ch4.lfsr ^ (A.ch4.lfsr >> 1)) & 1;
        A.ch4.lfsr = (A.ch4.lfsr >> 1) | (x << 14);
        if (A.ch4.width_mode7) {
            // also set bit6 (7-bit mode)
            A.ch4.lfsr = (A.ch4.lfsr & ~(1<<6)) | (x << 6);
        }
    }
    int out = (~A.ch4.lfsr) & 1; // bit0 inverted -> 1 is "high"
    return out ? A.ch4.env_vol : 0;
}

// ---- mixer ----
static void mix_and_push_sample(void) {
    // Per-channel dry values (0..15)
    int s1 = ch1_output();
    int s2 = ch2_output();
    int s3 = ch3_output();
    int s4 = ch4_output();

    // NR51 routing
    int l = 0, r = 0;
    if (A.nr51 & (1<<4)) l += s1; if (A.nr51 & (1<<0)) r += s1;
    if (A.nr51 & (1<<5)) l += s2; if (A.nr51 & (1<<1)) r += s2;
    if (A.nr51 & (1<<6)) l += s3; if (A.nr51 & (1<<2)) r += s3;
    if (A.nr51 & (1<<7)) l += s4; if (A.nr51 & (1<<3)) r += s4;

    // NR50 master volume 0..7 -> simply scale
    int lv = (A.nr50 >> 4) & 7;
    int rv = (A.nr50 >> 0) & 7;

    // crude normalisation: each channel 0..15, 4 channels sum 0..60
    // scale by (master+1)/8 to get 0..1, then to int16
    float lf = (l / 60.0f) * ((lv + 1) / 8.0f);
    float rf = (r / 60.0f) * ((rv + 1) / 8.0f);

    int16_t L = (int16_t)CLAMP((int)(lf * 32767.0f), -32768, 32767);
    int16_t R = (int16_t)CLAMP((int)(rf * 32767.0f), -32768, 32767);

    ring_push_stereo(L, R);
}

// ---- frame sequencer step ----
static void fs_step(void) {
    // length @ 0,2,4,6
    if ((A.fs_step & 1) == 0) {
        chx_length_tick(&A.ch1.length, A.ch1.length_enable, &A.ch1.enabled);
        chx_length_tick(&A.ch2.length, A.ch2.length_enable, &A.ch2.enabled);
        chx_length_tick(&A.ch4.length, A.ch4.length_enable, &A.ch4.enabled);
        // ch3 stubbed
    }
    // sweep @ 2,6
    if (A.fs_step == 2 || A.fs_step == 6) {
        ch1_sweep_tick();
    }
    // envelope @ 7
    if (A.fs_step == 7) {
        envelope_tick(A.ch1.env_period, A.ch1.env_increase, &A.ch1.env_counter, &A.ch1.env_vol);
        envelope_tick(A.ch2.env_period, A.ch2.env_increase, &A.ch2.env_counter, &A.ch2.env_vol);
        envelope_tick(A.ch4.env_period, A.ch4.env_increase, &A.ch4.env_counter, &A.ch4.env_vol);
    }
    A.fs_step = (A.fs_step + 1) & 7;
}

// ---- public API ----
void apu_init(int sample_rate) {
    memset(&A, 0, sizeof(A));
    A.sample_rate = sample_rate <= 0 ? 48000 : sample_rate;
    A.cycles_per_sample = (double)APU_CLOCK_HZ / (double)A.sample_rate;

    SDL_AudioSpec want = {0}, have = {0};
    want.freq = A.sample_rate;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024; // callback chunk
    want.callback = sdl_cb;
    A.dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (A.dev) SDL_PauseAudioDevice(A.dev, 0);

    apu_reset();
}

void apu_reset(void) {
    // Power on
    A.power = true;
    A.acc_l = A.acc_r = 0.0;
    A.acc_n = 0;
    A.nr50 = 0x77; // max volumes by default
    A.nr51 = 0xF3; // typical route (ch1-4 -> R/L), tweak as you like
    A.nr52 = 0x80; // power bit set

    A.fs_cycle_accum = 0;
    A.fs_step = 0;
    A.sample_accum = 0.0;
    A.rhead = A.rtail = 0;

    // Channel defaults
    memset(&A.ch1, 0, sizeof(A.ch1));
    memset(&A.ch2, 0, sizeof(A.ch2));
    memset(&A.ch3, 0, sizeof(A.ch3));
    memset(&A.ch4, 0, sizeof(A.ch4));
    A.ch4.lfsr = 0x7FFF;
}

static inline void ch1_step_1cycle(void) {
    if (!A.ch1.enabled) return;
    if (A.ch1.timer == 0) A.ch1.timer = (2048 - (A.ch1.freq & 0x7FF)) << 2;
    if (--A.ch1.timer == 0) {
        A.ch1.timer = (2048 - (A.ch1.freq & 0x7FF)) << 2;
        A.ch1.duty_pos = (A.ch1.duty_pos + 1) & 7;
    }
}

static inline void ch2_step_1cycle(void) {
    if (!A.ch2.enabled) return;
    if (A.ch2.timer == 0) A.ch2.timer = (2048 - (A.ch2.freq & 0x7FF)) << 2;
    if (--A.ch2.timer == 0) {
        A.ch2.timer = (2048 - (A.ch2.freq & 0x7FF)) << 2;
        A.ch2.duty_pos = (A.ch2.duty_pos + 1) & 7;
    }
}

static inline void ch3_step_1cycle(void) {
    if (!A.ch3.enabled || !A.ch3.dac_on) return;
    if (A.ch3.timer == 0) {
        u16 base = 2048 - (A.ch3.freq & 0x7FF);
        A.ch3.timer = base ? (base << 1) : 2;
    }
    if (--A.ch3.timer == 0) {
        u16 base = 2048 - (A.ch3.freq & 0x7FF);
        A.ch3.timer = base ? (base << 1) : 2;
        A.ch3.pos = (A.ch3.pos + 1) & 31;
    }
}

static inline void ch4_step_1cycle(void) {
    if (!A.ch4.enabled) return;
    if (A.ch4.timer == 0) {
        static const int divs[8] = {8,16,32,48,64,80,96,112};
        A.ch4.timer = (u16)(divs[A.ch4.divisor_code & 7] << (A.ch4.clock_shift & 0xF));
    }
    if (--A.ch4.timer == 0) {
        static const int divs[8] = {8,16,32,48,64,80,96,112};
        A.ch4.timer = (u16)(divs[A.ch4.divisor_code & 7] << (A.ch4.clock_shift & 0xF));
        u16 x = (A.ch4.lfsr ^ (A.ch4.lfsr >> 1)) & 1;
        A.ch4.lfsr = (A.ch4.lfsr >> 1) | (x << 14);
        if (A.ch4.width_mode7) {
            A.ch4.lfsr = (A.ch4.lfsr & ~(1<<6)) | (x << 6);
        }
    }
}


void apu_tick(void) {
    if (!A.power) return;

    // 512 Hz frame sequencer
    if (++A.fs_cycle_accum >= 8192) {
        A.fs_cycle_accum = 0;
        fs_step(); // your existing length/sweep/envelope ticks
    }

    // --- step channels one APU cycle ---
    ch1_step_1cycle();
    ch2_step_1cycle();
    ch3_step_1cycle();
    ch4_step_1cycle();

    // --- compute instantaneous output for THIS cycle ---
    int s1 = 0, s2 = 0, s3 = 0, s4 = 0;

    if (A.ch1.enabled) {
        int bit = DUTY[A.ch1.duty & 3][A.ch1.duty_pos];
        s1 = bit ? (A.ch1.env_vol & 0x0F) : 0;
    }
    if (A.ch2.enabled) {
        int bit = DUTY[A.ch2.duty & 3][A.ch2.duty_pos];
        s2 = bit ? (A.ch2.env_vol & 0x0F) : 0;
    }
    if (A.ch3.enabled && A.ch3.dac_on) {
        u8 b = A.ch3.wave_ram[A.ch3.pos >> 1];
        u8 s4 = (A.ch3.pos & 1) ? (b & 0x0F) : (b >> 4);
        switch (A.ch3.level) { case 0: s3=0; break; case 1: s3=s4; break; case 2: s3=s4>>1; break; case 3: s3=s4>>2; break; }
    }
    if (A.ch4.enabled) {
        int out = (~A.ch4.lfsr) & 1; // DMG high when bit0==0
        s4 = out ? (A.ch4.env_vol & 0x0F) : 0;
    }

    // route & scale this cycle
    int l = 0, r = 0;
    if (A.nr51 & (1<<4)) l += s1; if (A.nr51 & (1<<0)) r += s1;
    if (A.nr51 & (1<<5)) l += s2; if (A.nr51 & (1<<1)) r += s2;
    if (A.nr51 & (1<<6)) l += s3; if (A.nr51 & (1<<2)) r += s3;
    if (A.nr51 & (1<<7)) l += s4; if (A.nr51 & (1<<3)) r += s4;

    int lv = (A.nr50 >> 4) & 7;
    int rv = (A.nr50 >> 0) & 7;
    // normalise to 0..1 per cycle
    double lf = (l / 60.0) * ((lv + 1) / 8.0);
    double rf = (r / 60.0) * ((rv + 1) / 8.0);

    // accumulate for resampling
    A.acc_l += lf;
    A.acc_r += rf;
    A.acc_n += 1;

    // produce one PCM sample when enough APU cycles elapsed
    A.sample_accum += 1.0;
    if (A.sample_accum >= A.cycles_per_sample) {
        double nsamp = A.acc_n ? (double)A.acc_n : 1.0;
        double Lf = A.acc_l / nsamp;
        double Rf = A.acc_r / nsamp;

        int16_t L = (int16_t)CLAMP((int)(Lf * 32767.0), -32768, 32767);
        int16_t R = (int16_t)CLAMP((int)(Rf * 32767.0), -32768, 32767);
        ring_push_stereo(L, R);

        A.sample_accum -= A.cycles_per_sample;
        A.acc_l = A.acc_r = 0.0;
        A.acc_n = 0;
    }
}


/* ---------------- I/O mapping ----------------
   Ch1: FF10..FF14
   Ch2: FF16..FF19
   Ch3: FF1A..FF1E (stubbed)
   Ch4: FF20..FF23
   Mixer: FF24..FF26
------------------------------------------------*/

u8 apu_io_read(u16 a) {
    if (a == 0xFF26) return (A.power ? 0x80 : 0x00)
        | (A.ch1.enabled?1:0) | ((A.ch2.enabled?1:0)<<1)
        | ((A.ch3.enabled?1:0)<<2) | ((A.ch4.enabled?1:0)<<3);
    if (a == 0xFF24) return A.nr50;
    if (a == 0xFF25) return A.nr51;

    // For simplicity return reasonable shadows; many regs read back as last written
    switch (a) {
        case 0xFF10: return (A.ch1.sweep_period<<4) | (A.ch1.sweep_negate?0x08:0) | (A.ch1.sweep_shift & 7);
        case 0xFF11: return (A.ch1.duty<<6) | (64 - (A.ch1.length?A.ch1.length:64));
        case 0xFF12: return (A.ch1.init_volume<<4) | (A.ch1.env_increase?0x08:0) | (A.ch1.env_period & 7);
        case 0xFF13: return A.ch1.freq & 0xFF;
        case 0xFF14: return (A.ch1.length_enable?0x40:0) | ((A.ch1.freq>>8)&7);

        case 0xFF16: return (A.ch2.duty<<6) | (64 - (A.ch2.length?A.ch2.length:64));
        case 0xFF17: return (A.ch2.init_volume<<4) | (A.ch2.env_increase?0x08:0) | (A.ch2.env_period & 7);
        case 0xFF18: return A.ch2.freq & 0xFF;
        case 0xFF19: return (A.ch2.length_enable?0x40:0) | ((A.ch2.freq>>8)&7);

        // Wave and noise readbacks omitted/minimal
        default: return 0xFF;
    }
}

static void ch1_trigger(void) {
    A.ch1.enabled = true;
    if (A.ch1.length == 0) A.ch1.length = 64;
    A.ch1.timer = sq_period(A.ch1.freq);
    A.ch1.duty_pos = 0;
    // Envelope reload
    A.ch1.env_vol = A.ch1.init_volume & 0x0F;
    A.ch1.env_counter = A.ch1.env_period ? A.ch1.env_period : 8;
    // Sweep init
    A.ch1.sweep_counter = (A.ch1.sweep_period ? A.ch1.sweep_period : 8);
    A.ch1.sweep_enabled = (A.ch1.sweep_period || A.ch1.sweep_shift);
    if (A.ch1.sweep_shift) {
        // pre-calc overflow check
        u16 save = A.ch1.freq;
        if (!sweep_apply()) A.ch1.enabled = false;
        A.ch1.freq = save;
    }
}

static void ch2_trigger(void) {
    A.ch2.enabled = true;
    if (A.ch2.length == 0) A.ch2.length = 64;
    A.ch2.timer = sq_period(A.ch2.freq);
    A.ch2.duty_pos = 0;
    A.ch2.env_vol = A.ch2.init_volume & 0x0F;
    A.ch2.env_counter = A.ch2.env_period ? A.ch2.env_period : 8;
}

static void ch4_trigger(void) {
    A.ch4.enabled = true;
    if (A.ch4.length == 0) A.ch4.length = 64;
    A.ch4.lfsr = 0x7FFF;
    A.ch4.timer = noise_period(A.ch4.divisor_code, A.ch4.clock_shift);
    A.ch4.env_vol = A.ch4.env_vol & 0x0F; // keep last
    A.ch4.env_counter = A.ch4.env_period ? A.ch4.env_period : 8;
}

void apu_io_write(u16 a, u8 v) {
    if (a == 0xFF26) {
        bool new_power = (v & 0x80) != 0;
        if (!new_power) {
            // power off: clear everything
            apu_reset();
            A.power = false;
            A.nr52 = 0x00;
        } else {
            A.power = true;
            A.nr52 = 0x80;
        }
        return;
    }
    if (!A.power) return; // writes ignored when power=0, except FF26

    switch (a) {
        // Ch1 sweep
        case 0xFF10:
            A.ch1.sweep_period = (v >> 4) & 7;
            A.ch1.sweep_negate = (v & 0x08) != 0;
            A.ch1.sweep_shift  = v & 7;
            break;
        // Ch1 duty/length
        case 0xFF11:
            A.ch1.duty   = (v >> 6) & 3;
            A.ch1.length = 64 - (v & 0x3F);
            break;
        // Ch1 envelope
        case 0xFF12:
            A.ch1.init_volume = (v >> 4) & 0x0F;
            A.ch1.env_increase = (v & 0x08) != 0;
            A.ch1.env_period = v & 7;
            if ((v & 0xF8) == 0) { A.ch1.enabled = false; } // DAC off -> channel off
            break;
        // Ch1 freq lo
        case 0xFF13:
            A.ch1.freq = (A.ch1.freq & 0x0700) | v;
            break;
        // Ch1 trigger / hi
        case 0xFF14:
            A.ch1.length_enable = (v & 0x40) != 0;
            A.ch1.freq = (A.ch1.freq & 0x00FF) | ((v & 7) << 8);
            if (v & 0x80) ch1_trigger();
            break;

        // Ch2 duty/length
        case 0xFF16:
            A.ch2.duty   = (v >> 6) & 3;
            A.ch2.length = 64 - (v & 0x3F);
            break;
        // Ch2 envelope
        case 0xFF17:
            A.ch2.init_volume = (v >> 4) & 0x0F;
            A.ch2.env_increase = (v & 0x08) != 0;
            A.ch2.env_period = v & 7;
            if ((v & 0xF8) == 0) { A.ch2.enabled = false; }
            break;
        case 0xFF18:
            A.ch2.freq = (A.ch2.freq & 0x0700) | v;
            break;
        case 0xFF19:
            A.ch2.length_enable = (v & 0x40) != 0;
            A.ch2.freq = (A.ch2.freq & 0x00FF) | ((v & 7) << 8);
            if (v & 0x80) ch2_trigger();
            break;

        // Wave (stub) FF1A..FF1E: accept writes to look less broken, no output yet
        case 0xFF1A: A.ch3.dac_on = (v & 0x80)!=0; if (!A.ch3.dac_on) A.ch3.enabled=false; break;
        case 0xFF1B: A.ch3.length = 256 - v; break;
        case 0xFF1C: A.ch3.level  = (v >> 5) & 3; break;
        case 0xFF1D: A.ch3.freq   = (A.ch3.freq & 0x0700) | v; break;
        case 0xFF1E:
            A.ch3.length_enable = (v & 0x40)!=0;
            A.ch3.freq = (A.ch3.freq & 0x00FF) | ((v & 7) << 8);
            if (v & 0x80) { // trigger
                A.ch3.enabled = A.ch3.dac_on;
                if (A.ch3.length==0) A.ch3.length = (u8)256;
                A.ch3.pos=0;
                u16 base=2048-(A.ch3.freq&0x7FF); A.ch3.timer = base? (base<<1):2;
            }
            break;
        // Noise
        case 0xFF20: // NR41 length
            A.ch4.length = 64 - (v & 0x3F);
            break;
        case 0xFF21: // NR42 envelope
            A.ch4.env_vol = (v >> 4) & 0x0F;
            A.ch4.env_increase = (v & 0x08) != 0;
            A.ch4.env_period = v & 7;
            if ((v & 0xF8) == 0) { A.ch4.enabled = false; }
            break;
        case 0xFF22: // NR43 polynomial
            A.ch4.clock_shift = (v >> 4) & 0x0F;
            A.ch4.width_mode7 = (v & 0x08) != 0;
            A.ch4.divisor_code = (v & 0x07);
            break;
        case 0xFF23: // NR44 trigger/length enable
            A.ch4.length_enable = (v & 0x40) != 0;
            if (v & 0x80) ch4_trigger();
            break;

        // Mixer
        case 0xFF24: A.nr50 = v; break;
        case 0xFF25: A.nr51 = v; break;

        default: break;
    }
}
