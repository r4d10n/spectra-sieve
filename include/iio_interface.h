#include <string>
#include <iio.h>

class iio_interface {
public:
    iio_interface();
    ~iio_interface();

    int init_device(const std::string& uri, double rate, double freq, double gain, double bw);
    void deinit_device();
    int refill_buffer();
    ssize_t get_buffer_step();
    void* get_buffer_end();
    void* get_first_channel();
    void set_frequency(long long freq);
    void set_gain(long long gain);
    void set_sample_rate(long long rate);
    void set_bandwidth(long long bw);

private:
    struct iio_context *ctx;
    struct iio_device *phy;
    struct iio_device *dev;
    struct iio_channel *rx0_i, *rx0_q, *rxch;
    struct iio_buffer *rxbuf;
    size_t num_bins;

    struct iio_context *scan();
};
