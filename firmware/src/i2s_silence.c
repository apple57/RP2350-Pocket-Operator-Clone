#include "pico/stdlib.h"
#include "pico/audio_i2s.h"

#define SAMPLE_RATE 48000
#define BUFFER_SIZE 128     // stereo frames per buffer
                            // at 48 kHz, 128 frames =  ~2.7ms

int main() {
    // describes audio data format
    audio_format_t audio_format = {
        .sample_freq = SAMPLE_RATE,             // 48000 samples/sec
        .format = AUDIO_BUFFER_FORMAT_PCM_S16,  // signed 16-bit integers
        .channel_count = 2,                     // stereo (left + right)
    };

    // describes how samples are laid out in memory
    audio_buffer_format_t producer_format = {
        .format = &audio_format,    
        .sample_stride = 4,         //each stereo frame takes 4 bytes - 16 bits, L+R
    };

    // tells PIO which pins to use
    audio_i2s_config_t i2s_config = {
        .data_pin = 9,          // DIN -> GP9
        .clock_pin_base = 10,   // BCK -> GP10, LCK -> GP11 (consecutive)
        .dma_channel = 0,
        .pio_sm = 0,
    };
    
    // creates pool of 3 buffers - allowing CPU to fill one while DMA drains the other.
    // this helps us prevent audio dropouts, having 3 buffers gives extra slack.
    audio_buffer_pool_t *producer_pool = audio_new_producer_pool(
        &producer_format, 3, BUFFER_SIZE
    );

    // configures PIO state machine to generate SCL and LCK at the right frequencies
    // for  48 kHz 16-bit stereo, and wires DMA to stream data from the buffer pool 
    // automatically.
    audio_i2s_setup(&audio_format, &i2s_config);
    audio_i2s_connect(producer_pool);
    audio_i2s_set_enabled(true);

    // set LED pins for "heartbeat".
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    uint32_t loop_count = 0;

    while (1) {
        // blocks until a buffer is free to write into.
        audio_buffer_t *buffer = take_audio_buffer(producer_pool, true);

        // raw pointer into buffer's memory, casting to int16_t to write
        // 16-bit samples directly.
        int16_t *samples = (int16_t *)buffer->buffer->bytes;
        
        // Fill every sample with zero = silence.
        // max_sample_count * 2 since stereo -> 2 frames.
        for (uint i = 0; i < buffer->max_sample_count * 2; i++) {
            samples[i] = 0;
        }

        // Tells pool this buffer is ready to be sent to the DMA.
        // After this call, DMA owns 'buffer' and 'samples'.
        buffer->sample_count = buffer->max_sample_count;
        give_audio_buffer(producer_pool, buffer);

        // blink every 500 buffers to ensure main loop isn't hung up.
        if (++loop_count % 500 == 0) {
            gpio_put(PICO_DEFAULT_LED_PIN, !gpio_get(PICO_DEFAULT_LED_PIN));
        }
    }
} 

